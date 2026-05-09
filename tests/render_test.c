/*
 * render_test.c -- exercises render_frame() in three flavours:
 *
 *   1. Pure staging-buffer test: no device file, just check what the
 *      renderer staged via the test hooks. This covers tile bytes,
 *      player sprite IDs by direction, bomb/explosion mapping, and the
 *      win banner.
 *
 *   2. Loopback-write test: redirect FPGA_VIDEO_DEV to a temp file and
 *      verify the byte layout (300 tile bytes at offset 0x000, 30
 *      32-bit sprite words at offset 0x200) matches what the kernel
 *      driver expects to receive.
 *
 *   3. Sprite-packing round-trip: spot-check video_pack_sprite() and
 *      the on-wire bit layout against the design-doc spec.
 */

#include "game.h"
#include "render.h"
#include "video_interface.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int passes  = 0;
static int failures = 0;

#define CHECK(cond) do {                                                  \
    if (cond) {                                                           \
        passes++;                                                         \
    } else {                                                              \
        failures++;                                                       \
        fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                     \
} while (0)

#define CHECK_EQ_U(a, b) do {                                             \
    unsigned _a = (unsigned)(a);                                          \
    unsigned _b = (unsigned)(b);                                          \
    if (_a == _b) {                                                       \
        passes++;                                                         \
    } else {                                                              \
        failures++;                                                       \
        fprintf(stderr,                                                   \
                "  FAIL %s:%d: %s (=%u) != %s (=%u)\n",                   \
                __FILE__, __LINE__, #a, _a, #b, _b);                      \
    }                                                                     \
} while (0)

/* Helpers to decode sprite registers using the same masks as the
 * shipped header. */
static unsigned spr_x(uint32_t w)      { return w & VIDEO_SPRITE_X_MASK; }
static unsigned spr_y(uint32_t w)      { return (w & VIDEO_SPRITE_Y_MASK) >> VIDEO_SPRITE_Y_SHIFT; }
static unsigned spr_id(uint32_t w)     { return (w & VIDEO_SPRITE_ID_MASK) >> VIDEO_SPRITE_ID_SHIFT; }
static unsigned spr_enable(uint32_t w) { return (w & VIDEO_SPRITE_ENABLE_BIT) ? 1u : 0u; }

static void set_bomb(game_state_t *g, unsigned i, uint8_t owner,
                     uint8_t row, uint8_t col)
{
    g->bombs[i].active = 1;
    g->bombs[i].owner  = owner;
    g->bombs[i].row    = row;
    g->bombs[i].col    = col;
    g->bombs[i].timer  = BOMB_TIMER_TICKS;
}

static void set_explosion(game_state_t *g, unsigned i, uint8_t owner,
                          uint8_t center_row, uint8_t center_col)
{
    explosion_t *e = &g->explosions[i];
    e->active = 1;
    e->owner  = owner;
    e->center_row = center_row;
    e->center_col = center_col;
    e->timer  = EXPLOSION_TIMER_TICKS;

    /* Same order as game_logic.c::fill_explosion_cells. */
    e->cells[0].row = center_row;
    e->cells[0].col = center_col;
    e->cells[1].row = (uint8_t)(center_row - 1);
    e->cells[1].col = center_col;
    e->cells[2].row = (uint8_t)(center_row + 1);
    e->cells[2].col = center_col;
    e->cells[3].row = center_row;
    e->cells[3].col = (uint8_t)(center_col - 1);
    e->cells[4].row = center_row;
    e->cells[4].col = (uint8_t)(center_col + 1);
}

static void zero_state(game_state_t *g)
{
    memset(g, 0, sizeof(*g));
    g->winner = -1;
}

/* -------------------------------------------------------------------- */

static void test_initial_state_tiles_match_game(void)
{
    game_state_t g;
    const uint8_t *tiles;
    unsigned r, c;

    fprintf(stderr, "test_initial_state_tiles_match_game\n");
    zero_state(&g);
    /* Fabricate a non-trivial pattern so we can be sure rendering
     * actually walked the array. */
    for (r = 0; r < MAP_ROWS; ++r) {
        for (c = 0; c < MAP_COLS; ++c) {
            g.tiles[r][c] = (tile_id_t)((r + c) % 6);
        }
    }
    g.players[0].alive = 0;
    g.players[1].alive = 0;

    render_frame(&g);
    tiles = video_test_get_tiles();

    for (r = 0; r < MAP_ROWS; ++r) {
        for (c = 0; c < MAP_COLS; ++c) {
            CHECK_EQ_U(tiles[r * MAP_COLS + c], (r + c) % 6);
        }
    }
}

static void test_player_sprite_id_by_direction(void)
{
    /* For each direction the renderer should pick the matching
     * sprite id from the SPRITE_PLAYER1_* / SPRITE_PLAYER2_* enums. */
    static const struct {
        direction_t dir;
        uint8_t     p1_expected;
        uint8_t     p2_expected;
    } cases[] = {
        { DIR_UP,    SPRITE_PLAYER1_BACK,  SPRITE_PLAYER2_BACK  },
        { DIR_DOWN,  SPRITE_PLAYER1_FRONT, SPRITE_PLAYER2_FRONT },
        { DIR_LEFT,  SPRITE_PLAYER1_LEFT,  SPRITE_PLAYER2_LEFT  },
        { DIR_RIGHT, SPRITE_PLAYER1_RIGHT, SPRITE_PLAYER2_RIGHT },
        { DIR_NONE,  SPRITE_PLAYER1_FRONT, SPRITE_PLAYER2_FRONT },
    };
    unsigned k;

    fprintf(stderr, "test_player_sprite_id_by_direction\n");
    for (k = 0; k < sizeof(cases) / sizeof(cases[0]); ++k) {
        game_state_t g;
        const uint32_t *s;

        zero_state(&g);
        g.players[0].alive = 1;
        g.players[0].x = 32;
        g.players[0].y = 32;
        g.players[0].dir = cases[k].dir;

        g.players[1].alive = 1;
        g.players[1].x = 576;
        g.players[1].y = 32;
        g.players[1].dir = cases[k].dir;

        render_frame(&g);
        s = video_test_get_sprites();

        CHECK(spr_enable(s[0]) == 1);
        CHECK_EQ_U(spr_id(s[0]),  cases[k].p1_expected);
        CHECK_EQ_U(spr_x(s[0]),   32);
        CHECK_EQ_U(spr_y(s[0]),   32);

        CHECK(spr_enable(s[1]) == 1);
        CHECK_EQ_U(spr_id(s[1]),  cases[k].p2_expected);
        CHECK_EQ_U(spr_x(s[1]),   576);
        CHECK_EQ_U(spr_y(s[1]),   32);
    }
}

static void test_dead_player_skipped(void)
{
    game_state_t g;
    const uint32_t *s;

    fprintf(stderr, "test_dead_player_skipped\n");
    zero_state(&g);
    g.players[0].alive = 0;
    g.players[1].alive = 1;
    g.players[1].x = 100;
    g.players[1].y = 200;
    g.players[1].dir = DIR_RIGHT;

    render_frame(&g);
    s = video_test_get_sprites();

    /* P2 collapses into slot 0 because P1 was skipped. */
    CHECK(spr_enable(s[0]) == 1);
    CHECK_EQ_U(spr_id(s[0]), SPRITE_PLAYER2_RIGHT);
    CHECK_EQ_U(spr_x(s[0]),  100);
    CHECK_EQ_U(spr_y(s[0]),  200);
    CHECK(spr_enable(s[1]) == 0);
}

static void test_bombs_after_players(void)
{
    game_state_t g;
    const uint32_t *s;

    fprintf(stderr, "test_bombs_after_players\n");
    zero_state(&g);
    g.players[0].alive = 1;
    g.players[0].x = 32; g.players[0].y = 32;  g.players[0].dir = DIR_DOWN;
    g.players[1].alive = 1;
    g.players[1].x = 576; g.players[1].y = 32; g.players[1].dir = DIR_DOWN;

    set_bomb(&g, 0, /*owner=*/0, /*row=*/3, /*col=*/4);
    set_bomb(&g, 1, /*owner=*/1, /*row=*/5, /*col=*/6);

    render_frame(&g);
    s = video_test_get_sprites();

    /* slots 0..1 are players, 2..3 are bombs */
    CHECK(spr_enable(s[2]) == 1);
    CHECK_EQ_U(spr_id(s[2]),  SPRITE_PLAYER1_BOMB);
    CHECK_EQ_U(spr_x(s[2]),   4 * TILE_SIZE);
    CHECK_EQ_U(spr_y(s[2]),   3 * TILE_SIZE);

    CHECK(spr_enable(s[3]) == 1);
    CHECK_EQ_U(spr_id(s[3]),  SPRITE_PLAYER2_BOMB);
    CHECK_EQ_U(spr_x(s[3]),   6 * TILE_SIZE);
    CHECK_EQ_U(spr_y(s[3]),   5 * TILE_SIZE);
}

static void test_explosion_cells_to_directional_sprites(void)
{
    game_state_t g;
    const uint32_t *s;

    fprintf(stderr, "test_explosion_cells_to_directional_sprites\n");
    zero_state(&g);
    g.players[0].alive = 0;
    g.players[1].alive = 0;
    set_explosion(&g, 0, /*owner=*/0, /*row=*/5, /*col=*/7);

    render_frame(&g);
    s = video_test_get_sprites();

    /* slot 0 is the first explosion's center, then up/down/left/right */
    CHECK(spr_enable(s[0]) == 1);
    CHECK_EQ_U(spr_id(s[0]), SPRITE_PLAYER1_EXPLOSION_CENTER);
    CHECK_EQ_U(spr_x(s[0]),  7 * TILE_SIZE);
    CHECK_EQ_U(spr_y(s[0]),  5 * TILE_SIZE);

    CHECK_EQ_U(spr_id(s[1]), SPRITE_PLAYER1_EXPLOSION_UP);
    CHECK_EQ_U(spr_y(s[1]),  4 * TILE_SIZE);

    CHECK_EQ_U(spr_id(s[2]), SPRITE_PLAYER1_EXPLOSION_DOWN);
    CHECK_EQ_U(spr_y(s[2]),  6 * TILE_SIZE);

    CHECK_EQ_U(spr_id(s[3]), SPRITE_PLAYER1_EXPLOSION_LEFT);
    CHECK_EQ_U(spr_x(s[3]),  6 * TILE_SIZE);

    CHECK_EQ_U(spr_id(s[4]), SPRITE_PLAYER1_EXPLOSION_RIGHT);
    CHECK_EQ_U(spr_x(s[4]),  8 * TILE_SIZE);

    /* Now P2 explosion -- different sprite IDs. */
    zero_state(&g);
    set_explosion(&g, 0, /*owner=*/1, /*row=*/5, /*col=*/7);
    render_frame(&g);
    s = video_test_get_sprites();
    CHECK_EQ_U(spr_id(s[0]), SPRITE_PLAYER2_EXPLOSION_CENTER);
    CHECK_EQ_U(spr_id(s[1]), SPRITE_PLAYER2_EXPLOSION_UP);
    CHECK_EQ_U(spr_id(s[2]), SPRITE_PLAYER2_EXPLOSION_DOWN);
    CHECK_EQ_U(spr_id(s[3]), SPRITE_PLAYER2_EXPLOSION_LEFT);
    CHECK_EQ_U(spr_id(s[4]), SPRITE_PLAYER2_EXPLOSION_RIGHT);
}

static void test_win_banner(void)
{
    game_state_t g;
    const uint32_t *s;
    unsigned i;

    fprintf(stderr, "test_win_banner\n");
    zero_state(&g);
    g.players[0].alive = 0;
    g.players[1].alive = 0;
    g.game_over = 1;
    g.winner    = 0;  /* yellow wins */

    render_frame(&g);
    s = video_test_get_sprites();

    /* Three letters W I N + the player marker, all in the UI row. */
    for (i = 0; i < 3; ++i) {
        CHECK(spr_enable(s[i]) == 1);
        CHECK_EQ_U(spr_id(s[i]), SPRITE_UI_YELLOW_W + i);
        CHECK_EQ_U(spr_y(s[i]),  UI_ROW_START * TILE_SIZE);
        CHECK_EQ_U(spr_x(s[i]),  (UI_COL_START + i) * TILE_SIZE);
    }
    CHECK(spr_enable(s[3]) == 1);
    CHECK_EQ_U(spr_id(s[3]), SPRITE_UI_PLAYER1);

    /* P2 winning -> pink letters and P2 marker. */
    zero_state(&g);
    g.game_over = 1;
    g.winner    = 1;
    render_frame(&g);
    s = video_test_get_sprites();
    CHECK_EQ_U(spr_id(s[0]), SPRITE_UI_PINK_W);
    CHECK_EQ_U(spr_id(s[3]), SPRITE_UI_PLAYER2);
}

/* -------------------------------------------------------------------- */

static void test_loopback_writes_match_layout(void)
{
    /* This test uses the same FPGA_VIDEO_DEV trick the audio test
     * does: redirect to a regular file so we can pread() the bytes
     * back and verify the on-wire layout matches what the kernel
     * driver expects (tile bytes at 0x000, sprite words at 0x200). */
    const char *fake_path = "/tmp/fake_fpga_video";
    game_state_t g;
    uint8_t  tile_buf[VIDEO_TILE_BYTES];
    uint32_t sprite_buf[VIDEO_SPRITE_COUNT];
    int fd;
    unsigned r, c;

    fprintf(stderr, "test_loopback_writes_match_layout\n");
    unlink(fake_path);
    setenv("FPGA_VIDEO_DEV", fake_path, 1);

    zero_state(&g);
    for (r = 0; r < MAP_ROWS; ++r) {
        for (c = 0; c < MAP_COLS; ++c) {
            g.tiles[r][c] = (tile_id_t)((c & 0x07));
        }
    }
    g.players[0].alive = 1;
    g.players[0].x = 32; g.players[0].y = 32; g.players[0].dir = DIR_DOWN;
    g.players[1].alive = 1;
    g.players[1].x = 576; g.players[1].y = 32; g.players[1].dir = DIR_LEFT;

    if (video_interface_init() != 0) {
        fprintf(stderr, "  unable to open fake video device\n");
        failures++;
        return;
    }
    render_frame(&g);
    video_interface_close();

    fd = open(fake_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "  open(%s) failed: %s\n", fake_path, strerror(errno));
        failures++;
        unsetenv("FPGA_VIDEO_DEV");
        return;
    }

    if (pread(fd, tile_buf, sizeof(tile_buf),
              (off_t)VIDEO_TILE_OFFSET) != (ssize_t)sizeof(tile_buf)) {
        fprintf(stderr, "  pread(tile) short read\n");
        failures++;
    }
    if (pread(fd, sprite_buf, sizeof(sprite_buf),
              (off_t)VIDEO_SPRITE_OFFSET) != (ssize_t)sizeof(sprite_buf)) {
        fprintf(stderr, "  pread(sprite) short read\n");
        failures++;
    }
    close(fd);
    unsetenv("FPGA_VIDEO_DEV");

    /* Tiles match the source pattern (after 3-bit masking). */
    for (r = 0; r < MAP_ROWS; ++r) {
        for (c = 0; c < MAP_COLS; ++c) {
            CHECK_EQ_U(tile_buf[r * MAP_COLS + c], (c & 0x07));
        }
    }
    /* Sprite 0/1 enabled, the rest disabled. */
    CHECK(spr_enable(sprite_buf[0]) == 1);
    CHECK_EQ_U(spr_id(sprite_buf[0]), SPRITE_PLAYER1_FRONT);
    CHECK(spr_enable(sprite_buf[1]) == 1);
    CHECK_EQ_U(spr_id(sprite_buf[1]), SPRITE_PLAYER2_LEFT);
    {
        unsigned i;
        for (i = 2; i < VIDEO_SPRITE_COUNT; ++i) {
            CHECK(spr_enable(sprite_buf[i]) == 0);
        }
    }
}

static void test_sprite_packing_round_trip(void)
{
    uint32_t w;
    fprintf(stderr, "test_sprite_packing_round_trip\n");

    /* Spot-check field widths: x=10b, y=9b, id=5b, en=1b. */
    w = video_pack_sprite(0x3FF, 0x1FF, 0x1F, 1);
    CHECK_EQ_U(spr_x(w),       0x3FF);
    CHECK_EQ_U(spr_y(w),       0x1FF);
    CHECK_EQ_U(spr_id(w),      0x1F);
    CHECK_EQ_U(spr_enable(w),  1);

    /* Out-of-range fields get masked, not propagated. */
    w = video_pack_sprite(0xFFFF, 0xFFFF, 0xFF, 0);
    CHECK_EQ_U(spr_x(w),       0x3FF);
    CHECK_EQ_U(spr_y(w),       0x1FF);
    CHECK_EQ_U(spr_id(w),      0x1F);
    CHECK_EQ_U(spr_enable(w),  0);
    /* And the reserved bits stay zero. */
    CHECK((w & 0xFE000000u) == 0u);
}

int main(void)
{
    test_initial_state_tiles_match_game();
    test_player_sprite_id_by_direction();
    test_dead_player_skipped();
    test_bombs_after_players();
    test_explosion_cells_to_directional_sprites();
    test_win_banner();
    test_sprite_packing_round_trip();
    test_loopback_writes_match_layout();

    fprintf(stderr, "\nrender_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
