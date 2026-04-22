#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "game.h"
#include "usbcontroller.h"

static volatile int keep_running = 1;

static void on_sigint(int sig)
{
    (void)sig;
    keep_running = 0;
}

static void print_state(int p, const controller_state_t *s)
{
    printf("  P%d: U=%u D=%u L=%u R=%u Bomb=%u Rst=%u",
           p + 1, s->up, s->down, s->left, s->right, s->bomb, s->restart);
}

int main(void)
{
    signal(SIGINT, on_sigint);

    int opened = usbcontroller_init();
    if (opened < 0) {
        fprintf(stderr, "usbcontroller_init failed fatally.\n");
        return 1;
    }
    printf("Opened %d controller(s). Press Ctrl-C to quit.\n", opened);

    controller_state_t s[PLAYER_COUNT];
    while (keep_running) {
        usbcontroller_get_state(s);
        printf("\r");
        for (int p = 0; p < PLAYER_COUNT; p++) {
            print_state(p, &s[p]);
        }
        fflush(stdout);
        usleep(50 * 1000);
    }

    printf("\nShutting down.\n");
    usbcontroller_close();
    return 0;
}
