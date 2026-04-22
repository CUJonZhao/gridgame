#include "audio_interface.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static int audio_fd = -1;

int audio_interface_init(void)
{
    const char *path;
    int flags;

    if (audio_fd >= 0) {
        return 0;
    }

    /* FPGA_AUDIO_DEV env var overrides the device path for VM loopback tests. */
    path = getenv("FPGA_AUDIO_DEV");
    if (path == NULL || path[0] == '\0') {
        path = FPGA_AUDIO_DEV_PATH;
    }

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
        return;
    }
    if (audio.sound_id == SOUND_NONE) {
        return;
    }

    len = snprintf(buf, sizeof(buf), "%u\n", (unsigned)audio.sound_id);
    if (len <= 0) {
        return;
    }

    written = write(audio_fd, buf, (size_t)len);
    if (written < 0) {
        static int warned = 0;
        if (!warned) {
            fprintf(stderr,
                    "audio_interface: write() failed: %s\n",
                    strerror(errno));
            warned = 1;
        }
    }
}
