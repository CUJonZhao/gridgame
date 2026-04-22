#ifndef USBCONTROLLER_H
#define USBCONTROLLER_H

#include "game.h"

int  usbcontroller_init(void);
void usbcontroller_get_state(controller_state_t out[PLAYER_COUNT]);
void usbcontroller_close(void);

#endif
