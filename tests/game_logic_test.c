#include <stdio.h>
#include <string.h>

#include "game.h"

static int passes = 0;
static int failures = 0;

#define CHECK(cond) do {                                                  \
    if (cond) {                                                           \
        passes++;                                                         \
    } else {                                                              \
        failures++;                                                       \
        fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                     \
} while (0)

static const controller_state_t NONE = {0, 0, 0, 0, 0, 0};

static controller_state_t ctrl_with(int up, int down, int left, int right,
                                    int bomb, int restart)
{
    controller_state_t c = {0, 0, 0, 0, 0, 0};
    c.up      = (uint8_t)up;
    c.down    = (uint8_t)down;
    c.left    = (uint8_t)left;
    c.right   = (uint8_t)right;
    c.bomb    = (uint8_t)bomb;
    c.restart = (uint8_t)restart;
    return c;
}

static void step_once(game_state_t *g,
                      controller_state_t p1,
                      controller_state_t p2)
{
    controller_state_t in[PLAYER_COUNT] = {p1, p2};
    game_step(g, in);
}

static void move_one_tile(game_state_t *g,
                          controller_state_t p1,
                          controller_state_t p2)
{
    int max_iter = TILE_SIZE / PLAYER_SPEED + 4;
    int i;

    step_once(g, p1, p2);
    for (i = 0; i < max_iter; ++i) {
        if (!g->players[0].moving && !g->players[1].moving) {
            return;
        }
        step_once(g, NONE, NONE);
    }
}

static void warp_player(game_state_t *g, uint8_t idx, uint8_t row, uint8_t col)
{
    player_t *p = &g->players[idx];
    p->row        = row;
    p->col        = col;
    p->x          = (uint16_t)(col * TILE_SIZE);
    p->y          = (uint16_t)(row * TILE_SIZE);
    p->target_row = row;
    p->target_col = col;
    p->target_x   = p->x;
    p->target_y   = p->y;
    p->moving     = 0;
}

/* ---------------- tests ---------------- */

static void test_init(void)
{
    printf("test_init\n");
    game_state_t g;
    game_init(&g);

    CHECK(g.players[0].alive  == 1);
    CHECK(g.players[1].alive  == 1);
    CHECK(g.players[0].score  == 0);
    CHECK(g.players[1].score  == 0);
    CHECK(g.game_over         == 0);
    CHECK(g.winner            == -1);
    CHECK(g.pending_sound_1   == SOUND_NONE);
    CHECK(g.pending_sound_2   == SOUND_NONE);

    CHECK(g.tiles[0][0]                       == TILE_HARD_WALL);
    CHECK(g.tiles[MAP_ROWS - 1][MAP_COLS - 1] == TILE_HARD_WALL);

    CHECK(g.tiles[1][6]  == TILE_EMPTY);
    CHECK(g.tiles[1][13] == TILE_EMPTY);

    CHECK(g.players[0].row == 1);
    CHECK(g.players[0].col == 1);
    CHECK(g.players[1].row == 1);
    CHECK(g.players[1].col == 18);
}

static void test_move_each_direction(void)
{
    printf("test_move_each_direction\n");
    game_state_t g;
    game_init(&g);

    warp_player(&g, 1, 13, 18);
    warp_player(&g, 0, 5, 5);

    /* Clear the cardinal neighbours of (5,5) so the four moves below all
     * have a walkable BLUE tile to step onto regardless of the static map. */
    g.tiles[5][5] = TILE_BLUE;
    g.tiles[4][5] = TILE_BLUE;
    g.tiles[6][5] = TILE_BLUE;
    g.tiles[5][4] = TILE_BLUE;
    g.tiles[5][6] = TILE_BLUE;

    move_one_tile(&g, ctrl_with(0, 1, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 6 && g.players[0].col == 5);
    CHECK(g.players[0].dir == DIR_DOWN);

    move_one_tile(&g, ctrl_with(1, 0, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 5 && g.players[0].col == 5);
    CHECK(g.players[0].dir == DIR_UP);

    move_one_tile(&g, ctrl_with(0, 0, 0, 1, 0, 0), NONE);
    CHECK(g.players[0].row == 5 && g.players[0].col == 6);
    CHECK(g.players[0].dir == DIR_RIGHT);

    move_one_tile(&g, ctrl_with(0, 0, 1, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 5 && g.players[0].col == 5);
    CHECK(g.players[0].dir == DIR_LEFT);
}

static void test_boundary_and_wall(void)
{
    printf("test_boundary_and_wall\n");
    game_state_t g;
    game_init(&g);

    move_one_tile(&g, ctrl_with(1, 0, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);

    move_one_tile(&g, ctrl_with(0, 0, 1, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);

    g.tiles[1][2] = TILE_SOFT_WALL;
    move_one_tile(&g, ctrl_with(0, 0, 0, 1, 0, 0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);
}

static void test_capture_blue_tile(void)
{
    printf("test_capture_blue_tile\n");
    game_state_t g;
    game_init(&g);

    CHECK(g.tiles[2][1] == TILE_BLUE);

    move_one_tile(&g, ctrl_with(0, 1, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].row    == 2);
    CHECK(g.players[0].col    == 1);
    CHECK(g.tiles[2][1]       == TILE_YELLOW);
    CHECK(g.players[0].score  == 1);
}

static void test_capture_opponent_tile(void)
{
    printf("test_capture_opponent_tile\n");
    game_state_t g;
    game_init(&g);

    g.tiles[2][1]      = TILE_PINK;
    g.players[1].score = 3;

    move_one_tile(&g, ctrl_with(0, 1, 0, 0, 0, 0), NONE);
    CHECK(g.tiles[2][1]      == TILE_YELLOW);
    CHECK(g.players[0].score == 1);
    CHECK(g.players[1].score == 2);
}

static void test_capture_own_tile_is_noop(void)
{
    printf("test_capture_own_tile_is_noop\n");
    game_state_t g;
    game_init(&g);

    g.tiles[2][1]        = TILE_YELLOW;
    g.players[0].score   = 5;

    move_one_tile(&g, ctrl_with(0, 1, 0, 0, 0, 0), NONE);
    CHECK(g.tiles[2][1]      == TILE_YELLOW);
    CHECK(g.players[0].score == 5);
    CHECK(g.players[0].row   == 2);
    CHECK(g.players[0].col   == 1);
}

static void test_cannot_walk_onto_other_player(void)
{
    printf("test_cannot_walk_onto_other_player\n");
    game_state_t g;
    game_init(&g);

    warp_player(&g, 1, 2, 1);

    move_one_tile(&g, ctrl_with(0, 1, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);
}

static void test_cannot_walk_onto_bomb(void)
{
    printf("test_cannot_walk_onto_bomb\n");
    game_state_t g;
    game_init(&g);

    g.bombs[0].active = 1;
    g.bombs[0].owner  = 0;
    g.bombs[0].row    = 2;
    g.bombs[0].col    = 1;
    g.bombs[0].timer  = BOMB_TIMER_TICKS;

    move_one_tile(&g, ctrl_with(0, 1, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);
}

static void test_bomb_places_and_blocks_walking_back(void)
{
    printf("test_bomb_places_and_blocks_walking_back\n");
    game_state_t g;
    game_init(&g);

    step_once(&g, ctrl_with(0, 0, 0, 0, 1, 0), NONE);
    CHECK(g.bombs[0].active == 1);
    CHECK(g.bombs[0].row    == 1);
    CHECK(g.bombs[0].col    == 1);
    CHECK(g.bombs[0].owner  == 0);

    move_one_tile(&g, ctrl_with(0, 1, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 2 && g.players[0].col == 1);

    move_one_tile(&g, ctrl_with(1, 0, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].row == 2 && g.players[0].col == 1);
}

static void test_bomb_detonates_and_emits_explosion_sound(void)
{
    printf("test_bomb_detonates_and_emits_explosion_sound\n");
    game_state_t g;
    uint32_t i;
    sound_id_t got_sound = SOUND_NONE;

    game_init(&g);

    warp_player(&g, 0, 5, 5);
    g.players[1].alive = 0;

    step_once(&g, ctrl_with(0, 0, 0, 0, 1, 0), NONE);
    CHECK(g.bombs[0].active == 1);

    for (i = 0; i < (uint32_t)BOMB_TIMER_TICKS + 4; ++i) {
        step_once(&g, NONE, NONE);
        if (g.pending_sound_1 == SOUND_EXPLOSION) {
            got_sound = SOUND_EXPLOSION;
            break;
        }
    }

    CHECK(got_sound == SOUND_EXPLOSION);
    CHECK(g.bombs[0].active      == 0);
    CHECK(g.explosions[0].active == 1);
}

static void test_win_condition(void)
{
    printf("test_win_condition\n");
    game_state_t g;
    game_init(&g);
    g.players[0].score = WIN_SCORE - 1;

    move_one_tile(&g, ctrl_with(0, 1, 0, 0, 0, 0), NONE);
    CHECK(g.players[0].score >= WIN_SCORE);
    CHECK(g.game_over        == 1);
    CHECK(g.winner           == 0);
    CHECK(g.pending_sound_1  == SOUND_VICTORY);
}

static void test_restart_after_game_over(void)
{
    printf("test_restart_after_game_over\n");
    game_state_t g;
    game_init(&g);
    g.game_over        = 1;
    g.winner           = 0;
    g.players[0].score = 42;
    g.players[0].row   = 7;

    step_once(&g, ctrl_with(0, 0, 0, 0, 0, 1), NONE);
    CHECK(g.game_over        == 0);
    CHECK(g.winner           == -1);
    CHECK(g.players[0].score == 0);
    CHECK(g.players[0].row   == 1);
    CHECK(g.players[0].col   == 1);
}

static void test_dead_player_does_not_move(void)
{
    printf("test_dead_player_does_not_move\n");
    game_state_t g;
    game_init(&g);
    g.players[0].alive = 0;

    move_one_tile(&g, ctrl_with(0, 0, 0, 1, 0, 0), NONE);
    CHECK(g.players[0].row   == 1);
    CHECK(g.players[0].col   == 1);
    CHECK(g.players[0].alive == 0);
}

int main(void)
{
    test_init();
    test_move_each_direction();
    test_boundary_and_wall();
    test_capture_blue_tile();
    test_capture_opponent_tile();
    test_capture_own_tile_is_noop();
    test_cannot_walk_onto_other_player();
    test_cannot_walk_onto_bomb();
    test_bomb_places_and_blocks_walking_back();
    test_bomb_detonates_and_emits_explosion_sound();
    test_win_condition();
    test_restart_after_game_over();
    test_dead_player_does_not_move();

    printf("\n---- %d passed, %d failed ----\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
