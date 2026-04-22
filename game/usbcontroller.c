#include "usbcontroller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include <libusb-1.0/libusb.h>

/* ====== HID REPORT LAYOUT -- ADJUST AFTER `lsusb -v -d VID:PID` ====== */
#define USBCTRL_INTERFACE            0
#define USBCTRL_ENDPOINT_IN       0x81
#define USBCTRL_REPORT_LEN           8
#define USBCTRL_POLL_TIMEOUT_MS     20

#define USBCTRL_BYTE_XAXIS           0
#define USBCTRL_BYTE_YAXIS           1
#define USBCTRL_AXIS_LOW          0x40
#define USBCTRL_AXIS_HIGH         0xC0

#define USBCTRL_BYTE_BUTTONS_A       5
#define USBCTRL_MASK_BOMB         0x01
#define USBCTRL_BYTE_BUTTONS_B       6
#define USBCTRL_MASK_RESTART      0x02

/* ====== FILL IN WITH `lsusb` OUTPUT FOR YOUR GAMEPADS ====== */
static const struct { uint16_t vid; uint16_t pid; } target_ids[PLAYER_COUNT] = {
    { 0x0000, 0x0000 },  /* player 1 */
    { 0x0000, 0x0000 },  /* player 2 */
};
/* ============================================================ */

struct controller_slot {
    libusb_device_handle *handle;
    int                   iface_claimed;
    int                   kernel_driver_was_attached;
};

static libusb_context        *usb_ctx             = NULL;
static struct controller_slot slots[PLAYER_COUNT] = { {0} };
static controller_state_t     state_cache[PLAYER_COUNT];
static pthread_mutex_t        state_lock          = PTHREAD_MUTEX_INITIALIZER;
static pthread_t              poll_thread;
static volatile int           poll_thread_run     = 0;
static int                    poll_thread_started = 0;

static void parse_report(const uint8_t *buf, int len, controller_state_t *out)
{
    memset(out, 0, sizeof(*out));
    if (len < USBCTRL_REPORT_LEN) {
        return;
    }

    if (buf[USBCTRL_BYTE_XAXIS] < USBCTRL_AXIS_LOW)  out->left  = 1;
    if (buf[USBCTRL_BYTE_XAXIS] > USBCTRL_AXIS_HIGH) out->right = 1;
    if (buf[USBCTRL_BYTE_YAXIS] < USBCTRL_AXIS_LOW)  out->up    = 1;
    if (buf[USBCTRL_BYTE_YAXIS] > USBCTRL_AXIS_HIGH) out->down  = 1;

    if (buf[USBCTRL_BYTE_BUTTONS_A] & USBCTRL_MASK_BOMB)    out->bomb    = 1;
    if (buf[USBCTRL_BYTE_BUTTONS_B] & USBCTRL_MASK_RESTART) out->restart = 1;
}

static libusb_device_handle *open_controller(uint16_t vid, uint16_t pid,
                                             int *kernel_was_attached)
{
    libusb_device_handle *h = NULL;
    int rc;

    *kernel_was_attached = 0;

    if (vid == 0 && pid == 0) {
        libusb_device **list = NULL;
        ssize_t n = libusb_get_device_list(usb_ctx, &list);
        if (n < 0) return NULL;
        for (ssize_t i = 0; i < n; i++) {
            struct libusb_device_descriptor desc;
            if (libusb_get_device_descriptor(list[i], &desc) != 0) continue;
            if (libusb_open(list[i], &h) == 0) {
                break;
            }
            h = NULL;
        }
        libusb_free_device_list(list, 1);
    } else {
        h = libusb_open_device_with_vid_pid(usb_ctx, vid, pid);
    }

    if (!h) return NULL;

    if (libusb_kernel_driver_active(h, USBCTRL_INTERFACE) == 1) {
        rc = libusb_detach_kernel_driver(h, USBCTRL_INTERFACE);
        if (rc != 0) {
            fprintf(stderr, "usbcontroller: detach_kernel_driver failed: %s\n",
                    libusb_error_name(rc));
            libusb_close(h);
            return NULL;
        }
        *kernel_was_attached = 1;
    }

    rc = libusb_claim_interface(h, USBCTRL_INTERFACE);
    if (rc != 0) {
        fprintf(stderr, "usbcontroller: claim_interface failed: %s\n",
                libusb_error_name(rc));
        if (*kernel_was_attached)
            libusb_attach_kernel_driver(h, USBCTRL_INTERFACE);
        libusb_close(h);
        return NULL;
    }

    return h;
}

static void close_controller(struct controller_slot *s)
{
    if (!s->handle) return;
    if (s->iface_claimed) {
        libusb_release_interface(s->handle, USBCTRL_INTERFACE);
    }
    if (s->kernel_driver_was_attached) {
        libusb_attach_kernel_driver(s->handle, USBCTRL_INTERFACE);
    }
    libusb_close(s->handle);
    s->handle                     = NULL;
    s->iface_claimed              = 0;
    s->kernel_driver_was_attached = 0;
}

static void *poll_loop(void *arg)
{
    (void)arg;
    uint8_t buf[USBCTRL_REPORT_LEN];
    controller_state_t local[PLAYER_COUNT];

    while (poll_thread_run) {
        memset(local, 0, sizeof(local));

        for (int p = 0; p < PLAYER_COUNT; p++) {
            if (!slots[p].handle) continue;

            int transferred = 0;
            int rc = libusb_interrupt_transfer(slots[p].handle,
                                               USBCTRL_ENDPOINT_IN,
                                               buf, sizeof(buf),
                                               &transferred,
                                               USBCTRL_POLL_TIMEOUT_MS);
            if (rc == 0 && transferred == USBCTRL_REPORT_LEN) {
                parse_report(buf, transferred, &local[p]);
            } else if (rc == LIBUSB_ERROR_TIMEOUT) {
                pthread_mutex_lock(&state_lock);
                local[p] = state_cache[p];
                pthread_mutex_unlock(&state_lock);
            }
        }

        pthread_mutex_lock(&state_lock);
        memcpy(state_cache, local, sizeof(state_cache));
        pthread_mutex_unlock(&state_lock);
    }
    return NULL;
}

int usbcontroller_init(void)
{
    int rc;
    int opened = 0;

    memset(state_cache, 0, sizeof(state_cache));
    memset(slots,       0, sizeof(slots));

    rc = libusb_init(&usb_ctx);
    if (rc != 0) {
        fprintf(stderr, "usbcontroller: libusb_init failed: %s\n",
                libusb_error_name(rc));
        return -1;
    }

    for (int p = 0; p < PLAYER_COUNT; p++) {
        int kattached = 0;
        libusb_device_handle *h = open_controller(target_ids[p].vid,
                                                  target_ids[p].pid,
                                                  &kattached);
        if (h) {
            slots[p].handle                     = h;
            slots[p].iface_claimed              = 1;
            slots[p].kernel_driver_was_attached = kattached;
            opened++;
        } else {
            fprintf(stderr,
                    "usbcontroller: player %d: no controller opened "
                    "(vid=%04x pid=%04x)\n",
                    p + 1, target_ids[p].vid, target_ids[p].pid);
        }
    }

    if (opened > 0) {
        poll_thread_run = 1;
        rc = pthread_create(&poll_thread, NULL, poll_loop, NULL);
        if (rc != 0) {
            fprintf(stderr, "usbcontroller: pthread_create failed: %s\n",
                    strerror(rc));
            poll_thread_run = 0;
            for (int p = 0; p < PLAYER_COUNT; p++) close_controller(&slots[p]);
            libusb_exit(usb_ctx);
            usb_ctx = NULL;
            return -1;
        }
        poll_thread_started = 1;
    }

    return opened;
}

void usbcontroller_get_state(controller_state_t out[PLAYER_COUNT])
{
    pthread_mutex_lock(&state_lock);
    memcpy(out, state_cache, sizeof(controller_state_t) * PLAYER_COUNT);
    pthread_mutex_unlock(&state_lock);
}

void usbcontroller_close(void)
{
    if (poll_thread_started) {
        poll_thread_run = 0;
        pthread_join(poll_thread, NULL);
        poll_thread_started = 0;
    }
    for (int p = 0; p < PLAYER_COUNT; p++) {
        close_controller(&slots[p]);
    }
    if (usb_ctx) {
        libusb_exit(usb_ctx);
        usb_ctx = NULL;
    }
}
