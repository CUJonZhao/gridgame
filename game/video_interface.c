/*
 * video_interface.c -- user-space staging buffer + writer for the
 * GridBrawl FPGA video peripherals exposed via /dev/fpga_video.
 *
 * The kernel driver enforces that a single write() does not cross the
 * tile/sprite boundary, so this layer issues two pwrite() calls per
 * frame. 420 bytes/frame at 32 Hz is ~13 KB/s, well below the noise
 * floor for a misc device.
 */

#define _POSIX_C_SOURCE 200809L

#include "video_interface.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int      video_fd = -1;
static uint8_t  shadow_tiles[VIDEO_TILE_BYTES];
static uint32_t shadow_sprites[VIDEO_SPRITE_COUNT];

int video_interface_init(void)
{
    const char *path;
    int flags;

    if (video_fd >= 0) {
        return 0;
    }

    path = getenv("FPGA_VIDEO_DEV");
    if (path == NULL || path[0] == '\0') {
        path = FPGA_VIDEO_DEV_PATH;
    }

    /* O_CREAT lets us redirect to a regular file in loopback tests where
     * the kernel module is not loaded. The fake file gets a 420-byte
     * sparse window we can pwrite() into. */
    flags = O_RDWR | O_CREAT;
    video_fd = open(path, flags, 0666);
    if (video_fd < 0) {
        fprintf(stderr,
                "video_interface_init: open(%s) failed: %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}

void video_interface_close(void)
{
    if (video_fd >= 0) {
        close(video_fd);
        video_fd = -1;
    }
}

void video_set_tile(unsigned row, unsigned col, uint8_t tile_id)
{
    unsigned idx;

    if (row >= MAP_ROWS || col >= MAP_COLS) {
        return;
    }
    idx = row * MAP_COLS + col;
    /* Bits [2:0] only -- the FPGA tile RAM honours the same width. */
    shadow_tiles[idx] = (uint8_t)(tile_id & 0x07);
}

void video_set_sprite(unsigned index, uint16_t x, uint16_t y,
                      uint8_t sprite_id, uint8_t enable)
{
    if (index >= VIDEO_SPRITE_COUNT) {
        return;
    }
    shadow_sprites[index] = video_pack_sprite(x, y, sprite_id, enable);
}

void video_clear_sprites(void)
{
    /* Zero == disabled, sprite_id 0, x=y=0. The FPGA ignores disabled
     * sprites in the scan compositor regardless of the other fields. */
    memset(shadow_sprites, 0, sizeof(shadow_sprites));
}

static int write_full(int fd, off_t offset, const void *buf, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)buf;
    size_t written = 0;

    while (written < len) {
        ssize_t n = pwrite(fd, bytes + written, len - written,
                           offset + (off_t)written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            errno = EIO;
            return -1;
        }
        written += (size_t)n;
    }
    return 0;
}

int video_flush(void)
{
    static int warned = 0;

    if (video_fd < 0) {
        /* Quiet success: render_test.c uses the staging API without
         * ever opening a device. */
        return 0;
    }

    if (write_full(video_fd, (off_t)VIDEO_TILE_OFFSET,
                   shadow_tiles, sizeof(shadow_tiles)) != 0) {
        if (!warned) {
            fprintf(stderr,
                    "video_flush: tile pwrite failed: %s\n",
                    strerror(errno));
            warned = 1;
        }
        return -1;
    }

    if (write_full(video_fd, (off_t)VIDEO_SPRITE_OFFSET,
                   shadow_sprites, sizeof(shadow_sprites)) != 0) {
        if (!warned) {
            fprintf(stderr,
                    "video_flush: sprite pwrite failed: %s\n",
                    strerror(errno));
            warned = 1;
        }
        return -1;
    }
    return 0;
}

const uint8_t *video_test_get_tiles(void)
{
    return shadow_tiles;
}

const uint32_t *video_test_get_sprites(void)
{
    return shadow_sprites;
}
