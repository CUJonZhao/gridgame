#ifndef FPGA_VIDEO_H
#define FPGA_VIDEO_H

/*
 * fpga_video -- character device exposing the GridBrawl FPGA video
 * memory-mapped peripherals (Tile Map RAM + Sprite Registers) to user
 * space.
 *
 * Layout exposed via /dev/fpga_video, addressed by the file position
 * (use pwrite/pread or lseek + write/read):
 *
 *   [0x000 .. 0x12B]   Tile Map RAM     (300 bytes, one byte per tile,
 *                                        bits[2:0] = tile id)
 *   [0x12C .. 0x1FF]   reserved / hole, writes here are an error
 *   [0x200 .. 0x277]   Sprite Registers (30 * 4 bytes, little-endian
 *                                        Avalon-MM 32-bit words)
 *
 * Each sprite register has the layout from the design doc:
 *   bits [9:0]   x position
 *   bits [18:10] y position
 *   bits [23:19] sprite id
 *   bit  [24]    enable
 *   bits [31:25] reserved (write zero)
 *
 * The user-space helper FPGA_VIDEO_PACK_SPRITE() builds one such word.
 *
 * Sprite-region writes must be 4-byte aligned in length (one whole
 * register); tile-region writes can be any length within the region.
 */

#define FPGA_VIDEO_DEVICE_NAME    "fpga_video"

/* DE1-SoC HPS-to-FPGA lightweight bus addresses. These are placeholders
 * that must match the Platform Designer assignment in the eventual
 * Quartus project; they live next to the audio peripheral at 0xFF202000. */
#define FPGA_VIDEO_TILE_BASE      0xFF203000UL
#define FPGA_VIDEO_TILE_SPAN      0x200UL
#define FPGA_VIDEO_SPRITE_BASE    0xFF204000UL
#define FPGA_VIDEO_SPRITE_SPAN    0x80UL

#define FPGA_VIDEO_MAP_ROWS       15
#define FPGA_VIDEO_MAP_COLS       20
#define FPGA_VIDEO_TILE_BYTES     (FPGA_VIDEO_MAP_ROWS * FPGA_VIDEO_MAP_COLS) /* 300 */

#define FPGA_VIDEO_SPRITE_COUNT   30
#define FPGA_VIDEO_SPRITE_BYTES   (FPGA_VIDEO_SPRITE_COUNT * 4)               /* 120 */

/* Logical offsets within /dev/fpga_video. Sprite region starts at 0x200
 * so user code can keep its own contiguous buffers without overlap. */
#define FPGA_VIDEO_TILE_OFFSET    0x000UL
#define FPGA_VIDEO_TILE_END       (FPGA_VIDEO_TILE_OFFSET + FPGA_VIDEO_TILE_BYTES)
#define FPGA_VIDEO_SPRITE_OFFSET  0x200UL
#define FPGA_VIDEO_SPRITE_END     (FPGA_VIDEO_SPRITE_OFFSET + FPGA_VIDEO_SPRITE_BYTES)
#define FPGA_VIDEO_REGION_SIZE    FPGA_VIDEO_SPRITE_END

/* Bit layout helpers for sprite register words. */
/* Bit layout matches sprite_registers.sv:
 *   bits [9:0]   x position (10 bits)
 *   bits [18:10] y position (9 bits)
 *   bits [23:19] sprite id  (5 bits)
 *   bit  [24]    enable
 *   bits [31:25] reserved (write zero) */
#define FPGA_VIDEO_SPRITE_X_MASK      0x000003FFu
#define FPGA_VIDEO_SPRITE_Y_SHIFT     10
#define FPGA_VIDEO_SPRITE_Y_MASK      0x0007FC00u
#define FPGA_VIDEO_SPRITE_ID_SHIFT    19
#define FPGA_VIDEO_SPRITE_ID_MASK     0x00F80000u
#define FPGA_VIDEO_SPRITE_ENABLE_BIT  (1u << 24)

#define FPGA_VIDEO_PACK_SPRITE(x, y, id, en)                              \
    ((((unsigned)(x)  << 0)  & FPGA_VIDEO_SPRITE_X_MASK)  |               \
     (((unsigned)(y)  << FPGA_VIDEO_SPRITE_Y_SHIFT)       \
                                              & FPGA_VIDEO_SPRITE_Y_MASK) |\
     (((unsigned)(id) << FPGA_VIDEO_SPRITE_ID_SHIFT)      \
                                              & FPGA_VIDEO_SPRITE_ID_MASK)|\
     ((en) ? FPGA_VIDEO_SPRITE_ENABLE_BIT : 0u))

#endif
