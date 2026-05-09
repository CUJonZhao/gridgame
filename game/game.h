#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAP_ROWS 15
#define MAP_COLS 20
#define TILE_SIZE 32
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define PLAYER_COUNT 2
#define ACTIVE_BOMB_COUNT 2
#define ACTIVE_EXPLOSION_COUNT 2
#define EXPLOSION_TILE_COUNT 5
#define SPRITE_REGISTER_COUNT 30
#define SPRITE_ID_COUNT 30
#define TILE_ID_COUNT 6
#define WIN_SCORE 80

#define GAME_TICK_HZ 32
#define PLAYER_SPEED 2
#define BOMB_TIMER_TICKS 160
#define EXPLOSION_TIMER_TICKS 32

#define UI_ROW_START 1
#define UI_ROW_END 2
#define UI_COL_START 6
#define UI_COL_END 13

typedef enum {
    TILE_EMPTY = 0,
    TILE_BLUE = 1,
    TILE_PINK = 2,
    TILE_YELLOW = 3,
    TILE_HARD_WALL = 4,
    TILE_SOFT_WALL = 5
} tile_id_t;

typedef enum {
    DIR_UP = 0,
    DIR_DOWN = 1,
    DIR_LEFT = 2,
    DIR_RIGHT = 3,
    DIR_NONE = 4
} direction_t;

typedef enum {
    SPRITE_PLAYER1_FRONT = 0,
    SPRITE_PLAYER1_BACK = 1,
    SPRITE_PLAYER1_LEFT = 2,
    SPRITE_PLAYER1_RIGHT = 3,
    SPRITE_PLAYER2_FRONT = 4,
    SPRITE_PLAYER2_BACK = 5,
    SPRITE_PLAYER2_LEFT = 6,
    SPRITE_PLAYER2_RIGHT = 7,
    SPRITE_PLAYER1_BOMB = 8,
    SPRITE_PLAYER2_BOMB = 9,
    SPRITE_PLAYER1_EXPLOSION_CENTER = 10,
    SPRITE_PLAYER2_EXPLOSION_CENTER = 11,
    SPRITE_PLAYER1_EXPLOSION_UP = 12,
    SPRITE_PLAYER1_EXPLOSION_DOWN = 13,
    SPRITE_PLAYER2_EXPLOSION_UP = 14,
    SPRITE_PLAYER2_EXPLOSION_DOWN = 15,
    SPRITE_PLAYER1_EXPLOSION_LEFT = 16,
    SPRITE_PLAYER1_EXPLOSION_RIGHT = 17,
    SPRITE_PLAYER2_EXPLOSION_LEFT = 18,
    SPRITE_PLAYER2_EXPLOSION_RIGHT = 19,
    SPRITE_UI_CHARACTER_S = 20,
    SPRITE_UI_CHARACTER_V = 21,
    SPRITE_UI_PINK_W = 22,
    SPRITE_UI_PINK_I = 23,
    SPRITE_UI_PINK_N = 24,
    SPRITE_UI_YELLOW_W = 25,
    SPRITE_UI_YELLOW_I = 26,
    SPRITE_UI_YELLOW_N = 27,
    SPRITE_UI_PLAYER1 = 28,
    SPRITE_UI_PLAYER2 = 29
} sprite_id_t;

typedef enum {
    SOUND_NONE = 0,
    SOUND_EXPLOSION = 1,
    SOUND_PLAYER_DIES = 2,
    SOUND_VICTORY = 3,
    SOUND_WALL_BREAKS = 4
} sound_id_t;

typedef struct {
    sound_id_t sound_id;
} audio_t;

typedef struct {
    uint8_t up;
    uint8_t down;
    uint8_t left;
    uint8_t right;
    uint8_t bomb;
    uint8_t restart;
} controller_state_t;

typedef struct {
    uint8_t row;
    uint8_t col;
} grid_pos_t;

typedef struct {
    uint8_t active;
    uint8_t owner;
    uint8_t row;
    uint8_t col;
    uint32_t timer;
} bomb_t;

typedef struct {
    uint8_t active;
    uint8_t owner;
    uint8_t center_row;
    uint8_t center_col;
    uint32_t timer;
    grid_pos_t cells[EXPLOSION_TILE_COUNT];
} explosion_t;

typedef struct {
    uint8_t id;
    uint8_t alive;
    uint8_t row;
    uint8_t col;
    uint16_t x;
    uint16_t y;
    uint8_t target_row;
    uint8_t target_col;
    uint16_t target_x;
    uint16_t target_y;
    uint8_t moving;
    uint16_t score;
    direction_t dir;
    direction_t pending_dir;
} player_t;

typedef struct {
    tile_id_t tiles[MAP_ROWS][MAP_COLS];
    player_t players[PLAYER_COUNT];
    bomb_t bombs[ACTIVE_BOMB_COUNT];
    explosion_t explosions[ACTIVE_EXPLOSION_COUNT];
    sound_id_t pending_sound_1;
    sound_id_t pending_sound_2;
    uint8_t game_over;
    int8_t winner;
} game_state_t;

typedef struct {
    uint8_t row;
    uint8_t col;
    tile_id_t tile_id;
} tile_t;

typedef struct {
    uint8_t sprite_index;
    uint16_t x;
    uint16_t y;
    sprite_id_t sprite_id;
    uint8_t enable;
} sprite_t;

void game_init(game_state_t *game);
void game_reset(game_state_t *game);
void game_step(game_state_t *game, const controller_state_t inputs[PLAYER_COUNT]);

#endif
