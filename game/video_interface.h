#ifndef VIDEO_INTERFACE_H
#define VIDEO_INTERFACE_H

#include "game.h"

#include <stdint.h>
#include <stddef.h>

#define FPGA_VIDEO_DEV_PATH "/dev/fpga_video"

/* Match the kernel side. Duplicated here so the user-space build does
 * not need to include the kernel header. */
#define VIDEO_TILE_OFFSET     0x000UL
#define VIDEO_TILE_BYTES      (MAP_ROWS * MAP_COLS)        /* 300 */
#define VIDEO_SPRITE_OFFSET   0x200UL
#define VIDEO_SPRITE_COUNT    SPRITE_REGISTER_COUNT        /* 30 */
#define VIDEO_SPRITE_BYTES    (VIDEO_SPRITE_COUNT * 4)     /* 120 */

/* Bit layout matches sprite_registers.sv:
 *   bits [9:0]   x position (10 bits, max 1023)
 *   bits [18:10] y position (9 bits,  max 511)
 *   bits [23:19] sprite id  (5 bits,  max 31)
 *   bit  [24]    enable
 *   bits [31:25] reserved (write zero) */
#define VIDEO_SPRITE_X_MASK      0x000003FFu
#define VIDEO_SPRITE_Y_SHIFT     10
#define VIDEO_SPRITE_Y_MASK      0x0007FC00u
#define VIDEO_SPRITE_ID_SHIFT    19
#define VIDEO_SPRITE_ID_MASK     0x00F80000u
#define VIDEO_SPRITE_ENABLE_BIT  (1u << 24)

int  video_interface_init(void);
void video_interface_close(void);

void video_set_tile(unsigned row, unsigned col, uint8_t tile_id);
void video_set_sprite(unsigned index, uint16_t x, uint16_t y,
                      uint8_t sprite_id, uint8_t enable);
void video_clear_sprites(void);

int video_flush(void);

static inline uint32_t video_pack_sprite(uint16_t x, uint16_t y,
                                         uint8_t sprite_id, uint8_t enable)
{
    return ((uint32_t)x  & VIDEO_SPRITE_X_MASK) |
           (((uint32_t)y << VIDEO_SPRITE_Y_SHIFT) & VIDEO_SPRITE_Y_MASK) |
           (((uint32_t)sprite_id << VIDEO_SPRITE_ID_SHIFT) &
                                                  VIDEO_SPRITE_ID_MASK) |
           (enable ? VIDEO_SPRITE_ENABLE_BIT : 0u);
}

const uint8_t  *video_test_get_tiles(void);
const uint32_t *video_test_get_sprites(void);

#endif
