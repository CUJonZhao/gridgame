#include <stdio.h>
#include <unistd.h>

#include "game.h"
#include "audio_interface.h"

static void trigger(sound_id_t id, const char *name)
{
    audio_t a = { .sound_id = id };
    printf("  play_sound(%s = %u)\n", name, (unsigned)id);
    play_sound(a);
    sleep(2);
}

int main(void)
{
    if (audio_interface_init() < 0) {
        fprintf(stderr, "audio_interface_init failed. "
                "Is fpga_audio.ko loaded? Is /dev/fpga_audio present?\n");
        return 1;
    }

    printf("Audio test starting. You should hear 4 sounds, 2 seconds apart.\n");

    trigger(SOUND_EXPLOSION,   "SOUND_EXPLOSION");
    trigger(SOUND_PLAYER_DIES, "SOUND_PLAYER_DIES");
    trigger(SOUND_WALL_BREAKS, "SOUND_WALL_BREAKS");
    trigger(SOUND_VICTORY,     "SOUND_VICTORY");

    printf("Done.\n");
    audio_interface_close();
    return 0;
}
