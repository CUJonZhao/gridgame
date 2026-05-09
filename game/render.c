/*
 * render.c -- HPS-side translator from game_state_t to FPGA tile RAM
 * + 30 sprite registers. This is the missing link main.c flagged as
 * "Rendering is intentionally NOT done here yet".
 *
 * Conventions
 *   - Tile (row, col) maps to pixel (col * TILE_SIZE, row * TILE_SIZE).
 *   - The sprite registers' (x, y) is the upper-left pixel; players
 *     already track their position in pixel space, so they're written
 *     through verbatim.
 *   - The sprite scan compositor in hardware/rtl/sprite_scan_renderer.sv
 *     gives higher-index registers higher priority on overlap, so we
 *     intentionally place explosions and the win banner in higher
 *     slots than players and bombs.
 */

#include "render.h"
#include "video_interface.h"

#include <stddef.h>
#include <stdint.h>

static uint8_t player_sprite_id(uint8_t player_index, direction_t dir)
{
    uint8_t base = (player_index == 0)
        ? (uint8_t)SPRITE_PLAYER1_FRONT
        : (uint8_t)SPRITE_PLAYER2_FRONT;
    uint8_t offset;

    switch (dir) {
        case DIR_UP:    offset = 1; break;  /* BACK  */
        case DIR_DOWN:  offset = 0; break;  /* FRONT */
        case DIR_LEFT:  offset = 2; break;
        case DIR_RIGHT: offset = 3; break;
        case DIR_NONE:
        default:        offset = 0; break;
    }
    return (uint8_t)(base + offset);
}

static uint8_t explosion_sprite_id(uint8_t owner, unsigned cell_index)
{
    /* Cell index follows fill_explosion_cells() in game_logic.c:
     *   0 = center, 1 = up, 2 = down, 3 = left, 4 = right. */
    if (owner == 0) {
        switch (cell_index) {
            case 0:  return (uint8_t)SPRITE_PLAYER1_EXPLOSION_CENTER;
            case 1:  return (uint8_t)SPRITE_PLAYER1_EXPLOSION_UP;
            case 2:  return (uint8_t)SPRITE_PLAYER1_EXPLOSION_DOWN;
            case 3:  return (uint8_t)SPRITE_PLAYER1_EXPLOSION_LEFT;
            default: return (uint8_t)SPRITE_PLAYER1_EXPLOSION_RIGHT;
        }
    }
    switch (cell_index) {
        case 0:  return (uint8_t)SPRITE_PLAYER2_EXPLOSION_CENTER;
        case 1:  return (uint8_t)SPRITE_PLAYER2_EXPLOSION_UP;
        case 2:  return (uint8_t)SPRITE_PLAYER2_EXPLOSION_DOWN;
        case 3:  return (uint8_t)SPRITE_PLAYER2_EXPLOSION_LEFT;
        default: return (uint8_t)SPRITE_PLAYER2_EXPLOSION_RIGHT;
    }
}

void render_init(void)
{
    unsigned r, c;

    for (r = 0; r < MAP_ROWS; ++r) {
        for (c = 0; c < MAP_COLS; ++c) {
            video_set_tile(r, c, (uint8_t)TILE_EMPTY);
        }
    }
    video_clear_sprites();
    /* Don't flush here -- main.c may call this before
     * video_interface_init() succeeds. The first real render_frame()
     * will push the staged contents. */
}

static unsigned stage_players(const game_state_t *game, unsigned slot)
{
    unsigned p;

    for (p = 0; p < PLAYER_COUNT; ++p) {
        const player_t *pl = &game->players[p];
        if (slot >= VIDEO_SPRITE_COUNT) {
            break;
        }
        if (!pl->alive) {
            continue;
        }
        video_set_sprite(slot,
                         pl->x,
                         pl->y,
                         player_sprite_id((uint8_t)p, pl->dir),
                         1);
        ++slot;
    }
    return slot;
}

static unsigned stage_bombs(const game_state_t *game, unsigned slot)
{
    unsigned b;

    for (b = 0; b < ACTIVE_BOMB_COUNT; ++b) {
        const bomb_t *bomb = &game->bombs[b];
        uint16_t px;
        uint16_t py;
        uint8_t  sid;

        if (slot >= VIDEO_SPRITE_COUNT) {
            break;
        }
        if (!bomb->active) {
            continue;
        }
        px  = (uint16_t)(bomb->col * TILE_SIZE);
        py  = (uint16_t)(bomb->row * TILE_SIZE);
        sid = (bomb->owner == 0)
            ? (uint8_t)SPRITE_PLAYER1_BOMB
            : (uint8_t)SPRITE_PLAYER2_BOMB;
        video_set_sprite(slot, px, py, sid, 1);
        ++slot;
    }
    return slot;
}

static unsigned stage_explosions(const game_state_t *game, unsigned slot)
{
    unsigned e;
    unsigned i;

    for (e = 0; e < ACTIVE_EXPLOSION_COUNT; ++e) {
        const explosion_t *ex = &game->explosions[e];
        if (!ex->active) {
            continue;
        }
        for (i = 0; i < EXPLOSION_TILE_COUNT; ++i) {
            uint8_t  er;
            uint8_t  ec;
            uint16_t px;
            uint16_t py;
            uint8_t  sid;

            if (slot >= VIDEO_SPRITE_COUNT) {
                return slot;
            }
            er = ex->cells[i].row;
            ec = ex->cells[i].col;
            if (er >= MAP_ROWS || ec >= MAP_COLS) {
                continue;
            }
            px  = (uint16_t)(ec * TILE_SIZE);
            py  = (uint16_t)(er * TILE_SIZE);
            sid = explosion_sprite_id(ex->owner, i);
            video_set_sprite(slot, px, py, sid, 1);
            ++slot;
        }
    }
    return slot;
}

static unsigned stage_win_banner(const game_state_t *game, unsigned slot)
{
    /* Three-letter "WIN" banner + the winner's marker. The banner sits
     * on the top score row defined in game.h. Yellow letters belong to
     * P1 (winner == 0), pink letters to P2 (winner == 1). */
    uint8_t base_letter;
    uint8_t marker;
    unsigned i;
    unsigned ui_y;
    unsigned start_col;

    if (!game->game_over) {
        return slot;
    }
    if (game->winner != 0 && game->winner != 1) {
        return slot;
    }

    base_letter = (game->winner == 0)
        ? (uint8_t)SPRITE_UI_YELLOW_W
        : (uint8_t)SPRITE_UI_PINK_W;
    marker = (game->winner == 0)
        ? (uint8_t)SPRITE_UI_PLAYER1
        : (uint8_t)SPRITE_UI_PLAYER2;

    ui_y      = UI_ROW_START * TILE_SIZE;
    start_col = UI_COL_START;

    for (i = 0; i < 3; ++i) {
        if (slot >= VIDEO_SPRITE_COUNT) {
            return slot;
        }
        video_set_sprite(slot,
                         (uint16_t)((start_col + i) * TILE_SIZE),
                         (uint16_t)ui_y,
                         (uint8_t)(base_letter + i),
                         1);
        ++slot;
    }
    if (slot < VIDEO_SPRITE_COUNT) {
        video_set_sprite(slot,
                         (uint16_t)((start_col + 4) * TILE_SIZE),
                         (uint16_t)ui_y,
                         marker,
                         1);
        ++slot;
    }
    return slot;
}

void render_frame(const game_state_t *game)
{
    unsigned r;
    unsigned c;
    unsigned slot;

    if (game == NULL) {
        return;
    }

    /* 1. Tiles -- a full 300-byte refresh is cheap and avoids any
     * dirty-tracking bookkeeping when game_logic mutates the map. */
    for (r = 0; r < MAP_ROWS; ++r) {
        for (c = 0; c < MAP_COLS; ++c) {
            video_set_tile(r, c, (uint8_t)game->tiles[r][c]);
        }
    }

    /* 2. Sprite registers -- rebuild from scratch every frame so that
     * disabled actors immediately drop off-screen. */
    video_clear_sprites();
    slot = 0;
    slot = stage_players(game, slot);
    slot = stage_bombs(game, slot);
    slot = stage_explosions(game, slot);
    slot = stage_win_banner(game, slot);
    (void)slot; /* leftover slots remain disabled */

    /* 3. Push to the FPGA. video_flush() is a no-op when init was
     * skipped (e.g. unit tests), so the staging buffers stay valid
     * for read-back. */
    (void)video_flush();
}
