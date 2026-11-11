// Project Kestrel - Xbox One controller (GIP) USB device, LEFT chip
//
// Protocol flow and descriptors follow GP2040-CE's XBOneDriver
// (Copyright (c) 2024 OpenStickCommunity, MIT License - see components/kestrel_gip).
//
//   1. 0.5 s after the console configures us, send ANNOUNCE
//   2. console asks for our GIP descriptor -> send it in fragments, waiting for ACKs
//   3. console powers us on, then runs the security handshake. Every auth packet is
//      relayed raw over the link to the real Xbox controller plugged into the RIGHT
//      chip's hub, and its answers come back the same way.
//   4. console says "01 00" (authenticated) -> start sending input reports.

#include "output.h"

#if KESTREL_OUTPUT_MODE == OUTPUT_XBOX_ONE

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "tinyusb.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "gip.h"

static const char *TAG = "xbone";

// ---- Descriptors -------------------------------------------------------------

#define GIP_VID 0x0E6F   // PDP (licensed Xbox One wired controller), as used by GP2040-CE
#define GIP_PID 0x02A4

static const tusb_desc_device_t s_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF,
    .bDeviceSubClass    = 0x47,
    .bDeviceProtocol    = 0xD0,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = GIP_VID,
    .idProduct          = GIP_PID,
    .bcdDevice          = 0x0101,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

static const uint8_t s_config_desc[] = {
    0x09, TUSB_DESC_CONFIGURATION, 0x20, 0x00, 0x01, 0x01, 0x00, 0xA0, 0xFA,
    0x09, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, 0xFF, 0x47, 0xD0, 0x00,
    0x07, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, 0x40, 0x00, 0x01,
    0x07, TUSB_DESC_ENDPOINT, 0x01, TUSB_XFER_INTERRUPT, 0x40, 0x00, 0x01,
};
_Static_assert(sizeof(s_config_desc) == 0x20, "config descriptor length");

static char s_serial[13];
static const char *s_strings[] = {
    (const char[]){ 0x09, 0x04 },
    "Project Kestrel",
    "Kestrel Adapter (Xbox One)",
    s_serial,
};

static const char SECURITY_METHOD[] =
    "Xbox Security Method 3, Version 1.00, \xa9 2005 Microsoft Corporation. All rights reserved.";
static const char MS_OS_STRING[] = "MSFT100\x20";   // vendor code 0x20

// Windows asks for this to bind its Xbox One driver ("XGIP10")
typedef struct __attribute__((packed)) {
    uint32_t total_length;
    uint16_t version;
    uint16_t index;
    uint8_t  total_sections;
    uint8_t  reserved[7];
    uint8_t  first_interface;
    uint8_t  reserved2;
    uint8_t  compatible_id[8];
    uint8_t  sub_compatible_id[8];
    uint8_t  reserved3[6];
} os_compat_id_t;

static const os_compat_id_t s_compat_id = {
    .total_length = sizeof(os_compat_id_t),
    .version = 0x0100,
    .index = 0x0004,
    .total_sections = 1,
    .first_interface = 0,
    .reserved2 = 0x01,
    .compatible_id = { 'X', 'G', 'I', 'P', '1', '0', 0, 0 },
};

// Our GIP descriptor (gamepad-only profile, 182 bytes) - from GP2040-CE
static const uint8_t s_gip_descriptor[] = {
    0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB6, 0x00,
    0x77, 0x00, 0x16, 0x00, 0x1B, 0x00, 0x1C, 0x00,
    0x23, 0x00, 0x29, 0x00, 0x46, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x00, 0x02, 0x00, 0x00,
    0x06, 0x01, 0x02, 0x03, 0x04, 0x06, 0x07, 0x05,
    0x01, 0x04, 0x05, 0x06, 0x0A,
    0x01, 0x1A, 0x00, 0x57, 0x69, 0x6E, 0x64, 0x6F,
    0x77, 0x73, 0x2E, 0x58, 0x62, 0x6F, 0x78, 0x2E,
    0x49, 0x6E, 0x70, 0x75, 0x74, 0x2E, 0x47, 0x61,
    0x6D, 0x65, 0x70, 0x61, 0x64,
    0x03, 0x56, 0xFF, 0x76, 0x97, 0xFD, 0x9B, 0x81,
    0x45, 0xAD, 0x45, 0xB6, 0x45, 0xBB, 0xA5, 0x26,
    0xD6, 0x2C, 0x40, 0x2E, 0x08, 0xDF, 0x07, 0xE1,
    0x45, 0xA5, 0xAB, 0xA3, 0x12, 0x7A, 0xF1, 0x97,
    0xB5, 0xE7, 0x1F, 0xF3, 0xB8, 0x86, 0x73, 0xE9,
    0x40, 0xA9, 0xF8, 0x2F, 0x21, 0x26, 0x3A, 0xCF,
    0xB7,
    0x02,
    0x17, 0x00, 0x20, 0x0E, 0x00, 0x01, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x17, 0x00, 0x09, 0x3C, 0x00, 0x01, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
_Static_assert(sizeof(s_gip_descriptor) == 0xB6, "GIP descriptor length");

static uint8_t s_announce[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // device ID (from MAC)
    GIP_VID & 0xFF, GIP_VID >> 8, GIP_PID & 0xFF, GIP_PID >> 8,
    0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,   // firmware version
    0x01, 0x00,                                       // hardware version
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00,               // RF, security, GIP versions
};

static const uint8_t AUTH_OK[] = { 0x01, 0x00 };

// ---- State ---------------------------------------------------------------------

typedef struct {
    uint8_t len;
    uint8_t data[GIP_PACKET_MAX];
} pkt_t;

typedef enum { ST_NOT_READY, ST_READY_ANNOUNCE, ST_WAIT_DESC_REQ, ST_SEND_DESC, ST_SETUP_AUTH, ST_AUTH_DONE } st_t;
static const char *ST_NAMES[] = { "not ready", "announcing", "waiting for descriptor request",
                                  "sending descriptor", "authenticating", "authenticated" };

static uint8_t s_ep_in, s_ep_out;
static CFG_TUSB_MEM_ALIGN uint8_t s_in_buf[GIP_PACKET_MAX];
static CFG_TUSB_MEM_ALIGN uint8_t s_out_buf[GIP_PACKET_MAX];
static volatile bool s_evt_open, s_evt_reset;

static QueueHandle_t s_rx_q;      // console -> us (filled in the TinyUSB task)
static QueueHandle_t s_relay_q;   // real controller -> console (filled by the link task)
static QueueHandle_t s_tx_q;      // protocol packets waiting to go to the console

static st_t s_state = ST_NOT_READY;
static gip_msg_t s_in, s_out;
static uint32_t s_t_announce, s_last_queue_send, s_ack_t0, s_power_on_ms, s_last_keepalive;
static bool s_waiting_ack, s_powered_on, s_auth_completed, s_input_sent, s_passthrough_active, s_any_auth_seen;
static bool s_console_auth_orphaned, s_reconnect_pending, s_warned_no_controller, s_auth_by_fallback;
static uint32_t s_reconnect_at;
static uint8_t s_input_seq, s_keepalive_seq = 1, s_vk_seq;
static bool s_guide_down;
static uint8_t s_last_input[18];
static uint32_t s_relayed_to_dev, s_relayed_to_console;

static volatile bool s_auth_dev_mounted, s_auth_dev_ready;

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// ---- String descriptor hook (index 4 and 0xEE) ------------------------------------
// esp_tinyusb owns tud_descriptor_string_cb; the linker wraps it (see CMakeLists.txt).

const uint16_t *__real_tud_descriptor_string_cb(uint8_t index, uint16_t langid);

const uint16_t *__wrap_tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static uint16_t buf[128];
    const char *s = NULL;
    if (index == 4) s = SECURITY_METHOD;
    else if (index == 0xEE) s = MS_OS_STRING;
    if (!s) return __real_tud_descriptor_string_cb(index, langid);

    size_t n = strlen(s);
    if (n > 126) n = 126;
    for (size_t i = 0; i < n; i++) buf[1 + i] = (uint8_t)s[i];
    buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * n + 2));
    return buf;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *req)
{
    if (stage != CONTROL_STAGE_SETUP) return true;
    if (req->bmRequestType == 0xC0 && req->bRequest == 0x20 && req->wValue == 0 && req->wIndex == 0x0004) {
        return tud_control_xfer(rhport, req, (void *)&s_compat_id, sizeof(s_compat_id));
    }
    return false;
}

// ---- Class driver (runs in the TinyUSB task) --------------------------------------

static void gd_init(void) { s_ep_in = s_ep_out = 0; }
static bool gd_deinit(void) { return true; }

static void gd_reset(uint8_t rhport)
{
    (void)rhport;
    s_ep_in = s_ep_out = 0;
    s_evt_reset = true;
}

static uint16_t gd_open(uint8_t rhport, tusb_desc_interface_t const *itf, uint16_t max_len)
{
    TU_VERIFY(itf->bInterfaceClass == 0xFF && itf->bInterfaceSubClass == 0x47 && itf->bInterfaceProtocol == 0xD0, 0);

    uint16_t drv_len = 0;
    uint8_t const *p = (uint8_t const *)itf;
    uint8_t const *end = p + max_len;
    drv_len += tu_desc_len(p);
    p = tu_desc_next(p);
    while (p < end && tu_desc_type(p) != TUSB_DESC_INTERFACE) {
        if (tu_desc_type(p) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)p;
            TU_ASSERT(usbd_edpt_open(rhport, ep), 0);
            if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN) s_ep_in = ep->bEndpointAddress;
            else s_ep_out = ep->bEndpointAddress;
        }
        drv_len += tu_desc_len(p);
        p = tu_desc_next(p);
    }
    if (s_ep_out) usbd_edpt_xfer(rhport, s_ep_out, s_out_buf, sizeof(s_out_buf), false);
    s_evt_open = true;
    return drv_len;
}

static bool gd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *req)
{
    (void)rhport; (void)stage; (void)req;
    return false;
}

static bool gd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    if (ep_addr == s_ep_out) {
        if (result == XFER_RESULT_SUCCESS && xferred_bytes > 0 && xferred_bytes <= GIP_PACKET_MAX) {
            pkt_t pkt = { .len = (uint8_t)xferred_bytes };
            memcpy(pkt.data, s_out_buf, xferred_bytes);
            xQueueSend(s_rx_q, &pkt, 0);
        }
        usbd_edpt_xfer(rhport, s_ep_out, s_out_buf, sizeof(s_out_buf), false);
    }
    return true;
}

static const usbd_class_driver_t s_driver = {
    .name = "XBONE",
    .init = gd_init,
    .deinit = gd_deinit,
    .reset = gd_reset,
    .open = gd_open,
    .control_xfer_cb = gd_control_xfer_cb,
    .xfer_cb = gd_xfer_cb,
    .xfer_isr = NULL,
    .sof = NULL,
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &s_driver;
}

// ---- Sending -----------------------------------------------------------------------

static bool send_usb(const uint8_t *data, uint16_t len)
{
    if (len > sizeof(s_in_buf) || !tud_ready() || s_ep_in == 0) return false;
    if (!usbd_edpt_claim(0, s_ep_in)) return false;
    memcpy(s_in_buf, data, len);
    if (usbd_edpt_xfer(0, s_ep_in, s_in_buf, len, false)) return true;
    usbd_edpt_release(0, s_ep_in);
    return false;
}

static void queue_tx(const uint8_t *data, uint16_t len)
{
    pkt_t pkt = { .len = (uint8_t)(len > GIP_PACKET_MAX ? GIP_PACKET_MAX : len) };
    memcpy(pkt.data, data, pkt.len);
    if (xQueueSend(s_tx_q, &pkt, 0) != pdTRUE) ESP_LOGW(TAG, "tx queue full");
}

static void set_state(st_t st)
{
    if (st != s_state) ESP_LOGI(TAG, "state: %s", ST_NAMES[st]);
    s_state = st;
}

static void start_descriptor(uint8_t seq)
{
    gip_reset(&s_out);
    gip_set_attributes(&s_out, GIP_CMD_DESCRIPTOR, seq, true, true, false);
    gip_set_data(&s_out, s_gip_descriptor, sizeof(s_gip_descriptor));
    set_state(ST_SEND_DESC);
}

// ---- Packets from the console ----------------------------------------------------------

static void handle_console(const uint8_t *pkt, uint16_t len)
{
    gip_parse(&s_in, pkt, len);
    const uint8_t cmd = pkt[0];
    const bool is_auth_cmd = (cmd == GIP_CMD_AUTH || cmd == GIP_CMD_FINAL_AUTH);

    if (is_auth_cmd) {
        s_any_auth_seen = true;
        if (s_auth_by_fallback) {
            // We assumed "no auth needed" (Windows) but the console asks after all: take it back
            // so input waits for the real handshake through the controller on the hub.
            ESP_LOGW(TAG, "console asked for authentication late - running the real handshake");
            s_auth_by_fallback = false;
            s_auth_completed = false;
            s_input_sent = false;
            set_state(ST_SETUP_AUTH);
        }
        if (!s_auth_dev_ready) s_console_auth_orphaned = true;
    }

    if (s_auth_dev_ready && gip_is_auth_packet(pkt, len)) {
        // Raw relay: the real controller answers and ACKs everything itself.
        if (!s_passthrough_active) ESP_LOGI(TAG, "console started the security handshake - relaying to your controller");
        s_passthrough_active = true;
        if (is_auth_cmd && s_in.valid && !s_in.header.chunked && s_in.data_len == sizeof(AUTH_OK) &&
            memcmp(s_in.data, AUTH_OK, sizeof(AUTH_OK)) == 0) {
            s_auth_completed = true;
            s_input_sent = false;
            set_state(ST_AUTH_DONE);
            ESP_LOGI(TAG, "*** Xbox accepted the controller - input is live ***");
        }
        klink_send(KLINK_MSG_GIP_TO_DEVICE, pkt, (uint8_t)len);
        s_relayed_to_dev++;
        gip_reset(&s_in);
        return;
    }

    if (!s_in.valid) return;

    if (gip_ack_required(&s_in)) {
        gip_generate_ack(&s_in);
        queue_tx(s_in.packet, s_in.packet_len);
    }

    switch (s_in.header.command) {
    case GIP_CMD_ACK:
        s_waiting_ack = false;
        break;
    case GIP_CMD_DESCRIPTOR:
        start_descriptor(s_in.header.sequence);
        break;
    case GIP_CMD_POWER_MODE:
        if (!s_powered_on) {
            s_powered_on = true;
            s_power_on_ms = now_ms();
            ESP_LOGI(TAG, "host powered the controller on");
        }
        break;
    case GIP_CMD_LED:
        if (s_state == ST_WAIT_DESC_REQ) start_descriptor(s_in.header.sequence);
        break;
    case GIP_CMD_AUTH:
    case GIP_CMD_FINAL_AUTH:
        if (!s_warned_no_controller) {
            s_warned_no_controller = true;
            ESP_LOGW(TAG, "console wants to authenticate, but no Xbox controller is ready on the hub. "
                          "Plug one in - the adapter will reconnect automatically.");
        }
        break;
    default:
        break;
    }
}

// ---- Input reports -----------------------------------------------------------------------

static void build_input(const pad_state_t *pad, uint8_t *r)
{
    memset(r, 0, 18);
    r[0] = GIP_CMD_INPUT;
    r[1] = 0x00;
    r[2] = 0;      // sequence filled by caller
    r[3] = 14;
    uint16_t b = pad->buttons;
    r[4] = (uint8_t)(((b & PAD_START) ? 0x04 : 0) | ((b & PAD_BACK) ? 0x08 : 0) |
                     ((b & PAD_A) ? 0x10 : 0) | ((b & PAD_B) ? 0x20 : 0) |
                     ((b & PAD_X) ? 0x40 : 0) | ((b & PAD_Y) ? 0x80 : 0));
    r[5] = (uint8_t)(((b & PAD_DPAD_UP) ? 0x01 : 0) | ((b & PAD_DPAD_DOWN) ? 0x02 : 0) |
                     ((b & PAD_DPAD_LEFT) ? 0x04 : 0) | ((b & PAD_DPAD_RIGHT) ? 0x08 : 0) |
                     ((b & PAD_LB) ? 0x10 : 0) | ((b & PAD_RB) ? 0x20 : 0) |
                     ((b & PAD_LS) ? 0x40 : 0) | ((b & PAD_RS) ? 0x80 : 0));
    uint16_t lt = (uint16_t)((pad->lt << 2) | (pad->lt >> 6));   // 8-bit -> 10-bit
    uint16_t rt = (uint16_t)((pad->rt << 2) | (pad->rt >> 6));
    r[6] = (uint8_t)lt;  r[7] = (uint8_t)(lt >> 8);
    r[8] = (uint8_t)rt;  r[9] = (uint8_t)(rt >> 8);
    int16_t axes[4] = { pad->lx, pad->ly, pad->rx, pad->ry };
    for (int i = 0; i < 4; i++) {
        r[10 + i * 2] = (uint8_t)(axes[i] & 0xFF);
        r[11 + i * 2] = (uint8_t)((uint16_t)axes[i] >> 8);
    }
}

static inline uint8_t next_seq(uint8_t s)
{
    s++;
    return s ? s : 1;
}

static void send_gameplay(const pad_state_t *pad, uint32_t now)
{
    if (uxQueueMessagesWaiting(s_tx_q) > 0) return;   // protocol traffic first

    if (!s_auth_completed) {
        if (!s_input_sent) {   // one neutral report while authentication is pending
            pad_state_t neutral = {0};
            uint8_t r[18];
            build_input(&neutral, r);
            r[2] = next_seq(s_input_seq);
            if (send_usb(r, sizeof(r))) {
                s_input_seq = r[2];
                memcpy(s_last_input, r, sizeof(r));
                s_input_sent = true;
            }
        }
        return;
    }

    // Keep-alive every 15 s
    if (now - s_last_keepalive > 15000) {
        uint8_t k[8] = { GIP_CMD_KEEPALIVE, 0x20, s_keepalive_seq, 0x04, 0x80, 0x00, 0x00, 0x00 };
        if (send_usb(k, sizeof(k))) {
            s_last_keepalive = now;
            s_keepalive_seq = next_seq(s_keepalive_seq);
        }
        return;
    }

    // Xbox button is a separate "virtual key" message
    bool guide = (pad->buttons & PAD_GUIDE) != 0;
    if (guide != s_guide_down) {
        uint8_t seq = next_seq(s_vk_seq);
        uint8_t v[6] = { GIP_CMD_VIRTUAL_KEY, 0x20, seq, 0x02, guide ? 0x01 : 0x00, 0x5B };
        if (send_usb(v, sizeof(v))) {
            s_vk_seq = seq;
            s_guide_down = guide;
        }
        return;
    }

    uint8_t r[18];
    build_input(pad, r);
    if (s_input_sent && memcmp(&r[4], &s_last_input[4], 14) == 0) return;   // nothing changed
    r[2] = next_seq(s_input_seq);
    if (send_usb(r, sizeof(r))) {
        s_input_seq = r[2];
        memcpy(s_last_input, r, sizeof(r));
        s_input_sent = true;
    }
}

// ---- Public API ------------------------------------------------------------------------------

void output_start(void)
{
    s_rx_q = xQueueCreate(32, sizeof(pkt_t));
    s_relay_q = xQueueCreate(32, sizeof(pkt_t));
    s_tx_q = xQueueCreate(24, sizeof(pkt_t));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    memcpy(s_announce, mac, 6);
    snprintf(s_serial, sizeof(s_serial), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    const tinyusb_config_t cfg = {
        .device_descriptor = &s_device_desc,
        .string_descriptor = s_strings,
        .string_descriptor_count = sizeof(s_strings) / sizeof(s_strings[0]),
        .external_phy = false,
        .configuration_descriptor = s_config_desc,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));
    ESP_LOGI(TAG, "Xbox One controller mode ready");
}

bool output_mounted(void)
{
    return tud_mounted() && s_ep_in != 0;
}

void output_link_rx(uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (type == KLINK_MSG_GIP_TO_CONSOLE && len > 0 && len <= GIP_PACKET_MAX) {
        pkt_t pkt = { .len = len };
        memcpy(pkt.data, payload, len);
        xQueueSend(s_relay_q, &pkt, 0);
    } else if (type == KLINK_MSG_AUTH_DEV_STATUS && len >= sizeof(auth_dev_status_t)) {
        const auth_dev_status_t *st = (const auth_dev_status_t *)payload;
        if (st->ready && !s_auth_dev_ready) ESP_LOGI(TAG, "Xbox controller on the hub is ready for auth (%04x:%04x)", st->vid, st->pid);
        if (!st->mounted && s_auth_dev_mounted) ESP_LOGW(TAG, "Xbox controller unplugged from the hub");
        s_auth_dev_mounted = st->mounted;
        s_auth_dev_ready = st->ready;
    }
}

void output_poll(const pad_state_t *pad)
{
    const uint32_t now = now_ms();
    pkt_t pkt;

    if (s_evt_reset) {
        s_evt_reset = false;
        set_state(ST_NOT_READY);
    }
    if (s_evt_open) {
        s_evt_open = false;
        gip_reset(&s_in);
        gip_reset(&s_out);
        xQueueReset(s_tx_q);
        xQueueReset(s_rx_q);
        s_waiting_ack = s_powered_on = s_auth_completed = s_input_sent = false;
        s_passthrough_active = s_any_auth_seen = s_warned_no_controller = false;
        s_auth_by_fallback = false;
        s_guide_down = false;
        s_t_announce = now;
        s_last_keepalive = now;
        set_state(ST_READY_ANNOUNCE);
    }

    // Controller plugged in after the console already gave up waiting: reconnect so it
    // starts authentication again.
    if (s_auth_dev_ready && s_console_auth_orphaned && !s_auth_completed && !s_reconnect_pending) {
        ESP_LOGI(TAG, "controller ready - reconnecting to the console to restart authentication");
        s_console_auth_orphaned = false;
        set_state(ST_NOT_READY);
        tud_disconnect();
        s_reconnect_pending = true;
        s_reconnect_at = now + 250;
    }
    if (s_reconnect_pending && (int32_t)(now - s_reconnect_at) >= 0) {
        s_reconnect_pending = false;
        tud_connect();
    }

    while (xQueueReceive(s_rx_q, &pkt, 0) == pdTRUE) handle_console(pkt.data, pkt.len);
    while (xQueueReceive(s_relay_q, &pkt, 0) == pdTRUE) {
        queue_tx(pkt.data, pkt.len);
        s_relayed_to_console++;
    }

    // Some hosts never ask for authentication; treat it as accepted after 8 s. If the console
    // asks later anyway, handle_console() takes this back (s_auth_by_fallback).
    if (s_powered_on && !s_any_auth_seen && !s_auth_completed && now - s_power_on_ms > 8000) {
        s_auth_completed = true;
        s_auth_by_fallback = true;
        s_input_sent = false;
        set_state(ST_AUTH_DONE);
        ESP_LOGI(TAG, "host didn't ask for authentication - input is live");
    }

    // Protocol state machine
    bool blocked = false;
    if (s_waiting_ack) {
        if (now - s_ack_t0 < 2000) blocked = true;
        else s_waiting_ack = false;
    }
    if (!blocked) {
        switch (s_state) {
        case ST_READY_ANNOUNCE:
            if (now - s_t_announce > 500) {
                gip_reset(&s_out);
                gip_set_attributes(&s_out, GIP_CMD_ANNOUNCE, 1, true, false, false);
                gip_set_data(&s_out, s_announce, sizeof(s_announce));
                gip_generate(&s_out);
                queue_tx(s_out.packet, s_out.packet_len);
                set_state(ST_WAIT_DESC_REQ);
            }
            break;
        case ST_SEND_DESC:
            gip_generate(&s_out);
            queue_tx(s_out.packet, s_out.packet_len);
            if (gip_end_of_chunk(&s_out)) set_state(s_auth_completed ? ST_AUTH_DONE : ST_SETUP_AUTH);
            if (s_out.header.needs_ack) {
                s_waiting_ack = true;
                s_ack_t0 = now;
            }
            break;
        default:
            break;
        }
    }

    // Send queued protocol packets (fast while relaying auth, otherwise every 35 ms)
    if (uxQueueMessagesWaiting(s_tx_q) > 0 && (s_passthrough_active || now - s_last_queue_send > 35)) {
        if (xQueuePeek(s_tx_q, &pkt, 0) == pdTRUE && send_usb(pkt.data, pkt.len)) {
            xQueueReceive(s_tx_q, &pkt, 0);
            s_last_queue_send = now;
        }
    }

    if (output_mounted()) send_gameplay(pad, now);
}

bool output_auth_done(void)
{
    return output_mounted() && s_auth_completed;
}

bool output_wants_auth(void)
{
    return (output_mounted() && !s_auth_completed &&
            (s_state == ST_SETUP_AUTH || s_state == ST_SEND_DESC || s_any_auth_seen)) ||
           s_console_auth_orphaned;
}

const char *output_status(void)
{
    static char buf[160];
    snprintf(buf, sizeof(buf), "xbox_one=%s auth_ctrl=%s relayed=%lu/%lu",
             ST_NAMES[s_state],
             s_auth_dev_ready ? "ready" : (s_auth_dev_mounted ? "starting" : "none"),
             (unsigned long)s_relayed_to_dev, (unsigned long)s_relayed_to_console);
    return buf;
}

#endif // OUTPUT_XBOX_ONE
