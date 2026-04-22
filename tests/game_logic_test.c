#include <stdio.h>
#include <string.h>

#include "game.h"

extern uint32_t g_move_cooldown_frames;

static int passes   = 0;
static int failures = 0;

#define CHECK(cond) do {                                                \
    if (cond) { passes++; }                                             \
    else {                                                              \
        failures++;                                                     \
        fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                   \
} while (0)

static const controller_state_t NONE = {0};

static controller_state_t ctrl_with(int up, int down, int left, int right,
                                    int bomb, int restart)
{
    controller_state_t c = {0};
    c.up = up; c.down = down; c.left = left; c.right = right;
    c.bomb = bomb; c.restart = restart;
    return c;
}

static void step_once(game_state_t *g,
                      controller_state_t p1,
                      controller_state_t p2)
{
    controller_state_t in[PLAYER_COUNT] = { p1, p2 };
    game_step(g, in);
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
    CHECK(g.pending_sound     == SOUND_NONE);
    CHECK(g.tiles[0][0]       == TILE_HARD_WALL);
    CHECK(g.tiles[MAP_ROWS-1][MAP_COLS-1] == TILE_HARD_WALL);
    CHECK(g.tiles[1][1]       == TILE_EMPTY);
    CHECK(g.players[0].row    == 1);
    CHECK(g.players[0].col    == 1);
    CHECK(g.players[1].row    == MAP_ROWS - 2);
    CHECK(g.players[1].col    == MAP_COLS - 2);
}

static void test_move_each_direction(void)
{
    printf("test_move_each_direction\n");
    game_state_t g;
    game_init(&g);

    /* Put P1 in open interior to test all 4 directions */
    g.players[0].row = 5;
    g.players[0].col = 5;

    step_once(&g, ctrl_with(0,1,0,0,0,0), NONE);   /* down */
    CHECK(g.players[0].row == 6 && g.players[0].col == 5);
    CHECK(g.players[0].dir == DIR_DOWN);

    step_once(&g, ctrl_with(1,0,0,0,0,0), NONE);   /* up */
    CHECK(g.players[0].row == 5 && g.players[0].col == 5);
    CHECK(g.players[0].dir == DIR_UP);

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);   /* right */
    CHECK(g.players[0].row == 5 && g.players[0].col == 6);
    CHECK(g.players[0].dir == DIR_RIGHT);

    step_once(&g, ctrl_with(0,0,1,0,0,0), NONE);   /* left */
    CHECK(g.players[0].row == 5 && g.players[0].col == 5);
    CHECK(g.players[0].dir == DIR_LEFT);
}

static void test_boundary_and_wall(void)
{
    printf("test_boundary_and_wall\n");
    game_state_t g;
    game_init(&g);

    /* P1 at (1,1). Up and left are border hard walls. */
    step_once(&g, ctrl_with(1,0,0,0,0,0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);

    step_once(&g, ctrl_with(0,0,1,0,0,0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);

    /* Now plant a soft wall in front of P1 and make sure it blocks too. */
    g.tiles[1][2] = TILE_SOFT_WALL;
    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);
}

static void test_capture_empty_tile(void)
{
    printf("test_capture_empty_tile\n");
    game_state_t g;
    game_init(&g);
    CHECK(g.tiles[1][1] == TILE_EMPTY);

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);    /* P1 moves right */
    CHECK(g.players[0].row == 1 && g.players[0].col == 2);
    CHECK(g.tiles[1][2]    == TILE_PINK);
    CHECK(g.players[0].score == 1);
    /* Starting tile is not retroactively captured (by design). */
    CHECK(g.tiles[1][1] == TILE_EMPTY);
}

static void test_capture_blue_tile(void)
{
    printf("test_capture_blue_tile\n");
    game_state_t g;
    game_init(&g);
    g.tiles[1][2] = TILE_BLUE;

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.tiles[1][2]      == TILE_PINK);
    CHECK(g.players[0].score == 1);
}

static void test_capture_opponent_tile(void)
{
    printf("test_capture_opponent_tile\n");
    game_state_t g;
    game_init(&g);

    g.tiles[1][2]            = TILE_YELLOW;
    g.players[1].score       = 3;

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.tiles[1][2]      == TILE_PINK);
    CHECK(g.players[0].score == 1);
    CHECK(g.players[1].score == 2);
}

static void test_capture_own_tile_is_noop(void)
{
    printf("test_capture_own_tile_is_noop\n");
    game_state_t g;
    game_init(&g);
    g.tiles[1][2]            = TILE_PINK;
    g.players[0].score       = 5;

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.tiles[1][2]      == TILE_PINK);
    CHECK(g.players[0].score == 5);
    CHECK(g.players[0].row == 1 && g.players[0].col == 2);
}

static void test_cannot_walk_onto_other_player(void)
{
    printf("test_cannot_walk_onto_other_player\n");
    game_state_t g;
    game_init(&g);

    g.players[1].row = 1;
    g.players[1].col = 2;

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);
}

static void test_cannot_walk_onto_bomb(void)
{
    printf("test_cannot_walk_onto_bomb\n");
    game_state_t g;
    game_init(&g);

    g.bombs[0].active = 1;
    g.bombs[0].owner  = 0;
    g.bombs[0].row    = 1;
    g.bombs[0].col    = 2;

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);
}

static void test_cooldown_blocks_next_move(void)
{
    printf("test_cooldown_blocks_next_move\n");
    game_state_t g;
    g_move_cooldown_frames = 3;
    game_init(&g);

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].col == 2);

    /* cooldown = 3; next 3 frames: no move */
    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].col == 2);
    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].col == 2);
    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].col == 2);

    /* 4th frame: cooldown expired, moves again */
    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].col == 3);

    g_move_cooldown_frames = 0;
}

static void test_win_condition(void)
{
    printf("test_win_condition\n");
    game_state_t g;
    game_init(&g);
    g.players[0].score = WIN_SCORE - 1;

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].score >= WIN_SCORE);
    CHECK(g.game_over        == 1);
    CHECK(g.winner           == 0);
    CHECK(g.pending_sound    == SOUND_VICTORY);
}

static void test_restart_after_game_over(void)
{
    printf("test_restart_after_game_over\n");
    game_state_t g;
    game_init(&g);
    g.game_over = 1;
    g.winner    = 0;
    g.players[0].score = 42;
    g.players[0].row   = 7;

    step_once(&g, ctrl_with(0,0,0,0,0,1), NONE);   /* restart button */
    CHECK(g.game_over        == 0);
    CHECK(g.winner           == -1);
    CHECK(g.players[0].score == 0);
    CHECK(g.players[0].row   == 1);
}

static void test_dead_player_does_not_move(void)
{
    printf("test_dead_player_does_not_move\n");
    game_state_t g;
    game_init(&g);
    g.players[0].alive = 0;

    step_once(&g, ctrl_with(0,0,0,1,0,0), NONE);
    CHECK(g.players[0].row == 1 && g.players[0].col == 1);
}

/* ---------------- main ---------------- */

int main(void)
{
    /* Disable cooldown for most tests; the cooldown-specific test flips it. */
    g_move_cooldown_frames = 0;

    test_init();
    test_move_each_direction();
    test_boundary_and_wall();
    test_capture_empty_tile();
    test_capture_blue_tile();
    test_capture_opponent_tile();
    test_capture_own_tile_is_noop();
    test_cannot_walk_onto_other_player();
    test_cannot_walk_onto_bomb();
    test_cooldown_blocks_next_move();
    test_win_condition();
    test_restart_after_game_over();
    test_dead_player_does_not_move();

    printf("\n---- %d passed, %d failed ----\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
