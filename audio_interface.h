#ifndef AUDIO_INTERFACE_H
#define AUDIO_INTERFACE_H

#include "game.h"

/*
 * User-space wrapper around /dev/fpga_audio.
 *
 * The kernel driver (kernel_modules/audio/fpga_audio.c) exposes an ASCII
 * interface: writing the decimal string form of a sound_id (e.g. "1\n")
 * triggers the corresponding sound. This file hides that detail from the
 * rest of the game code: game_logic just calls play_sound(audio_t) from
 * game.h, and we translate it into a write() on the device node.
 */

#define FPGA_AUDIO_DEV_PATH "/dev/fpga_audio"

/*
 * Open /dev/fpga_audio and cache the fd.
 * Returns 0 on success, -1 on failure (errno set by open()).
 * Must be called once from demo.c before the game loop starts.
 */
int audio_interface_init(void);

/*
 * Close the cached fd. Safe to call even if init failed.
 */
void audio_interface_close(void);

/*
 * Implementation of play_sound() declared in game.h.
 * Writes the sound_id to /dev/fpga_audio. Silently no-ops if the device
 * has not been opened or if sound_id == SOUND_NONE.
 */
void play_sound(audio_t audio);

#endif
