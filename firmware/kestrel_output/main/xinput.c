// Project Kestrel - XInput (Xbox 360 wired controller) USB device
//
// Windows' built-in driver (xusb22) binds to this, so no driver install is needed and
// any game or gamepad tester sees a normal Xbox controller. This is the stage 1 output;
// stage 2 adds the Xbox One protocol (GIP) next to it for the console.
//
// XInput uses interrupt endpoints and a vendor class (0xFF/0x5D/0x01), which TinyUSB's
// built-in vendor class doesn't handle, so this file registers its own class driver.

#include "output.h"

#if KESTREL_OUTPUT_MODE == OUTPUT_XINPUT

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "tinyusb.h"
#include "tusb.h"
#include "device/usbd_pvt.h"

static const char *TAG = "xinput";

// ---- Descriptors -------------------------------------------------------------------

static const tusb_desc_device_t s_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF,
    .bDeviceSubClass    = 0xFF,
    .bDeviceProtocol    = 0xFF,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x045E,   // Microsoft
    .idProduct          = 0x028E,   // Xbox 360 Controller (what Windows' xusb22 driver binds to)
    .bcdDevice          = 0x0114,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

#define XINPUT_EP_IN   0x81
#define XINPUT_EP_OUT  0x01
#define XINPUT_EP_SIZE 32

static const uint8_t s_config_desc[] = {
    // Configuration
    0x09, TUSB_DESC_CONFIGURATION, 0x30, 0x00, 0x01, 0x01, 0x00, 0x80, 0xFA,
    // Interface 0: vendor 0xFF / 0x5D / 0x01 (XInput gamepad)
    0x09, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, 0xFF, 0x5D, 0x01, 0x00,
    // XInput class-specific descriptor (type 0x21), as sent by a wired 360 pad
    0x10, 0x21, 0x10, 0x01, 0x01, 0x24, 0x81, 0x14, 0x03, 0x00, 0x03, 0x13, 0x01, 0x00, 0x03, 0x00,
    // Endpoint IN: interrupt, 32 bytes, every 1 ms
    0x07, TUSB_DESC_ENDPOINT, XINPUT_EP_IN, TUSB_XFER_INTERRUPT, XINPUT_EP_SIZE, 0x00, 0x01,
    // Endpoint OUT: interrupt, 32 bytes, every 8 ms (rumble / LEDs)
    0x07, TUSB_DESC_ENDPOINT, XINPUT_EP_OUT, TUSB_XFER_INTERRUPT, XINPUT_EP_SIZE, 0x00, 0x08,
};
_Static_assert(sizeof(s_config_desc) == 0x30, "config descriptor length mismatch");

static const char *s_strings[] = {
    (const char[]){ 0x09, 0x04 },   // 0: English (US)
    "Project Kestrel",              // 1: manufacturer
    "Kestrel Adapter",              // 2: product
    "KST-0001",                     // 3: serial
};

// ---- Class driver ------------------------------------------------------------------

static uint8_t s_ep_in;
static uint8_t s_ep_out;
static CFG_TUSB_MEM_ALIGN uint8_t s_in_buf[20];
static CFG_TUSB_MEM_ALIGN uint8_t s_out_buf[XINPUT_EP_SIZE];

static void xi_init(void)
{
    s_ep_in = s_ep_out = 0;
}

static bool xi_deinit(void)
{
    return true;
}

static void xi_reset(uint8_t rhport)
{
    (void)rhport;
    s_ep_in = s_ep_out = 0;
}

static uint16_t xi_open(uint8_t rhport, tusb_desc_interface_t const *itf, uint16_t max_len)
{
    TU_VERIFY(itf->bInterfaceClass == 0xFF &&
              itf->bInterfaceSubClass == 0x5D &&
              itf->bInterfaceProtocol == 0x01, 0);

    uint16_t drv_len = 0;
    uint8_t const *p = (uint8_t const *)itf;
    uint8_t const *end = p + max_len;

    // Interface descriptor
    drv_len += tu_desc_len(p);
    p = tu_desc_next(p);

    // Everything up to the next interface belongs to us (0x21 descriptor + endpoints)
    while (p < end && tu_desc_type(p) != TUSB_DESC_INTERFACE) {
        if (tu_desc_type(p) == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)p;
            TU_ASSERT(usbd_edpt_open(rhport, ep), 0);
            if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN) {
                s_ep_in = ep->bEndpointAddress;
            } else {
                s_ep_out = ep->bEndpointAddress;
            }
        }
        drv_len += tu_desc_len(p);
        p = tu_desc_next(p);
    }

    // Listen for rumble / LED packets from the host (ignored for now)
    if (s_ep_out) {
        usbd_edpt_xfer(rhport, s_ep_out, s_out_buf, sizeof(s_out_buf), false);
    }
    return drv_len;
}

static bool xi_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *req)
{
    (void)rhport;
    (void)stage;
    (void)req;
    // No class/vendor control requests are needed; stalling them is what most clones do.
    return false;
}

static bool xi_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)result;
    (void)xferred_bytes;
    if (ep_addr == s_ep_out) {
        usbd_edpt_xfer(rhport, s_ep_out, s_out_buf, sizeof(s_out_buf), false);
    }
    return true;
}

static const usbd_class_driver_t s_xinput_driver = {
    .name            = "XINPUT",
    .init            = xi_init,
    .deinit          = xi_deinit,
    .reset           = xi_reset,
    .open            = xi_open,
    .control_xfer_cb = xi_control_xfer_cb,
    .xfer_cb         = xi_xfer_cb,
    .xfer_isr        = NULL,
    .sof             = NULL,
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &s_xinput_driver;
}

// ---- Public API ----------------------------------------------------------------------

// The Xbox One mode hooks string descriptors; XInput needs nothing extra.
const uint16_t *__real_tud_descriptor_string_cb(uint8_t index, uint16_t langid);
const uint16_t *__wrap_tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    return __real_tud_descriptor_string_cb(index, langid);
}

static bool xinput_send(const pad_state_t *pad);

void output_start(void)
{
    const tinyusb_config_t cfg = {
        .device_descriptor = &s_device_desc,
        .string_descriptor = s_strings,
        .string_descriptor_count = sizeof(s_strings) / sizeof(s_strings[0]),
        .external_phy = false,
        .configuration_descriptor = s_config_desc,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));
    ESP_LOGI(TAG, "XInput controller ready");
}

bool output_mounted(void)
{
    return tud_mounted() && s_ep_in != 0;
}

void output_link_rx(uint8_t type, const uint8_t *payload, uint8_t len)
{
    (void)type; (void)payload; (void)len;
}

const char *output_status(void)
{
    return "xinput";
}

bool output_auth_done(void) { return output_mounted(); }
bool output_wants_auth(void) { return false; }

// Send on change, and at least every 8 ms.
void output_poll(const pad_state_t *pad)
{
    static pad_state_t last;
    static int64_t last_send_us;
    static bool sent_once;
    int64_t now = esp_timer_get_time();
    if (!output_mounted()) {
        sent_once = false;
        return;
    }
    if (!sent_once || memcmp(pad, &last, sizeof(last)) != 0 || now - last_send_us > 8000) {
        if (xinput_send(pad)) {
            last = *pad;
            last_send_us = now;
            sent_once = true;
        }
    }
}

static bool xinput_send(const pad_state_t *pad)
{
    const uint8_t rhport = 0;
    if (!tud_ready() || s_ep_in == 0) return false;
    if (!usbd_edpt_claim(rhport, s_ep_in)) return false;   // previous report still in flight

    uint8_t *r = s_in_buf;
    memset(r, 0, sizeof(s_in_buf));
    r[0] = 0x00;                         // message type: input report
    r[1] = 0x14;                         // length: 20 bytes
    r[2] = (uint8_t)(pad->buttons & 0xFF);
    r[3] = (uint8_t)(pad->buttons >> 8);
    r[4] = pad->lt;
    r[5] = pad->rt;
    int16_t v[4] = { pad->lx, pad->ly, pad->rx, pad->ry };
    for (int i = 0; i < 4; i++) {
        r[6 + i * 2] = (uint8_t)(v[i] & 0xFF);
        r[7 + i * 2] = (uint8_t)((uint16_t)v[i] >> 8);
    }

    return usbd_edpt_xfer(rhport, s_ep_in, s_in_buf, sizeof(s_in_buf), false);
}

#endif // OUTPUT_XINPUT
