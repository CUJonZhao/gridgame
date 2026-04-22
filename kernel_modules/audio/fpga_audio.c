#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "fpga_audio.h"

static void __iomem *audio_base = NULL;
static unsigned int last_sound_id = FPGA_AUDIO_SOUND_NONE;

static void fpga_audio_hw_write(unsigned int sound_id)
{
    unsigned int value;

    value = sound_id & FPGA_AUDIO_SOUND_MASK;
    iowrite32(value, audio_base + FPGA_AUDIO_SOUND_REG_OFFSET);
    last_sound_id = value;
}

static ssize_t fpga_audio_read(struct file *file, char __user *buf,
                               size_t count, loff_t *ppos)
{
    char out[16];
    int len;

    len = scnprintf(out, sizeof(out), "%u\n", last_sound_id);
    return simple_read_from_buffer(buf, count, ppos, out, len);
}

static ssize_t fpga_audio_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
    char kbuf[32];
    size_t len;
    unsigned int sound_id;
    int ret;

    if (count == 0) {
        return 0;
    }

    len = min(count, sizeof(kbuf) - 1);

    if (copy_from_user(kbuf, buf, len)) {
        return -EFAULT;
    }

    kbuf[len] = '\0';
    strim(kbuf);

    ret = kstrtouint(kbuf, 0, &sound_id);
    if (ret) {
        return -EINVAL;
    }

    if (sound_id > FPGA_AUDIO_SOUND_WALL_BREAKS) {
        return -EINVAL;
    }

    fpga_audio_hw_write(sound_id);
    return count;
}

static const struct file_operations fpga_audio_fops = {
    .owner = THIS_MODULE,
    .read = fpga_audio_read,
    .write = fpga_audio_write,
};

static struct miscdevice fpga_audio_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = FPGA_AUDIO_DEVICE_NAME,
    .fops = &fpga_audio_fops,
    .mode = 0666,
};

static int __init fpga_audio_init(void)
{
    int ret;

    audio_base = ioremap(FPGA_AUDIO_BASE, FPGA_AUDIO_SPAN);
    if (!audio_base) {
        pr_err("fpga_audio: ioremap failed\n");
        return -ENOMEM;
    }

    ret = misc_register(&fpga_audio_miscdev);
    if (ret) {
        pr_err("fpga_audio: misc_register failed\n");
        iounmap(audio_base);
        audio_base = NULL;
        return ret;
    }

    fpga_audio_hw_write(FPGA_AUDIO_SOUND_NONE);

    pr_info("fpga_audio: loaded, /dev/%s created\n", FPGA_AUDIO_DEVICE_NAME);
    return 0;
}

static void __exit fpga_audio_exit(void)
{
    misc_deregister(&fpga_audio_miscdev);

    if (audio_base) {
        iounmap(audio_base);
        audio_base = NULL;
    }

    pr_info("fpga_audio: unloaded\n");
}

module_init(fpga_audio_init);
module_exit(fpga_audio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GridBrawl Team");
MODULE_DESCRIPTION("FPGA audio driver for GridBrawl");