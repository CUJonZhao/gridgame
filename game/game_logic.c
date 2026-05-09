#include "game.h"
#include <string.h>

#define RESPAWN_STATE_NONE 255u
#define RESPAWN_STATE_SAFE_WAIT 254u

static const tile_id_t initial_map[MAP_ROWS][MAP_COLS] = {
    {TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_SOFT_WALL, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL, TILE_HARD_WALL, TILE_SOFT_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_SOFT_WALL, TILE_BLUE, TILE_BLUE, TILE_BLUE, TILE_HARD_WALL},
    {TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL, TILE_HARD_WALL}
};

static controller_state_t prev_inputs[PLAYER_COUNT];
static uint8_t pending_soft_wall_hits[MAP_ROWS][MAP_COLS];
static uint8_t explosion_marked_walls[ACTIVE_EXPLOSION_COUNT][EXPLOSION_TILE_COUNT];
static uint8_t respawn_wait_state[PLAYER_COUNT];

static const uint8_t spawn_rows[PLAYER_COUNT] = {1, 1};
static const uint8_t spawn_cols[PLAYER_COUNT] = {1, 18};

static tile_id_t player_tile_id(uint8_t player_id)
{
    return player_id == 0 ? TILE_YELLOW : TILE_PINK;
}

static direction_t read_direction(const controller_state_t *input)
{
    if (input->up) {
        return DIR_UP;
    }
    if (input->down) {
        return DIR_DOWN;
    }
    if (input->left) {
        return DIR_LEFT;
    }
    if (input->right) {
        return DIR_RIGHT;
    }
    return DIR_NONE;
}

static uint8_t tile_is_enterable(tile_id_t tile)
{
    return tile == TILE_BLUE || tile == TILE_YELLOW || tile == TILE_PINK;
}

static void direction_delta(direction_t dir, int *dr, int *dc)
{
    *dr = 0;
    *dc = 0;
    if (dir == DIR_UP) {
        *dr = -1;
    } else if (dir == DIR_DOWN) {
        *dr = 1;
    } else if (dir == DIR_LEFT) {
        *dc = -1;
    } else if (dir == DIR_RIGHT) {
        *dc = 1;
    }
}

static int find_active_bomb_at(const game_state_t *game, uint8_t row, uint8_t col)
{
    int i;
    for (i = 0; i < ACTIVE_BOMB_COUNT; ++i) {
        if (game->bombs[i].active && game->bombs[i].row == row && game->bombs[i].col == col) {
            return i;
        }
    }
    return -1;
}

static uint8_t tile_blocked_by_other_player(const game_state_t *game, uint8_t player_index, uint8_t row, uint8_t col)
{
    const player_t *other = &game->players[1 - player_index];

    if (!other->alive) {
        return 0;
    }
    if (other->row == row && other->col == col) {
        return 1;
    }
    if (other->moving && other->target_row == row && other->target_col == col) {
        return 1;
    }
    return 0;
}

static uint8_t can_enter_tile(const game_state_t *game, uint8_t player_index, uint8_t row, uint8_t col)
{
    if (row >= MAP_ROWS || col >= MAP_COLS) {
        return 0;
    }
    if (!tile_is_enterable(game->tiles[row][col])) {
        return 0;
    }
    if (find_active_bomb_at(game, row, col) >= 0) {
        return 0;
    }
    if (tile_blocked_by_other_player(game, player_index, row, col)) {
        return 0;
    }
    return 1;
}

static void set_player_target(player_t *player, direction_t dir, uint8_t row, uint8_t col)
{
    player->moving = 1;
    player->dir = dir;
    player->target_row = row;
    player->target_col = col;
    player->target_x = (uint16_t)(col * TILE_SIZE);
    player->target_y = (uint16_t)(row * TILE_SIZE);
}

static uint8_t step_toward_u16(uint16_t *value, uint16_t target)
{
    if (*value < target) {
        uint16_t delta = (uint16_t)(target - *value);
        *value = (uint16_t)(*value + (delta > PLAYER_SPEED ? PLAYER_SPEED : delta));
    } else if (*value > target) {
        uint16_t delta = (uint16_t)(*value - target);
        *value = (uint16_t)(*value - (delta > PLAYER_SPEED ? PLAYER_SPEED : delta));
    }
    return *value == target;
}

static uint8_t advance_player_motion(player_t *player)
{
    uint8_t reached_x;
    uint8_t reached_y;

    if (!player->moving) {
        return 0;
    }

    reached_x = step_toward_u16(&player->x, player->target_x);
    reached_y = step_toward_u16(&player->y, player->target_y);

    if (reached_x && reached_y) {
        player->x = player->target_x;
        player->y = player->target_y;
        player->row = player->target_row;
        player->col = player->target_col;
        player->moving = 0;
        return 1;
    }

    return 0;
}

static void apply_tile_capture(game_state_t *game, uint8_t player_index)
{
    player_t *player = &game->players[player_index];
    tile_id_t own_tile = player_tile_id(player_index);
    tile_id_t opponent_tile = player_tile_id((uint8_t)(1 - player_index));
    tile_id_t *tile = &game->tiles[player->row][player->col];

    if (*tile == TILE_BLUE) {
        *tile = own_tile;
        player->score = (uint16_t)(player->score + 1);
    } else if (*tile == opponent_tile) {
        *tile = own_tile;
        player->score = (uint16_t)(player->score + 1);
        if (game->players[1 - player_index].score > 0) {
            game->players[1 - player_index].score = (uint16_t)(game->players[1 - player_index].score - 1);
        }
    }
}

static uint8_t build_move_candidate(const game_state_t *game, uint8_t player_index, direction_t dir, uint8_t *target_row, uint8_t *target_col)
{
    const player_t *player = &game->players[player_index];
    int dr;
    int dc;
    int next_row;
    int next_col;

    if (!player->alive || player->moving || dir == DIR_NONE) {
        return 0;
    }

    direction_delta(dir, &dr, &dc);
    next_row = (int)player->row + dr;
    next_col = (int)player->col + dc;

    if (next_row < 0 || next_row >= MAP_ROWS || next_col < 0 || next_col >= MAP_COLS) {
        return 0;
    }

    *target_row = (uint8_t)next_row;
    *target_col = (uint8_t)next_col;

    return can_enter_tile(game, player_index, *target_row, *target_col);
}

static void clear_player_territory(game_state_t *game, uint8_t player_index)
{
    tile_id_t own_tile = player_tile_id(player_index);
    int row;
    int col;

    for (row = 0; row < MAP_ROWS; ++row) {
        for (col = 0; col < MAP_COLS; ++col) {
            if (game->tiles[row][col] == own_tile) {
                game->tiles[row][col] = TILE_BLUE;
            }
        }
    }
}

static uint8_t rect_overlaps_tile(uint16_t x, uint16_t y, uint8_t row, uint8_t col)
{
    int ax1 = x;
    int ay1 = y;
    int ax2 = ax1 + TILE_SIZE - 1;
    int ay2 = ay1 + TILE_SIZE - 1;
    int bx1 = col * TILE_SIZE;
    int by1 = row * TILE_SIZE;
    int bx2 = bx1 + TILE_SIZE - 1;
    int by2 = by1 + TILE_SIZE - 1;

    if (ax2 < bx1 || bx2 < ax1) {
        return 0;
    }
    if (ay2 < by1 || by2 < ay1) {
        return 0;
    }
    return 1;
}

static uint8_t explosion_hits_player(const explosion_t *explosion, const player_t *player)
{
    int i;
    for (i = 0; i < EXPLOSION_TILE_COUNT; ++i) {
        if (rect_overlaps_tile(player->x, player->y, explosion->cells[i].row, explosion->cells[i].col)) {
            return 1;
        }
    }
    return 0;
}

static void kill_player(game_state_t *game, uint8_t player_index, uint8_t explosion_index)
{
    player_t *player = &game->players[player_index];

    if (!player->alive) {
        return;
    }

    player->alive = 0;
    player->moving = 0;
    player->pending_dir = DIR_NONE;
    player->target_row = player->row;
    player->target_col = player->col;
    player->target_x = player->x;
    player->target_y = player->y;

    clear_player_territory(game, player_index);
    player->score = 0;

    respawn_wait_state[player_index] = explosion_index;
}

static void fill_explosion_cells(explosion_t *explosion)
{
    explosion->cells[0].row = explosion->center_row;
    explosion->cells[0].col = explosion->center_col;
    explosion->cells[1].row = (uint8_t)(explosion->center_row - 1);
    explosion->cells[1].col = explosion->center_col;
    explosion->cells[2].row = (uint8_t)(explosion->center_row + 1);
    explosion->cells[2].col = explosion->center_col;
    explosion->cells[3].row = explosion->center_row;
    explosion->cells[3].col = (uint8_t)(explosion->center_col - 1);
    explosion->cells[4].row = explosion->center_row;
    explosion->cells[4].col = (uint8_t)(explosion->center_col + 1);
}

static uint8_t create_explosion_from_bomb(game_state_t *game, uint8_t index)
{
    bomb_t *bomb = &game->bombs[index];
    explosion_t *explosion = &game->explosions[index];
    uint8_t hit_soft_wall = 0;
    int i;

    explosion->active = 1;
    explosion->owner = bomb->owner;
    explosion->center_row = bomb->row;
    explosion->center_col = bomb->col;
    explosion->timer = EXPLOSION_TIMER_TICKS;
    fill_explosion_cells(explosion);

    memset(explosion_marked_walls[index], 0, sizeof(explosion_marked_walls[index]));

    for (i = 0; i < EXPLOSION_TILE_COUNT; ++i) {
        uint8_t row = explosion->cells[i].row;
        uint8_t col = explosion->cells[i].col;
        if (game->tiles[row][col] == TILE_SOFT_WALL) {
            pending_soft_wall_hits[row][col] = (uint8_t)(pending_soft_wall_hits[row][col] + 1);
            explosion_marked_walls[index][i] = 1;
            hit_soft_wall = 1;
        }
    }

    bomb->active = 0;
    bomb->timer = 0;

    return hit_soft_wall;
}

static void cleanup_explosion(game_state_t *game, uint8_t index)
{
    explosion_t *explosion = &game->explosions[index];
    int i;

    for (i = 0; i < EXPLOSION_TILE_COUNT; ++i) {
        uint8_t row = explosion->cells[i].row;
        uint8_t col = explosion->cells[i].col;

        if (explosion_marked_walls[index][i]) {
            if (pending_soft_wall_hits[row][col] > 0) {
                pending_soft_wall_hits[row][col] = (uint8_t)(pending_soft_wall_hits[row][col] - 1);
            }
            if (pending_soft_wall_hits[row][col] == 0 && game->tiles[row][col] == TILE_SOFT_WALL) {
                game->tiles[row][col] = TILE_BLUE;
            }
            explosion_marked_walls[index][i] = 0;
        }
    }

    explosion->active = 0;
    explosion->timer = 0;
}

static uint8_t tile_in_active_explosion(const game_state_t *game, uint8_t row, uint8_t col)
{
    int i;
    int j;

    for (i = 0; i < ACTIVE_EXPLOSION_COUNT; ++i) {
        if (!game->explosions[i].active) {
            continue;
        }
        for (j = 0; j < EXPLOSION_TILE_COUNT; ++j) {
            if (game->explosions[i].cells[j].row == row && game->explosions[i].cells[j].col == col) {
                return 1;
            }
        }
    }

    return 0;
}

static uint8_t spawn_tile_safe(const game_state_t *game, uint8_t player_index)
{
    uint8_t row = spawn_rows[player_index];
    uint8_t col = spawn_cols[player_index];
    const player_t *other = &game->players[1 - player_index];

    if (!tile_is_enterable(game->tiles[row][col])) {
        return 0;
    }
    if (find_active_bomb_at(game, row, col) >= 0) {
        return 0;
    }
    if (tile_in_active_explosion(game, row, col)) {
        return 0;
    }
    if (other->alive) {
        if (other->row == row && other->col == col) {
            return 0;
        }
        if (other->moving && other->target_row == row && other->target_col == col) {
            return 0;
        }
    }
    return 1;
}

static void respawn_player(game_state_t *game, uint8_t player_index)
{
    player_t *player = &game->players[player_index];

    player->alive = 1;
    player->moving = 0;
    player->row = spawn_rows[player_index];
    player->col = spawn_cols[player_index];
    player->x = (uint16_t)(player->col * TILE_SIZE);
    player->y = (uint16_t)(player->row * TILE_SIZE);
    player->target_row = player->row;
    player->target_col = player->col;
    player->target_x = player->x;
    player->target_y = player->y;
    player->dir = DIR_DOWN;
    player->pending_dir = DIR_NONE;

    respawn_wait_state[player_index] = RESPAWN_STATE_NONE;
}

static void reset_internal_state(void)
{
    memset(prev_inputs, 0, sizeof(prev_inputs));
    memset(pending_soft_wall_hits, 0, sizeof(pending_soft_wall_hits));
    memset(explosion_marked_walls, 0, sizeof(explosion_marked_walls));
    memset(respawn_wait_state, RESPAWN_STATE_NONE, sizeof(respawn_wait_state));
}

void game_reset(game_state_t *game)
{
    int i;

    memcpy(game->tiles, initial_map, sizeof(initial_map));
    memset(game->bombs, 0, sizeof(game->bombs));
    memset(game->explosions, 0, sizeof(game->explosions));

    for (i = 0; i < PLAYER_COUNT; ++i) {
        game->players[i].id = (uint8_t)i;
        game->players[i].alive = 1;
        game->players[i].row = spawn_rows[i];
        game->players[i].col = spawn_cols[i];
        game->players[i].x = (uint16_t)(spawn_cols[i] * TILE_SIZE);
        game->players[i].y = (uint16_t)(spawn_rows[i] * TILE_SIZE);
        game->players[i].target_row = game->players[i].row;
        game->players[i].target_col = game->players[i].col;
        game->players[i].target_x = game->players[i].x;
        game->players[i].target_y = game->players[i].y;
        game->players[i].moving = 0;
        game->players[i].score = 0;
        game->players[i].dir = DIR_DOWN;
        game->players[i].pending_dir = DIR_NONE;

        game->bombs[i].active = 0;
        game->bombs[i].owner = (uint8_t)i;
        game->bombs[i].row = spawn_rows[i];
        game->bombs[i].col = spawn_cols[i];
        game->bombs[i].timer = 0;

        game->explosions[i].active = 0;
        game->explosions[i].owner = (uint8_t)i;
        game->explosions[i].center_row = spawn_rows[i];
        game->explosions[i].center_col = spawn_cols[i];
        game->explosions[i].timer = 0;
        memset(game->explosions[i].cells, 0, sizeof(game->explosions[i].cells));
    }

    game->pending_sound_1 = SOUND_NONE;
    game->pending_sound_2 = SOUND_NONE;
    game->game_over = 0;
    game->winner = -1;

    reset_internal_state();
}

void game_init(game_state_t *game)
{
    game_reset(game);
}

void game_step(game_state_t *game, const controller_state_t inputs[PLAYER_COUNT])
{
    direction_t desired_dirs[PLAYER_COUNT];
    uint8_t arrived[PLAYER_COUNT] = {0, 0};
    uint8_t candidate_valid[PLAYER_COUNT] = {0, 0};
    uint8_t candidate_row[PLAYER_COUNT] = {0, 0};
    uint8_t candidate_col[PLAYER_COUNT] = {0, 0};
    uint8_t explosion_triggered = 0;
    uint8_t wall_hit_this_frame = 0;
    uint8_t player_died_this_frame = 0;
    int i;

    if (inputs[0].restart || inputs[1].restart) {
        game_reset(game);
        return;
    }

    game->pending_sound_1 = SOUND_NONE;
    game->pending_sound_2 = SOUND_NONE;

    if (game->game_over) {
        for (i = 0; i < PLAYER_COUNT; ++i) {
            prev_inputs[i] = inputs[i];
        }
        return;
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        desired_dirs[i] = read_direction(&inputs[i]);
        if (game->players[i].alive) {
            game->players[i].pending_dir = desired_dirs[i];
        } else {
            game->players[i].pending_dir = DIR_NONE;
        }
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (game->players[i].alive) {
            arrived[i] = advance_player_motion(&game->players[i]);
        }
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (arrived[i] && game->players[i].alive) {
            apply_tile_capture(game, (uint8_t)i);
        }
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (build_move_candidate(game, (uint8_t)i, game->players[i].pending_dir, &candidate_row[i], &candidate_col[i])) {
            candidate_valid[i] = 1;
        }
    }

    if (candidate_valid[0] && candidate_valid[1] &&
        candidate_row[0] == candidate_row[1] &&
        candidate_col[0] == candidate_col[1]) {
        candidate_valid[0] = 0;
        candidate_valid[1] = 0;
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (candidate_valid[i]) {
            set_player_target(&game->players[i], game->players[i].pending_dir, candidate_row[i], candidate_col[i]);
        }
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        uint8_t bomb_pressed = (uint8_t)(inputs[i].bomb && !prev_inputs[i].bomb);
        player_t *player = &game->players[i];
        bomb_t *bomb = &game->bombs[i];
        explosion_t *explosion = &game->explosions[i];

        if (!bomb_pressed) {
            continue;
        }
        if (!player->alive || player->moving) {
            continue;
        }
        if (bomb->active || explosion->active) {
            continue;
        }
        if (!tile_is_enterable(game->tiles[player->row][player->col])) {
            continue;
        }
        if (find_active_bomb_at(game, player->row, player->col) >= 0) {
            continue;
        }

        bomb->active = 1;
        bomb->owner = (uint8_t)i;
        bomb->row = player->row;
        bomb->col = player->col;
        bomb->timer = BOMB_TIMER_TICKS;
    }

    for (i = 0; i < ACTIVE_BOMB_COUNT; ++i) {
        if (!game->bombs[i].active) {
            continue;
        }
        if (game->bombs[i].timer > 0) {
            game->bombs[i].timer--;
        }
        if (game->bombs[i].timer == 0) {
            explosion_triggered = 1;
            if (create_explosion_from_bomb(game, (uint8_t)i)) {
                wall_hit_this_frame = 1;
            }
        }
    }

    for (i = 0; i < ACTIVE_EXPLOSION_COUNT; ++i) {
        int p;
        if (!game->explosions[i].active) {
            continue;
        }
        for (p = 0; p < PLAYER_COUNT; ++p) {
            if (game->players[p].alive && explosion_hits_player(&game->explosions[i], &game->players[p])) {
                kill_player(game, (uint8_t)p, (uint8_t)i);
                player_died_this_frame = 1;
            }
        }
    }

    for (i = 0; i < ACTIVE_EXPLOSION_COUNT; ++i) {
        if (!game->explosions[i].active) {
            continue;
        }
        if (game->explosions[i].timer > 0) {
            game->explosions[i].timer--;
        }
        if (game->explosions[i].timer == 0) {
            cleanup_explosion(game, (uint8_t)i);
        }
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        if (game->players[i].alive) {
            continue;
        }

        if (respawn_wait_state[i] < ACTIVE_EXPLOSION_COUNT) {
            if (game->explosions[respawn_wait_state[i]].active) {
                continue;
            }
            respawn_wait_state[i] = RESPAWN_STATE_SAFE_WAIT;
        }

        if (respawn_wait_state[i] == RESPAWN_STATE_SAFE_WAIT && spawn_tile_safe(game, (uint8_t)i)) {
            respawn_player(game, (uint8_t)i);
        }
    }

    if (game->players[0].score >= WIN_SCORE && game->players[1].score >= WIN_SCORE) {
        game->game_over = 1;
        if (game->players[0].score > game->players[1].score) {
            game->winner = 0;
        } else if (game->players[1].score > game->players[0].score) {
            game->winner = 1;
        } else {
            game->winner = -1;
        }
    } else if (game->players[0].score >= WIN_SCORE) {
        game->game_over = 1;
        game->winner = 0;
    } else if (game->players[1].score >= WIN_SCORE) {
        game->game_over = 1;
        game->winner = 1;
    }

    if (game->game_over) {
        game->pending_sound_1 = SOUND_VICTORY;
        game->pending_sound_2 = SOUND_NONE;
    } else if (explosion_triggered) {
        game->pending_sound_1 = SOUND_EXPLOSION;
        if (player_died_this_frame) {
            game->pending_sound_2 = SOUND_PLAYER_DIES;
        } else if (wall_hit_this_frame) {
            game->pending_sound_2 = SOUND_WALL_BREAKS;
        }
    } else if (player_died_this_frame) {
        game->pending_sound_1 = SOUND_PLAYER_DIES;
    }

    for (i = 0; i < PLAYER_COUNT; ++i) {
        prev_inputs[i] = inputs[i];
    }
}
