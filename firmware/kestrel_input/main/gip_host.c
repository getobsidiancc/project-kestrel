// Project Kestrel - Xbox One controller on the hub (RIGHT chip)
//
// Finds a wired Xbox One / Series controller (GIP interface FF/47/D0), runs its start-up
// handshake (announce -> descriptor -> power on), then relays security packets between it
// and the console, untouched. Flow follows GP2040-CE's XBOneAuthUSBListener
// (Copyright (c) 2024 OpenStickCommunity, MIT License - see components/kestrel_gip).
//
// USB channel sharing: the ESP32-S3 has 8 USB host channels and every pipe (including each
// device's control pipe) holds one while connected. Hub + mouse + keyboard + controller
// needs more than 8, so:
//   - while the console wants to authenticate, keyboards are paused and the controller is
//     claimed ("active")
//   - once the console has accepted us, the controller is released ("parked") and the
//     keyboards get their channels back. Xbox One only authenticates once per connection.
//   - if the console asks again (reboot, replug), the controller is reclaimed.

#include "gip_host.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "usb/usb_host.h"
#include "gip.h"
#include "input_usb.h"

static const char *TAG = "xbox_ctrl";

typedef struct {
    uint8_t len;
    uint8_t data[GIP_PACKET_MAX];
} pkt_t;

typedef enum { GS_NONE, GS_ACTIVE, GS_PARKED } gstate_t;

static usb_host_client_handle_t s_client;
static usb_device_handle_t s_dev;
static uint8_t s_iface, s_ep_in, s_ep_out;
static uint16_t s_mps_in = 64;
static uint16_t s_vid, s_pid;
static usb_transfer_t *s_xin, *s_xout;
static volatile bool s_in_busy, s_out_busy;

static volatile gstate_t s_gstate = GS_NONE;
static volatile bool s_ready;
static bool s_heard, s_kicked;
static uint32_t s_active_ms, s_last_proto_ms, s_auth_done_since, s_last_activate_try;
static gip_msg_t s_in, s_out;
static uint32_t s_to_console, s_to_device;

static volatile bool s_console_auth_done, s_console_want_auth;

static QueueHandle_t s_relay_q;   // console -> controller (raw auth packets)
static QueueHandle_t s_proto_q;   // our own handshake packets / ACKs

static volatile uint8_t s_new_addr[8];
static volatile uint8_t s_new_count;
static volatile bool s_gone;

static const uint8_t POWER_ON[] = { 0x06, 0x62, 0x45, 0xb8, 0x77, 0x26, 0x2c, 0x55,
                                    0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f };
static const uint8_t POWER_ON_SINGLE[] = { 0x00 };
static const uint8_t LED_ON[] = { 0x00, 0x01, 0x14 };
static const uint8_t RUMBLE_ON[] = { 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xeb };

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void send_status(void)
{
    auth_dev_status_t st;
    gip_host_status(&st);
    klink_send(KLINK_MSG_AUTH_DEV_STATUS, &st, sizeof(st));
}

static void proto_queue(const uint8_t *data, uint16_t len)
{
    pkt_t p = { .len = (uint8_t)(len > GIP_PACKET_MAX ? GIP_PACKET_MAX : len) };
    memcpy(p.data, data, p.len);
    xQueueSend(s_proto_q, &p, 0);
}

static void proto_msg(uint8_t cmd, uint8_t seq, bool internal, const uint8_t *data, uint16_t len)
{
    gip_reset(&s_out);
    gip_set_attributes(&s_out, cmd, seq, internal, false, false);
    gip_set_data(&s_out, data, len);
    gip_generate(&s_out);
    proto_queue(s_out.packet, s_out.packet_len);
}

// ---- Packets from the controller ----------------------------------------------------

static void handle_controller(const uint8_t *pkt, uint16_t len)
{
    if (s_ready && gip_is_auth_packet(pkt, len)) {
        klink_send(KLINK_MSG_GIP_TO_CONSOLE, pkt, (uint8_t)len);
        s_to_console++;
        return;
    }

    gip_parse(&s_in, pkt, len);
    if (!s_in.valid) {
        gip_reset(&s_in);
        return;
    }
    s_heard = true;

    if (gip_ack_required(&s_in)) {
        gip_generate_ack(&s_in);
        proto_queue(s_in.packet, s_in.packet_len);
    }

    switch (s_in.header.command) {
    case GIP_CMD_ANNOUNCE:
        ESP_LOGI(TAG, "controller announced itself - asking for its descriptor");
        proto_msg(GIP_CMD_DESCRIPTOR, 1, true, NULL, 0);
        break;
    case GIP_CMD_DESCRIPTOR:
        if (gip_end_of_chunk(&s_in) && !s_ready) {
            proto_msg(GIP_CMD_POWER_MODE, 2, true, POWER_ON, sizeof(POWER_ON));
            proto_msg(GIP_CMD_POWER_MODE, 3, true, POWER_ON_SINGLE, sizeof(POWER_ON_SINGLE));
            proto_msg(GIP_CMD_LED, 1, false, LED_ON, sizeof(LED_ON));
            proto_msg(GIP_CMD_RUMBLE, 1, false, RUMBLE_ON, sizeof(RUMBLE_ON));
            s_ready = true;
            ESP_LOGI(TAG, "controller powered on - ready to answer the console's auth");
            send_status();
        }
        break;
    default:
        break;   // its own input reports etc. are ignored
    }
}

// ---- USB transfers --------------------------------------------------------------------

static void in_cb(usb_transfer_t *t)
{
    s_in_busy = false;
    if (t->status == USB_TRANSFER_STATUS_COMPLETED && t->actual_num_bytes > 0) {
        handle_controller(t->data_buffer, (uint16_t)t->actual_num_bytes);
    }
    if (s_gstate == GS_ACTIVE && !s_gone && t->status != USB_TRANSFER_STATUS_NO_DEVICE &&
        t->status != USB_TRANSFER_STATUS_CANCELED) {
        if (usb_host_transfer_submit(t) == ESP_OK) s_in_busy = true;
    }
}

static void out_cb(usb_transfer_t *t)
{
    (void)t;
    s_out_busy = false;
}

static bool submit_out(const uint8_t *data, uint8_t len)
{
    memcpy(s_xout->data_buffer, data, len);
    s_xout->num_bytes = len;
    s_out_busy = true;
    if (usb_host_transfer_submit(s_xout) != ESP_OK) {
        s_out_busy = false;
        return false;
    }
    return true;
}

// ---- Claim / release ----------------------------------------------------------------------

static bool activate(void)
{
    s_last_activate_try = now_ms();
    esp_err_t err = usb_host_interface_claim(s_client, s_dev, s_iface, 0);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "no free USB channel for the controller - pausing the keyboard while the console authenticates");
        input_usb_suspend_keyboards();
        err = usb_host_interface_claim(s_client, s_dev, s_iface, 0);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "still couldn't claim the Xbox controller (%s). Try unplugging the keyboard "
                      "until the console shows the controller.", esp_err_to_name(err));
        s_gstate = GS_PARKED;
        input_usb_resume_parked();
        return false;
    }

    gip_reset(&s_in);
    gip_reset(&s_out);
    xQueueReset(s_relay_q);
    xQueueReset(s_proto_q);
    s_ready = false;
    s_heard = s_kicked = false;
    s_active_ms = now_ms();
    s_auth_done_since = 0;

    s_xin->device_handle = s_dev;
    s_xin->bEndpointAddress = s_ep_in;
    s_xin->callback = in_cb;
    s_xin->num_bytes = s_mps_in;
    s_xout->device_handle = s_dev;
    s_xout->bEndpointAddress = s_ep_out;
    s_xout->callback = out_cb;

    s_gstate = GS_ACTIVE;
    if (usb_host_transfer_submit(s_xin) == ESP_OK) s_in_busy = true;
    ESP_LOGI(TAG, "Xbox controller claimed - starting it up");
    send_status();
    return true;
}

static void release_interface(void)
{
    s_gstate = GS_PARKED;   // stops IN resubmits
    s_ready = false;
    usb_host_endpoint_halt(s_dev, s_ep_in);
    usb_host_endpoint_flush(s_dev, s_ep_in);
    usb_host_endpoint_halt(s_dev, s_ep_out);
    usb_host_endpoint_flush(s_dev, s_ep_out);
    for (int i = 0; i < 50 && (s_in_busy || s_out_busy); i++) {
        usb_host_client_handle_events(s_client, pdMS_TO_TICKS(10));
    }
    s_in_busy = s_out_busy = false;
    usb_host_interface_release(s_client, s_dev, s_iface);
}

static void park(void)
{
    release_interface();
    ESP_LOGI(TAG, "console accepted the adapter - released the Xbox controller, keyboard back on "
                  "(relayed %lu packets to console, %lu to controller)",
             (unsigned long)s_to_console, (unsigned long)s_to_device);
    input_usb_resume_parked();
    send_status();
}

static void try_attach(uint8_t addr)
{
    if (s_gstate != GS_NONE) return;   // one auth controller at a time

    usb_device_handle_t dev;
    if (usb_host_device_open(s_client, addr, &dev) != ESP_OK) return;

    const usb_device_desc_t *dd;
    const usb_config_desc_t *cd;
    if (usb_host_get_device_descriptor(dev, &dd) != ESP_OK ||
        usb_host_get_active_config_descriptor(dev, &cd) != ESP_OK) {
        usb_host_device_close(s_client, dev);
        return;
    }

    int off = 0;
    const usb_standard_desc_t *s = (const usb_standard_desc_t *)cd;
    const usb_intf_desc_t *gip = NULL;
    while ((s = usb_parse_next_descriptor_of_type(s, cd->wTotalLength, USB_B_DESCRIPTOR_TYPE_INTERFACE, &off)) != NULL) {
        const usb_intf_desc_t *i = (const usb_intf_desc_t *)s;
        if (i->bInterfaceClass == 0xFF && i->bInterfaceSubClass == 0x47 && i->bInterfaceProtocol == 0xD0 &&
            i->bAlternateSetting == 0) {
            gip = i;
            break;
        }
    }
    if (!gip) {
        usb_host_device_close(s_client, dev);
        return;
    }

    uint8_t ep_in = 0, ep_out = 0;
    int ioff = 0;
    const usb_intf_desc_t *intf = usb_parse_interface_descriptor(cd, gip->bInterfaceNumber, 0, &ioff);
    for (int e = 0; intf && e < intf->bNumEndpoints; e++) {
        int eoff = ioff;
        const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(intf, e, cd->wTotalLength, &eoff);
        if (!ep) continue;
        if (USB_EP_DESC_GET_EP_DIR(ep)) { ep_in = ep->bEndpointAddress; s_mps_in = USB_EP_DESC_GET_MPS(ep); }
        else ep_out = ep->bEndpointAddress;
    }
    if (!ep_in || !ep_out) {
        ESP_LOGW(TAG, "Xbox controller has no usable endpoints");
        usb_host_device_close(s_client, dev);
        return;
    }

    s_dev = dev;
    s_iface = gip->bInterfaceNumber;
    s_ep_in = ep_in;
    s_ep_out = ep_out;
    s_vid = dd->idVendor;
    s_pid = dd->idProduct;
    s_gone = false;
    s_gstate = GS_PARKED;
    ESP_LOGI(TAG, "Xbox controller found (%04x:%04x)", s_vid, s_pid);

    if (!s_console_auth_done) activate();   // console may want it any moment
    else send_status();
}

static void detach(void)
{
    if (s_gstate == GS_ACTIVE) release_interface();
    usb_host_device_close(s_client, s_dev);
    s_dev = NULL;
    s_gstate = GS_NONE;
    s_ready = false;
    ESP_LOGW(TAG, "Xbox controller unplugged");
    input_usb_resume_parked();
    send_status();
}

static void client_cb(const usb_host_client_event_msg_t *msg, void *arg)
{
    (void)arg;
    if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        if (s_new_count < sizeof(s_new_addr)) s_new_addr[s_new_count++] = msg->new_dev.address;
    } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        if (s_gstate != GS_NONE && msg->dev_gone.dev_hdl == s_dev) s_gone = true;
    }
}

static void gip_task(void *arg)
{
    (void)arg;
    const usb_host_client_config_t cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 8,
        .async = { .client_event_callback = client_cb, .callback_arg = NULL },
    };
    ESP_ERROR_CHECK(usb_host_client_register(&cfg, &s_client));
    ESP_ERROR_CHECK(usb_host_transfer_alloc(64, 0, &s_xin));
    ESP_ERROR_CHECK(usb_host_transfer_alloc(64, 0, &s_xout));

    uint32_t last_status = 0;
    for (;;) {
        usb_host_client_handle_events(s_client, 1);

        while (s_new_count) try_attach(s_new_addr[--s_new_count]);
        if (s_gone) {
            s_gone = false;
            detach();
        }

        uint32_t now = now_ms();
        if (now - last_status > 500) {
            last_status = now;
            send_status();
        }

        if (s_gstate == GS_PARKED) {
            // Console wants to authenticate again (reboot / replug) - take the controller back
            if (s_console_want_auth && !s_console_auth_done && now - s_last_activate_try > 5000) {
                ESP_LOGI(TAG, "console wants to authenticate - reclaiming the Xbox controller");
                activate();
            }
            continue;
        }
        if (s_gstate != GS_ACTIVE) continue;

        // Console has accepted us: hand the USB channels back to the keyboard
        if (s_console_auth_done) {
            if (s_auth_done_since == 0) s_auth_done_since = now;
            if (now - s_auth_done_since > 1000) {
                park();
                continue;
            }
        } else {
            s_auth_done_since = 0;
        }

        // Some pads (PowerA, original Xbox One pad) never announce: ask for the descriptor
        if (!s_heard && !s_kicked && now - s_active_ms > 250) {
            s_kicked = true;
            static const uint8_t desc_req[] = { GIP_CMD_DESCRIPTOR, 0x20, 0x01, 0x00 };
            proto_queue(desc_req, sizeof(desc_req));
        }

        if (!s_out_busy) {
            pkt_t p;
            if (xQueueReceive(s_relay_q, &p, 0) == pdTRUE) {
                if (submit_out(p.data, p.len)) s_to_device++;
            } else if (now - s_last_proto_ms > 15 && xQueuePeek(s_proto_q, &p, 0) == pdTRUE) {
                if (submit_out(p.data, p.len)) {
                    xQueueReceive(s_proto_q, &p, 0);
                    s_last_proto_ms = now;
                }
            }
        }
    }
}

void gip_host_start(void)
{
    s_relay_q = xQueueCreate(32, sizeof(pkt_t));
    s_proto_q = xQueueCreate(24, sizeof(pkt_t));
    xTaskCreatePinnedToCore(gip_task, "gip_host", 6144, NULL, 9, NULL, 0);
}

void gip_host_from_console(const uint8_t *data, uint8_t len)
{
    if (s_gstate != GS_ACTIVE || len == 0 || len > GIP_PACKET_MAX) return;
    pkt_t p = { .len = len };
    memcpy(p.data, data, len);
    xQueueSend(s_relay_q, &p, 0);
}

void gip_host_set_console_state(bool auth_done, bool want_auth)
{
    s_console_auth_done = auth_done;
    s_console_want_auth = want_auth;
}

void gip_host_status(auth_dev_status_t *out)
{
    out->mounted = (s_gstate != GS_NONE);
    out->ready = (s_gstate == GS_ACTIVE) && s_ready;
    out->vid = s_vid;
    out->pid = s_pid;
}
