/*
 * fpga_video -- Linux misc-device driver for the GridBrawl FPGA video
 * memory-mapped peripherals on the DE1-SoC.
 *
 * The device exposes a single linear address space (see fpga_video.h)
 * that is the concatenation of the Tile Map RAM and the Sprite Register
 * file, separated by a small reserved gap so the two regions can grow
 * independently without colliding in the Avalon-MM map.
 *
 * Writes are byte-addressed by the file position. The driver translates
 * each chunk of the write into either an iowrite8() loop into Tile Map
 * RAM or a sequence of iowrite32() calls into Sprite Registers. Reads
 * mirror the last value written so user space can sanity-check its own
 * frame builder without round-tripping through the FPGA.
 *
 * The driver is intentionally thin: all game logic and frame building
 * stays in user space (game/render.c). The kernel only owns the iomem
 * windows and the byte/word translation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "fpga_video.h"

static void __iomem *tile_base   = NULL;
static void __iomem *sprite_base = NULL;

/* Shadow copies so reads always succeed and so the driver can be loaded
 * without a real FPGA underneath (e.g. during early bring-up of the
 * Platform Designer system). */
static u8  shadow_tiles[FPGA_VIDEO_TILE_BYTES];
static u32 shadow_sprites[FPGA_VIDEO_SPRITE_COUNT];

static DEFINE_MUTEX(fpga_video_lock);

static void fpga_video_write_tile_byte(unsigned int idx, u8 value)
{
    if (idx >= FPGA_VIDEO_TILE_BYTES) {
        return;
    }
    shadow_tiles[idx] = value;
    if (tile_base) {
        iowrite8(value, tile_base + idx);
    }
}

static void fpga_video_write_sprite_word(unsigned int idx, u32 value)
{
    if (idx >= FPGA_VIDEO_SPRITE_COUNT) {
        return;
    }
    shadow_sprites[idx] = value;
    if (sprite_base) {
        iowrite32(value, sprite_base + (idx * 4));
    }
}

static ssize_t fpga_video_handle_tile_write(loff_t pos, const u8 *kbuf, size_t len)
{
    size_t i;
    unsigned int base_idx;

    if (pos < FPGA_VIDEO_TILE_OFFSET) {
        return -EINVAL;
    }
    base_idx = (unsigned int)(pos - FPGA_VIDEO_TILE_OFFSET);
    if (base_idx + len > FPGA_VIDEO_TILE_BYTES) {
        return -EINVAL;
    }
    for (i = 0; i < len; ++i) {
        fpga_video_write_tile_byte(base_idx + i, kbuf[i]);
    }
    return (ssize_t)len;
}

static ssize_t fpga_video_handle_sprite_write(loff_t pos, const u8 *kbuf, size_t len)
{
    size_t i;
    unsigned int base_idx;

    if (pos < FPGA_VIDEO_SPRITE_OFFSET) {
        return -EINVAL;
    }
    if ((pos - FPGA_VIDEO_SPRITE_OFFSET) & 0x3) {
        return -EINVAL;
    }
    if (len & 0x3) {
        return -EINVAL;
    }
    base_idx = (unsigned int)((pos - FPGA_VIDEO_SPRITE_OFFSET) >> 2);
    if (base_idx + (len >> 2) > FPGA_VIDEO_SPRITE_COUNT) {
        return -EINVAL;
    }
    for (i = 0; i < (len >> 2); ++i) {
        u32 word;
        memcpy(&word, kbuf + (i << 2), sizeof(word));
        fpga_video_write_sprite_word(base_idx + i, le32_to_cpu(word));
    }
    return (ssize_t)len;
}

static ssize_t fpga_video_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
    u8 *kbuf;
    ssize_t ret;
    loff_t end;

    if (count == 0) {
        return 0;
    }
    if (count > FPGA_VIDEO_REGION_SIZE) {
        return -EINVAL;
    }

    end = *ppos + (loff_t)count;
    if (end > FPGA_VIDEO_REGION_SIZE) {
        return -EINVAL;
    }

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) {
        return -ENOMEM;
    }
    if (copy_from_user(kbuf, buf, count)) {
        ret = -EFAULT;
        goto out;
    }

    mutex_lock(&fpga_video_lock);
    if (*ppos + count <= FPGA_VIDEO_TILE_END) {
        ret = fpga_video_handle_tile_write(*ppos, kbuf, count);
    } else if (*ppos >= FPGA_VIDEO_SPRITE_OFFSET) {
        ret = fpga_video_handle_sprite_write(*ppos, kbuf, count);
    } else {
        /* Crossing the gap or the tile/sprite boundary in one write is
         * not supported; user space should issue two separate writes. */
        ret = -EINVAL;
    }
    mutex_unlock(&fpga_video_lock);

    if (ret > 0) {
        *ppos += ret;
    }

out:
    kfree(kbuf);
    return ret;
}

static ssize_t fpga_video_read(struct file *file, char __user *buf,
                               size_t count, loff_t *ppos)
{
    u8 *kbuf;
    size_t available;
    size_t i;
    ssize_t ret;

    if (*ppos >= FPGA_VIDEO_REGION_SIZE) {
        return 0;
    }
    available = FPGA_VIDEO_REGION_SIZE - (size_t)*ppos;
    if (count > available) {
        count = available;
    }
    if (count == 0) {
        return 0;
    }

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) {
        return -ENOMEM;
    }

    mutex_lock(&fpga_video_lock);
    for (i = 0; i < count; ++i) {
        loff_t off = *ppos + (loff_t)i;
        if (off < FPGA_VIDEO_TILE_END) {
            kbuf[i] = shadow_tiles[off - FPGA_VIDEO_TILE_OFFSET];
        } else if (off >= FPGA_VIDEO_SPRITE_OFFSET &&
                   off < FPGA_VIDEO_SPRITE_END) {
            unsigned int word_idx = (unsigned int)
                ((off - FPGA_VIDEO_SPRITE_OFFSET) >> 2);
            unsigned int byte_idx = (unsigned int)
                ((off - FPGA_VIDEO_SPRITE_OFFSET) & 0x3);
            u32 word = cpu_to_le32(shadow_sprites[word_idx]);
            kbuf[i] = ((u8 *)&word)[byte_idx];
        } else {
            kbuf[i] = 0;
        }
    }
    mutex_unlock(&fpga_video_lock);

    if (copy_to_user(buf, kbuf, count)) {
        ret = -EFAULT;
    } else {
        *ppos += (loff_t)count;
        ret = (ssize_t)count;
    }

    kfree(kbuf);
    return ret;
}

static loff_t fpga_video_llseek(struct file *file, loff_t off, int whence)
{
    return fixed_size_llseek(file, off, whence, FPGA_VIDEO_REGION_SIZE);
}

static const struct file_operations fpga_video_fops = {
    .owner   = THIS_MODULE,
    .read    = fpga_video_read,
    .write   = fpga_video_write,
    .llseek  = fpga_video_llseek,
};

static struct miscdevice fpga_video_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = FPGA_VIDEO_DEVICE_NAME,
    .fops  = &fpga_video_fops,
    .mode  = 0666,
};

static int __init fpga_video_init(void)
{
    int ret;

    memset(shadow_tiles,   0, sizeof(shadow_tiles));
    memset(shadow_sprites, 0, sizeof(shadow_sprites));

    tile_base = ioremap(FPGA_VIDEO_TILE_BASE, FPGA_VIDEO_TILE_SPAN);
    if (!tile_base) {
        pr_warn("fpga_video: ioremap(tile) failed; running shadow-only\n");
    }

    sprite_base = ioremap(FPGA_VIDEO_SPRITE_BASE, FPGA_VIDEO_SPRITE_SPAN);
    if (!sprite_base) {
        pr_warn("fpga_video: ioremap(sprite) failed; running shadow-only\n");
    }

    ret = misc_register(&fpga_video_miscdev);
    if (ret) {
        pr_err("fpga_video: misc_register failed: %d\n", ret);
        if (tile_base)   iounmap(tile_base);
        if (sprite_base) iounmap(sprite_base);
        tile_base = NULL;
        sprite_base = NULL;
        return ret;
    }

    pr_info("fpga_video: loaded, /dev/%s created (%lu bytes)\n",
            FPGA_VIDEO_DEVICE_NAME, (unsigned long)FPGA_VIDEO_REGION_SIZE);
    return 0;
}

static void __exit fpga_video_exit(void)
{
    misc_deregister(&fpga_video_miscdev);

    if (tile_base) {
        iounmap(tile_base);
        tile_base = NULL;
    }
    if (sprite_base) {
        iounmap(sprite_base);
        sprite_base = NULL;
    }

    pr_info("fpga_video: unloaded\n");
}

module_init(fpga_video_init);
module_exit(fpga_video_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GridBrawl Team");
MODULE_DESCRIPTION("FPGA video driver (Tile Map RAM + Sprite Registers) for GridBrawl");
