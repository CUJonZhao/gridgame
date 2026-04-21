#include "audio_interface.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/*
 * User-space side of the audio path.
 *
 * Kernel contract (see kernel_modules/audio/fpga_audio.c):
 *   - Open /dev/fpga_audio.
 *   - write() an ASCII decimal string like "1\n" to trigger sound_id 1.
 *   - Driver parses with kstrtouint, rejects ids > FPGA_AUDIO_SOUND_WALL_BREAKS.
 *
 * We keep the fd module-static so play_sound() stays cheap: one write()
 * per call, no open/close on the hot path.
 */

static int audio_fd = -1;

int audio_interface_init(void)
{
    const char *path;
    int flags;

    if (audio_fd >= 0) {
        /* Already initialized. */
        return 0;
    }

    /*
     * Allow the target path to be overridden via env var so we can run
     * loopback tests in a VM against a regular file or FIFO without
     * touching the code. Example:
     *     FPGA_AUDIO_DEV=/tmp/fake_audio ./audio_test
     * On the real board this is unset and we fall back to the device node.
     */
    path = getenv("FPGA_AUDIO_DEV");
    if (path == NULL || path[0] == '\0') {
        path = FPGA_AUDIO_DEV_PATH;
    }

    /* Use O_CREAT|O_APPEND for the mock-file case; on a real char device
     * O_CREAT is harmless (kernel ignores it), and O_APPEND lets multiple
     * test writes accumulate in the mock file for inspection. */
    flags = O_WRONLY | O_CREAT | O_APPEND;
    audio_fd = open(path, flags, 0666);
    if (audio_fd < 0) {
        fprintf(stderr,
                "audio_interface_init: open(%s) failed: %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}

void audio_interface_close(void)
{
    if (audio_fd >= 0) {
        close(audio_fd);
        audio_fd = -1;
    }
}

void play_sound(audio_t audio)
{
    char buf[8];
    int  len;
    ssize_t written;

    if (audio_fd < 0) {
        /* audio_interface_init() was never called or failed. */
        return;
    }

    if (audio.sound_id == SOUND_NONE) {
        return;
    }

    /* sound_id fits in 4 bits (0..4 today, <=15 by driver's mask),
     * so "N\n\0" is at most 3 bytes. 8 is plenty. */
    len = snprintf(buf, sizeof(buf), "%u\n", (unsigned)audio.sound_id);
    if (len <= 0) {
        return;
    }

    written = write(audio_fd, buf, (size_t)len);
    if (written < 0) {
        /* Don't spam stderr from the game loop; one-shot warning is enough.
         * If this ever fires in a real run, check dmesg for driver errors. */
        static int warned = 0;
        if (!warned) {
            fprintf(stderr,
                    "audio_interface: write() failed: %s\n",
                    strerror(errno));
            warned = 1;
        }
    }
}
