#ifndef RENDER_H
#define RENDER_H

#include "game.h"

/*
 * render_init -- prime the FPGA tile RAM and sprite registers with a
 * known-empty state. Safe to call before video_interface_init() (it
 * only writes the staging buffers in that case).
 */
void render_init(void);

/*
 * render_frame -- translate the current game_state_t into FPGA tile
 * RAM bytes and 30 sprite registers, then push the result through
 * video_flush(). The function never reads the game state through
 * dangling pointers; it copies what it needs while iterating.
 *
 * Sprite slot allocation (lower index = lower compositor priority):
 *   slot 0..1   : players P1, P2 (skipped if not alive)
 *   slot 2..3   : active bombs
 *   slot 4..13  : explosion cells (up to 2 explosions x 5 cells)
 *   slot 14..29 : reserved for UI / win banner / future use
 */
void render_frame(const game_state_t *game);

#endif
