#include "game.h"

#include <string.h>

/* Tunable: frames between successive moves when a direction is held.
 * Tests may set this to 0 to disable the cooldown. */
uint32_t g_move_cooldown_frames = 8;

static uint32_t move_cooldown[PLAYER_COUNT];

static void init_tiles(game_state_t *g)
{
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (r == 0 || r == MAP_ROWS - 1 || c == 0 || c == MAP_COLS - 1) {
                g->tiles[r][c] = TILE_HARD_WALL;
            } else {
                g->tiles[r][c] = TILE_EMPTY;
            }
        }
    }
    /* Soft walls are intentionally NOT placed here.
     * Member B will add them together with the bomb/explosion logic. */
}

static void init_players(game_state_t *g)
{
    g->players[0].id    = 0;
    g->players[0].alive = 1;
    g->players[0].row   = 1;
    g->players[0].col   = 1;
    g->players[0].score = 0;
    g->players[0].dir   = DIR_DOWN;

    g->players[1].id    = 1;
    g->players[1].alive = 1;
    g->players[1].row   = MAP_ROWS - 2;
    g->players[1].col   = MAP_COLS - 2;
    g->players[1].score = 0;
    g->players[1].dir   = DIR_UP;
}

void game_init(game_state_t *g)
{
    memset(g, 0, sizeof(*g));
    init_tiles(g);
    init_players(g);
    g->pending_sound = SOUND_NONE;
    g->game_over     = 0;
    g->winner        = -1;
    memset(move_cooldown, 0, sizeof(move_cooldown));
}

void game_reset(game_state_t *g)
{
    game_init(g);
}

static int tile_is_walkable(tile_id_t t)
{
    return t != TILE_HARD_WALL && t != TILE_SOFT_WALL;
}

static int another_player_at(const game_state_t *g, int row, int col, int except)
{
    for (int i = 0; i < PLAYER_COUNT; i++) {
        if (i == except) continue;
        if (g->players[i].alive &&
            g->players[i].row == row &&
            g->players[i].col == col) {
            return 1;
        }
    }
    return 0;
}

static int bomb_at(const game_state_t *g, int row, int col)
{
    for (int i = 0; i < ACTIVE_BOMB_COUNT; i++) {
        if (g->bombs[i].active &&
            g->bombs[i].row == row &&
            g->bombs[i].col == col) {
            return 1;
        }
    }
    return 0;
}

static tile_id_t player_color(int pidx)
{
    return (pidx == 0) ? TILE_PINK : TILE_YELLOW;
}

static void apply_capture(game_state_t *g, int pidx, int row, int col)
{
    tile_id_t old    = g->tiles[row][col];
    tile_id_t mine   = player_color(pidx);
    tile_id_t theirs = player_color(1 - pidx);

    if (old == mine) return;

    if (old == TILE_EMPTY || old == TILE_BLUE) {
        g->tiles[row][col] = mine;
        g->players[pidx].score++;
    } else if (old == theirs) {
        g->tiles[row][col] = mine;
        g->players[pidx].score++;
        if (g->players[1 - pidx].score > 0) {
            g->players[1 - pidx].score--;
        }
    }

    if (g->players[pidx].score >= WIN_SCORE && !g->game_over) {
        g->game_over     = 1;
        g->winner        = (int8_t)pidx;
        g->pending_sound = SOUND_VICTORY;
    }
}

static void player_step(game_state_t *g, int pidx, const controller_state_t *in)
{
    player_t *p = &g->players[pidx];
    if (!p->alive) return;

    if (move_cooldown[pidx] > 0) {
        move_cooldown[pidx]--;
        return;
    }

    int dr = 0, dc = 0;
    if      (in->up)    { dr = -1; p->dir = DIR_UP;    }
    else if (in->down)  { dr =  1; p->dir = DIR_DOWN;  }
    else if (in->left)  { dc = -1; p->dir = DIR_LEFT;  }
    else if (in->right) { dc =  1; p->dir = DIR_RIGHT; }
    else return;

    int nr = p->row + dr;
    int nc = p->col + dc;

    if (nr < 0 || nr >= MAP_ROWS || nc < 0 || nc >= MAP_COLS) return;
    if (!tile_is_walkable(g->tiles[nr][nc]))                  return;
    if (another_player_at(g, nr, nc, pidx))                   return;
    if (bomb_at(g, nr, nc))                                   return;

    p->row = (uint8_t)nr;
    p->col = (uint8_t)nc;
    move_cooldown[pidx] = g_move_cooldown_frames;

    apply_capture(g, pidx, nr, nc);
}

void game_step(game_state_t *g, const controller_state_t inputs[PLAYER_COUNT])
{
    if (g->game_over) {
        if (inputs[0].restart || inputs[1].restart) {
            game_reset(g);
        }
        return;
    }

    /* === Member A: movement + tile coloring + score + win check === */
    for (int p = 0; p < PLAYER_COUNT; p++) {
        player_step(g, p, &inputs[p]);
    }

    /* === Member B: bomb / explosion / player death / clear-territory ===
     * TODO (Member B):
     *   - on inputs[p].bomb rising edge: place bomb at player's tile
     *   - decrement bomb timers; when 0 -> spawn explosion, set pending_sound = SOUND_EXPLOSION
     *   - explosion cross: destroy soft walls (set pending_sound = SOUND_WALL_BREAKS),
     *     kill players in range (alive = 0, set pending_sound = SOUND_PLAYER_DIES,
     *     clear their captured tiles back to TILE_EMPTY and zero their score)
     *   - decrement explosion timers
     */
}
