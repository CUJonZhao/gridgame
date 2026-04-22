#ifndef AUDIO_INTERFACE_H
#define AUDIO_INTERFACE_H

#include "game.h"

#define FPGA_AUDIO_DEV_PATH "/dev/fpga_audio"

int  audio_interface_init(void);
void audio_interface_close(void);
void play_sound(audio_t audio);

#endif
