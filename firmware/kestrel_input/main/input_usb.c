// Project Kestrel - USB host input (mouse + keyboard through a hub)
//
// Uses ESP-IDF's USB Host Library and the espressif/usb_host_hid class driver.
// Keyboards and mice are used in their full "report protocol" mode when their report
// descriptor can be decoded (hidparse.c): every key counts (n-key rollover), 16-bit mouse
// movement, scroll wheel. Otherwise they fall back to the fixed 6-key "boot protocol".
// Extra non-boot interfaces are kept only if they carry a full key bitmap or mouse X/Y;
// the rest are released straight away to keep the S3's 8 USB host channels free.

#include "input_usb.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "hidparse.h"

static const char *TAG = "input";

#define MAX_DEVICES 6

typedef struct {
    hid_host_device_handle_t handle;
    uint8_t addr;           // USB device address
    uint8_t proto;          // role: HID_PROTOCOL_KEYBOARD / HID_PROTOCOL_MOUSE
    bool full;              // report protocol with a decoded layout (else boot protocol)
    hp_layout_t lay;
    uint8_t buttons;        // mouse buttons from this device
    uint8_t keys[32];       // key bitmap from this device
} dev_slot_t;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static dev_slot_t s_dev[MAX_DEVICES];
static int32_t s_acc_dx, s_acc_dy, s_acc_wheel;

typedef enum { EV_DRIVER, EV_SUSPEND_KB, EV_RESUME } app_event_kind_t;

typedef struct {
    app_event_kind_t kind;
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
} app_event_t;

static QueueHandle_t s_event_q;
static SemaphoreHandle_t s_suspend_done;

// Interfaces we closed (or couldn't open) to free USB host channels, reopened later.
typedef struct {
    hid_host_device_handle_t handle;
    uint8_t addr;
} parked_t;
static parked_t s_parked[MAX_DEVICES];
static int s_parked_count;

static void park(hid_host_device_handle_t h, uint8_t addr)
{
    taskENTER_CRITICAL(&s_lock);
    bool known = false;
    for (int i = 0; i < s_parked_count; i++) known |= (s_parked[i].handle == h);
    if (!known && s_parked_count < MAX_DEVICES) {
        s_parked[s_parked_count].handle = h;
        s_parked[s_parked_count].addr = addr;
        s_parked_count++;
    }
    taskEXIT_CRITICAL(&s_lock);
}

static dev_slot_t *slot_find(hid_host_device_handle_t h)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (s_dev[i].handle == h) return &s_dev[i];
    }
    return NULL;
}

static void handle_keyboard_report(dev_slot_t *d, const uint8_t *data, size_t len)
{
    if (len < 8) return;
    uint8_t keys[32] = {0};
    // byte 0: modifier bits -> usages 0xE0..0xE7
    for (int b = 0; b < 8; b++) {
        if (data[0] & (1 << b)) {
            uint8_t u = (uint8_t)(0xE0 + b);
            keys[u >> 3] |= (uint8_t)(1 << (u & 7));
        }
    }
    // bytes 2..7: up to six pressed keys (0x01-0x03 are rollover/error codes)
    for (size_t i = 2; i < 8; i++) {
        uint8_t u = data[i];
        if (u > 0x03) keys[u >> 3] |= (uint8_t)(1 << (u & 7));
    }
    taskENTER_CRITICAL(&s_lock);
    memcpy(d->keys, keys, sizeof(keys));
    taskEXIT_CRITICAL(&s_lock);
}

static void handle_mouse_report(dev_slot_t *d, const uint8_t *data, size_t len)
{
    if (len < 3) return;
    int8_t dx = (int8_t)data[1];
    int8_t dy = (int8_t)data[2];
    int8_t wh = (len >= 4) ? (int8_t)data[3] : 0;
    taskENTER_CRITICAL(&s_lock);
    d->buttons = data[0];
    s_acc_dx += dx;
    s_acc_dy += dy;
    s_acc_wheel += wh;
    taskEXIT_CRITICAL(&s_lock);
}

// Full (report protocol) mode: decode with the layout from the report descriptor.
static void handle_full_report(dev_slot_t *d, const uint8_t *data, size_t len)
{
    const hp_layout_t *L = &d->lay;
    uint8_t rid = 0;
    const uint8_t *p = data;
    size_t plen = len;
    if (L->uses_report_ids) {
        if (len < 1) return;
        rid = data[0];
        p = data + 1;
        plen = len - 1;
    }

    uint8_t keys[32];
    memcpy(keys, d->keys, sizeof(keys));
    bool key_fields = false;
    int btn = -1;
    int32_t dx = 0, dy = 0, wh = 0;

    // pass 1: clear every key this report describes
    for (int i = 0; i < L->nfields; i++) {
        const hp_field_t *f = &L->f[i];
        if (f->report_id != rid || (f->kind != HPF_KEY_BITMAP && f->kind != HPF_KEY_ARRAY)) continue;
        key_fields = true;
        for (uint32_t u = f->usage_min; u <= f->usage_max && u <= 0xFF; u++) keys[u >> 3] &= (uint8_t)~(1u << (u & 7));
    }
    // pass 2: set what's pressed, read mouse values
    for (int i = 0; i < L->nfields; i++) {
        const hp_field_t *f = &L->f[i];
        if (f->report_id != rid) continue;
        switch (f->kind) {
        case HPF_KEY_BITMAP:
            for (int k = 0; k < f->count; k++) {
                uint32_t u = f->usage_min + k;
                if (u <= 0xFF && hp_extract(p, plen, (uint16_t)(f->bitoff + k * f->size), 1, false)) {
                    keys[u >> 3] |= (uint8_t)(1u << (u & 7));
                }
            }
            break;
        case HPF_KEY_ARRAY:
            for (int k = 0; k < f->count; k++) {
                uint32_t u = f->usage_min + (uint32_t)hp_extract(p, plen, (uint16_t)(f->bitoff + k * f->size), f->size, false);
                if (u > 0x03 && u <= 0xFF) keys[u >> 3] |= (uint8_t)(1u << (u & 7));
            }
            break;
        case HPF_BUTTONS:
            btn = 0;
            for (int k = 0; k < f->count && k < 8; k++) {
                if (hp_extract(p, plen, (uint16_t)(f->bitoff + k * f->size), 1, false)) btn |= 1 << k;
            }
            break;
        case HPF_X:     dx = hp_extract(p, plen, f->bitoff, f->size, f->is_signed); break;
        case HPF_Y:     dy = hp_extract(p, plen, f->bitoff, f->size, f->is_signed); break;
        case HPF_WHEEL: wh = hp_extract(p, plen, f->bitoff, f->size, f->is_signed); break;
        default: break;
        }
    }

    taskENTER_CRITICAL(&s_lock);
    if (key_fields) memcpy(d->keys, keys, sizeof(keys));
    if (btn >= 0) d->buttons = (uint8_t)btn;
    s_acc_dx += dx;
    s_acc_dy += dy;
    s_acc_wheel += wh;
    taskEXIT_CRITICAL(&s_lock);
}

static void (*s_report_cb)(void);
void input_usb_set_report_cb(void (*cb)(void)) { s_report_cb = cb; }

static void interface_cb(hid_host_device_handle_t handle, const hid_host_interface_event_t event, void *arg)
{
    (void)arg;
    uint8_t data[64];
    size_t len = 0;
    dev_slot_t *d = slot_find(handle);

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        if (!d) break;
        if (hid_host_device_get_raw_input_report_data(handle, data, sizeof(data), &len) != ESP_OK) break;
        if (d->full) {
            handle_full_report(d, data, len);
        } else if (d->proto == HID_PROTOCOL_KEYBOARD) {
            handle_keyboard_report(d, data, len);
        } else if (d->proto == HID_PROTOCOL_MOUSE) {
            handle_mouse_report(d, data, len);
        }
        if (s_report_cb) s_report_cb();
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "%s disconnected", (d && d->proto == HID_PROTOCOL_MOUSE) ? "mouse" : "keyboard");
        taskENTER_CRITICAL(&s_lock);
        for (int i = 0; i < s_parked_count; i++) {   // unplugged while paused
            if (s_parked[i].handle == handle) {
                s_parked[i] = s_parked[--s_parked_count];
                break;
            }
        }
        taskEXIT_CRITICAL(&s_lock);
        if (d) {
            taskENTER_CRITICAL(&s_lock);
            memset(d, 0, sizeof(*d));
            taskEXIT_CRITICAL(&s_lock);
        }
        hid_host_device_close(handle);
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "transfer error");
        break;
    default:
        break;
    }
}

// Is there already a keyboard interface on this device (optionally one with every key)?
static bool keyboard_on_addr(uint8_t addr, bool nkro_only)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        const dev_slot_t *s = &s_dev[i];
        if (s->handle && s->addr == addr && s->proto == HID_PROTOCOL_KEYBOARD &&
            (!nkro_only || (s->full && s->lay.key_count >= 32))) return true;
    }
    return false;
}

static bool open_device(hid_host_device_handle_t handle, bool quiet)
{
    hid_host_dev_params_t p;
    if (hid_host_device_get_params(handle, &p) != ESP_OK) return false;

    const bool boot = p.sub_class == HID_SUBCLASS_BOOT_INTERFACE &&
                      (p.proto == HID_PROTOCOL_KEYBOARD || p.proto == HID_PROTOCOL_MOUSE);

    // A mouse's extra "keyboard" interface (macro keys) isn't worth a USB channel: skip it.
    if (boot && p.proto == HID_PROTOCOL_KEYBOARD) {
        for (int i = 0; i < MAX_DEVICES; i++) {
            if (s_dev[i].handle && s_dev[i].addr == p.addr && s_dev[i].proto == HID_PROTOCOL_MOUSE) {
                ESP_LOGI(TAG, "ignoring keyboard interface %d on addr %d (it's the mouse's macro keys)", p.iface_num, p.addr);
                return true;
            }
        }
    }

    dev_slot_t *d = slot_find(NULL);
    if (!d) {
        ESP_LOGW(TAG, "too many devices, ignoring");
        return true;
    }

    const hid_host_device_config_t cfg = {
        .callback = interface_cb,
        .callback_arg = NULL,
    };
    if (hid_host_device_open(handle, &cfg) != ESP_OK) {
        if (!boot) return true;   // extra interface and no channel for it: skip quietly
        if (!quiet) ESP_LOGW(TAG, "%s on addr %d waiting for a free USB channel",
                             p.proto == HID_PROTOCOL_MOUSE ? "mouse" : "keyboard", p.addr);
        park(handle, p.addr);
        return false;
    }

    // Decode the report descriptor to use the device's full mode
    static hp_layout_t lay;
    size_t dl = 0;
    const uint8_t *desc = hid_host_get_report_descriptor(handle, &dl);
    const bool parsed = desc && hidparse(desc, dl, &lay);
    const bool kb_full = parsed && lay.key_count >= 6;
    const bool mouse_full = parsed && lay.has_xy;

    uint8_t role;
    bool full;
    if (boot && p.proto == HID_PROTOCOL_KEYBOARD) { role = HID_PROTOCOL_KEYBOARD; full = kb_full; }
    else if (boot) { role = HID_PROTOCOL_MOUSE; full = mouse_full; }
    else if (kb_full && lay.key_count >= 32 && !keyboard_on_addr(p.addr, true)) {
        role = HID_PROTOCOL_KEYBOARD; full = true;   // separate n-key rollover interface
    } else {
        // media keys, vendor or a keyboard's macro "mouse": release its channel
        hid_host_device_pause(handle);   // media keys / vendor interface: release its channel
        ESP_LOGI(TAG, "ignoring HID interface %d on addr %d (no keys or mouse movement)", p.iface_num, p.addr);
        return true;
    }

    taskENTER_CRITICAL(&s_lock);
    memset(d, 0, sizeof(*d));
    d->handle = handle;
    d->addr = p.addr;
    d->proto = role;
    d->full = full;
    if (full) d->lay = lay;
    taskEXIT_CRITICAL(&s_lock);

    if (boot) hid_class_request_set_protocol(handle, full ? HID_REPORT_PROTOCOL_REPORT : HID_REPORT_PROTOCOL_BOOT);
    if (role == HID_PROTOCOL_KEYBOARD) hid_class_request_set_idle(handle, 0, 0);
    hid_host_device_start(handle);

    if (role == HID_PROTOCOL_KEYBOARD) {
        ESP_LOGI(TAG, "keyboard connected (addr %d, iface %d) - %s", p.addr, p.iface_num,
                 full ? (lay.key_count >= 32 ? "full mode, every key counts" : "full mode, 6-key") : "boot mode, 6-key");
    } else {
        ESP_LOGI(TAG, "mouse connected (addr %d, iface %d) - %s%s%s", p.addr, p.iface_num,
                 full ? "full mode, " : "boot mode, 8-bit",
                 full ? (lay.xy_bits >= 16 ? "16-bit" : "12-bit or less") : "",
                 full && lay.has_wheel ? ", wheel" : "");
    }
    return true;
}

// Startup check of the descriptor decoder against two known descriptors.
static void hidparse_selftest(void)
{
    static const uint8_t kb[] = {   // standard boot keyboard (HID spec appendix)
        0x05,0x01,0x09,0x06,0xA1,0x01,0x05,0x07,0x19,0xE0,0x29,0xE7,0x15,0x00,0x25,0x01,0x75,0x01,
        0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x01,0x95,0x05,0x75,0x01,0x05,0x08,0x19,0x01,
        0x29,0x05,0x91,0x02,0x95,0x01,0x75,0x03,0x91,0x01,0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65,
        0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,0xC0 };
    static const uint8_t ms[] = {   // gaming mouse: report ID 2, 5 buttons, 16-bit X/Y, wheel
        0x05,0x01,0x09,0x02,0xA1,0x01,0x85,0x02,0x09,0x01,0xA1,0x00,0x05,0x09,0x19,0x01,0x29,0x05,
        0x15,0x00,0x25,0x01,0x95,0x05,0x75,0x01,0x81,0x02,0x95,0x01,0x75,0x03,0x81,0x01,0x05,0x01,
        0x16,0x01,0x80,0x26,0xFF,0x7F,0x75,0x10,0x95,0x02,0x09,0x30,0x09,0x31,0x81,0x06,0x15,0x81,
        0x25,0x7F,0x75,0x08,0x95,0x01,0x09,0x38,0x81,0x06,0xC0,0xC0 };
    static hp_layout_t a, b;
    bool ok = hidparse(kb, sizeof(kb), &a) && hidparse(ms, sizeof(ms), &b);
    // keyboard: modifier bitmap at bit 0, 6-key array at bit 16
    ok = ok && a.nfields == 2 && a.f[0].kind == HPF_KEY_BITMAP && a.f[0].usage_min == 0xE0 && a.f[0].bitoff == 0 &&
         a.f[1].kind == HPF_KEY_ARRAY && a.f[1].bitoff == 16 && a.f[1].count == 6 && a.f[1].usage_max == 0x65;
    // a report "W pressed with Shift": 02 00 1A 00 00 00 00 00
    static const uint8_t rep[] = { 0x02, 0x00, 0x1A, 0, 0, 0, 0, 0 };
    ok = ok && hp_extract(rep, 8, 1, 1, false) == 1 && hp_extract(rep, 8, 16, 8, false) == 0x1A;
    // mouse: report IDs, buttons at 0, X at 8 (16-bit signed), Y at 24, wheel at 40
    ok = ok && b.uses_report_ids && b.has_xy && b.has_wheel && b.xy_bits == 16;
    static const uint8_t mrep[] = { 0x01, 0x34, 0x12, 0xFE, 0xFF, 0xFF };   // btn1, X=+0x1234, Y=-2, wheel=-1
    for (int i = 0; ok && i < b.nfields; i++) {
        const hp_field_t *f = &b.f[i];
        if (f->kind == HPF_X) ok = hp_extract(mrep, 6, f->bitoff, f->size, f->is_signed) == 0x1234;
        if (f->kind == HPF_Y) ok = hp_extract(mrep, 6, f->bitoff, f->size, f->is_signed) == -2;
        if (f->kind == HPF_WHEEL) ok = hp_extract(mrep, 6, f->bitoff, f->size, f->is_signed) == -1;
        if (f->kind == HPF_BUTTONS) ok = f->bitoff == 0 && f->count == 5;
    }
    if (ok) ESP_LOGI(TAG, "HID decoder self-test: PASS");
    else ESP_LOGE(TAG, "HID decoder self-test: FAIL - tell Claude");
}

// Called by the HID driver task; hand the work to our own task.
static void driver_cb(hid_host_device_handle_t handle, const hid_host_driver_event_t event, void *arg)
{
    (void)arg;
    app_event_t ev = { .kind = EV_DRIVER, .handle = handle, .event = event };
    xQueueSend(s_event_q, &ev, 0);
}

static void suspend_keyboards(void)
{
    int n = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!s_dev[i].handle || s_dev[i].proto != HID_PROTOCOL_KEYBOARD) continue;
        hid_host_device_handle_t h = s_dev[i].handle;
        hid_host_dev_params_t p;
        uint8_t addr = (hid_host_device_get_params(h, &p) == ESP_OK) ? p.addr : 0;
        taskENTER_CRITICAL(&s_lock);
        memset(&s_dev[i], 0, sizeof(s_dev[i]));
        taskEXIT_CRITICAL(&s_lock);
        if (hid_host_device_pause(h) == ESP_OK) {   // frees the channel, keeps the interface
            park(h, addr);
            n++;
        }
    }
    if (n) ESP_LOGI(TAG, "paused %d keyboard interface(s) to free USB channels for the Xbox controller", n);
}

static void resume_parked(void)
{
    if (s_parked_count == 0) return;
    uint8_t addrs[16];
    int num = 0;
    usb_host_device_addr_list_fill(sizeof(addrs), addrs, &num);

    parked_t list[MAX_DEVICES];
    taskENTER_CRITICAL(&s_lock);
    int count = s_parked_count;
    memcpy(list, s_parked, sizeof(list));
    s_parked_count = 0;
    taskEXIT_CRITICAL(&s_lock);

    int reopened = 0;
    for (int i = 0; i < count; i++) {
        bool present = false;
        for (int a = 0; a < num; a++) present |= (addrs[a] == list[i].addr);
        if (!present) continue;                  // device was unplugged meanwhile
        if (open_device(list[i].handle, true)) reopened++;   // re-parks itself on failure
    }
    if (reopened) ESP_LOGI(TAG, "resumed %d interface(s)", reopened);
}

static void app_event_task(void *arg)
{
    (void)arg;
    app_event_t ev;
    for (;;) {
        if (xQueueReceive(s_event_q, &ev, portMAX_DELAY) != pdTRUE) continue;
        switch (ev.kind) {
        case EV_DRIVER:
            if (ev.event == HID_HOST_DRIVER_EVENT_CONNECTED) open_device(ev.handle, false);
            break;
        case EV_SUSPEND_KB:
            suspend_keyboards();
            xSemaphoreGive(s_suspend_done);
            break;
        case EV_RESUME:
            resume_parked();
            break;
        }
    }
}

void input_usb_suspend_keyboards(void)
{
    app_event_t ev = { .kind = EV_SUSPEND_KB };
    xSemaphoreTake(s_suspend_done, 0);
    xQueueSend(s_event_q, &ev, portMAX_DELAY);
    xSemaphoreTake(s_suspend_done, pdMS_TO_TICKS(500));
}

void input_usb_resume_parked(void)
{
    app_event_t ev = { .kind = EV_RESUME };
    xQueueSend(s_event_q, &ev, 0);
}

// Diagnostic client: logs every USB device that enumerates (hub, mouse, keyboard, ...)
static usb_host_client_handle_t s_diag_client;
static volatile uint8_t s_diag_new_addr[8];
static volatile uint8_t s_diag_new_count;

static void diag_client_cb(const usb_host_client_event_msg_t *msg, void *arg)
{
    (void)arg;
    if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        if (s_diag_new_count < sizeof(s_diag_new_addr)) {
            s_diag_new_addr[s_diag_new_count++] = msg->new_dev.address;
        }
    } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ESP_LOGI(TAG, "USB device removed");
    }
}

static void diag_task(void *arg)
{
    (void)arg;
    const usb_host_client_config_t cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 8,
        .async = {
            .client_event_callback = diag_client_cb,
            .callback_arg = NULL,
        },
    };
    if (usb_host_client_register(&cfg, &s_diag_client) != ESP_OK) {
        ESP_LOGW(TAG, "diag client register failed");
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        usb_host_client_handle_events(s_diag_client, pdMS_TO_TICKS(100));
        while (s_diag_new_count) {
            uint8_t addr = s_diag_new_addr[--s_diag_new_count];
            usb_device_handle_t dev;
            if (usb_host_device_open(s_diag_client, addr, &dev) != ESP_OK) {
                ESP_LOGW(TAG, "USB device at addr %d: could not open", addr);
                continue;
            }
            const usb_device_desc_t *d;
            usb_device_info_t info;
            if (usb_host_get_device_descriptor(dev, &d) == ESP_OK && usb_host_device_info(dev, &info) == ESP_OK) {
                ESP_LOGI(TAG, "USB device addr %d: VID %04x PID %04x class %02x, %s speed, %d configs",
                         addr, d->idVendor, d->idProduct, d->bDeviceClass,
                         info.speed == USB_SPEED_LOW ? "low" : "full", d->bNumConfigurations);
                const usb_config_desc_t *c;
                if (usb_host_get_active_config_descriptor(dev, &c) == ESP_OK) {
                    int off = 0;
                    const usb_standard_desc_t *s = (const usb_standard_desc_t *)c;
                    while ((s = usb_parse_next_descriptor_of_type(s, c->wTotalLength, USB_B_DESCRIPTOR_TYPE_INTERFACE, &off)) != NULL) {
                        const usb_intf_desc_t *i = (const usb_intf_desc_t *)s;
                        ESP_LOGI(TAG, "   interface %d: class %02x subclass %02x protocol %02x, %d endpoints",
                                 i->bInterfaceNumber, i->bInterfaceClass, i->bInterfaceSubClass,
                                 i->bInterfaceProtocol, i->bNumEndpoints);
                    }
                }
            }
            usb_host_device_close(s_diag_client, dev);
        }
    }
}

static void usb_lib_task(void *arg)
{
    TaskHandle_t parent = (TaskHandle_t)arg;
    const usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_cfg));
    xTaskNotifyGive(parent);

    for (;;) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

void input_usb_start(void)
{
    s_event_q = xQueueCreate(16, sizeof(app_event_t));
    s_suspend_done = xSemaphoreCreateBinary();
    hidparse_selftest();

    xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 12, NULL, 0);
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

    const hid_host_driver_config_t drv_cfg = {
        .create_background_task = true,
        .task_priority = 11,
        .stack_size = 4096,
        .core_id = 0,
        .callback = driver_cb,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(hid_host_install(&drv_cfg));
    xTaskCreatePinnedToCore(app_event_task, "hid_events", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(diag_task, "usb_diag", 4096, NULL, 4, NULL, 0);
    ESP_LOGI(TAG, "USB host ready - plug in a hub, mouse and keyboard");
}

void input_usb_snapshot(input_snapshot_t *out)
{
    memset(out, 0, sizeof(*out));
    taskENTER_CRITICAL(&s_lock);
    out->dx = s_acc_dx;
    out->dy = s_acc_dy;
    out->wheel = s_acc_wheel;
    s_acc_dx = s_acc_dy = s_acc_wheel = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!s_dev[i].handle) continue;
        if (s_dev[i].proto == HID_PROTOCOL_MOUSE) {
            out->mouse_buttons |= s_dev[i].buttons;
            out->mice++;
        } else {
            for (int k = 0; k < 32; k++) out->keys[k] |= s_dev[i].keys[k];
            out->keyboards++;
        }
    }
    taskEXIT_CRITICAL(&s_lock);
}
