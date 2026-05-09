/*
 * gridbrawl main loop.
 *
 * Glues together the four subsystems:
 *   - usbcontroller   : reads the two HID gamepads (background poll thread)
 *   - game_logic      : pure-C state machine, advanced once per game tick
 *   - audio_interface : writes sound IDs to /dev/fpga_audio
 *   - render          : pushes tile RAM + sprite registers to /dev/fpga_video
 *
 * The loop runs at GAME_TICK_HZ (defined in game.h) using absolute-time
 * scheduling on CLOCK_MONOTONIC so we don't drift.  Signals are handled
 * cleanly so the device files and the libusb context get released.
 *
 * Set GRIDBRAWL_DEBUG=1 to print player positions and scores roughly
 * once per second.  GRIDBRAWL_NO_AUDIO / NO_USB / NO_VIDEO short-circuit
 * the matching subsystems so we can iterate on the loop without the
 * full DE1-SoC stack in place.
 */

#include "game.h"
#include "usbcontroller.h"
#include "audio_interface.h"
#include "video_interface.h"
#include "render.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NS_PER_SEC      1000000000L
#define TICK_NS         (NS_PER_SEC / GAME_TICK_HZ)

static volatile sig_atomic_t g_running = 1;

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void timespec_add_ns(struct timespec *t, long ns)
{
    t->tv_nsec += ns;
    while (t->tv_nsec >= NS_PER_SEC) {
        t->tv_nsec -= NS_PER_SEC;
        t->tv_sec  += 1;
    }
}

static void emit_sound(sound_id_t id)
{
    audio_t a;
    if (id == SOUND_NONE) {
        return;
    }
    a.sound_id = id;
    play_sound(a);
}

static void debug_dump(const game_state_t *g, unsigned tick)
{
    static int enabled  = -1;
    static unsigned last_tick = 0;

    if (enabled == -1) {
        const char *env = getenv("GRIDBRAWL_DEBUG");
        enabled = (env && env[0] != '\0' && env[0] != '0') ? 1 : 0;
    }
    if (!enabled) {
        return;
    }
    if (tick - last_tick < (unsigned)GAME_TICK_HZ) {
        return;
    }
    last_tick = tick;

    fprintf(stderr,
            "[tick %5u] P1 row=%u col=%u alive=%u score=%u | "
            "P2 row=%u col=%u alive=%u score=%u | game_over=%u winner=%d\n",
            tick,
            g->players[0].row, g->players[0].col,
            g->players[0].alive, g->players[0].score,
            g->players[1].row, g->players[1].col,
            g->players[1].alive, g->players[1].score,
            g->game_over, (int)g->winner);
}

int main(int argc, char **argv)
{
    game_state_t          game;
    controller_state_t    inputs[PLAYER_COUNT];
    struct timespec       next_tick;
    unsigned              tick = 0;
    int                   usb_opened;
    int                   audio_ok;
    int                   video_ok;
    int                   skip_audio;
    int                   skip_usb;
    int                   skip_video;

    (void)argc;
    (void)argv;

    install_signal_handlers();

    skip_audio = (getenv("GRIDBRAWL_NO_AUDIO") != NULL);
    skip_usb   = (getenv("GRIDBRAWL_NO_USB")   != NULL);
    skip_video = (getenv("GRIDBRAWL_NO_VIDEO") != NULL);

    if (!skip_audio) {
        audio_ok = (audio_interface_init() == 0);
        if (!audio_ok) {
            fprintf(stderr,
                    "main: audio_interface_init failed (continuing silently)\n");
        }
    } else {
        audio_ok = 0;
    }

    if (!skip_video) {
        video_ok = (video_interface_init() == 0);
        if (!video_ok) {
            fprintf(stderr,
                    "main: video_interface_init failed (continuing without rendering)\n");
        }
    } else {
        video_ok = 0;
    }

    if (!skip_usb) {
        usb_opened = usbcontroller_init();
        if (usb_opened < 0) {
            fprintf(stderr,
                    "main: usbcontroller_init failed; running with no input\n");
            usb_opened = 0;
        } else if (usb_opened == 0) {
            fprintf(stderr,
                    "main: no controllers opened; running with no input\n");
        } else {
            fprintf(stderr,
                    "main: %d controller(s) attached\n", usb_opened);
        }
    } else {
        usb_opened = 0;
    }

    game_init(&game);
    render_init();

    if (clock_gettime(CLOCK_MONOTONIC, &next_tick) != 0) {
        fprintf(stderr, "main: clock_gettime failed: %s\n", strerror(errno));
        if (audio_ok) audio_interface_close();
        if (video_ok) video_interface_close();
        if (!skip_usb) usbcontroller_close();
        return 1;
    }

    while (g_running) {
        memset(inputs, 0, sizeof(inputs));
        if (usb_opened > 0) {
            usbcontroller_get_state(inputs);
        }

        game_step(&game, inputs);

        render_frame(&game);

        if (audio_ok) {
            emit_sound(game.pending_sound_1);
            emit_sound(game.pending_sound_2);
        }

        debug_dump(&game, tick);
        ++tick;

        timespec_add_ns(&next_tick, TICK_NS);
        /* If the loop body takes longer than a tick, catch up rather than
         * piling up missed deadlines. */
        {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (now.tv_sec > next_tick.tv_sec ||
                (now.tv_sec == next_tick.tv_sec && now.tv_nsec > next_tick.tv_nsec)) {
                next_tick = now;
                continue;
            }
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_tick, NULL);
    }

    fprintf(stderr, "main: shutting down\n");

    if (!skip_usb) usbcontroller_close();
    if (audio_ok)  audio_interface_close();
    if (video_ok)  video_interface_close();
    return 0;
}
