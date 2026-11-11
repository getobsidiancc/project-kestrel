// Project Kestrel - INPUT side (RIGHT chip)
//
// Reads the mouse and keyboard over USB host, turns them into a controller state
// every millisecond, and sends it to the LEFT chip over the internal UART link.
// Owns the adapter's settings (keybinds, sensitivity) and answers the config app.

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "kestrel_link.h"
#include "kestrel_cfg.h"
#include "settings.h"
#include "input_usb.h"
#include "translate.h"
#include "gip_host.h"

static const char *TAG = "kestrel_in";

#define LED_GPIO 9
#define FW_VERSION 22

static TaskHandle_t s_tick_task;
static volatile bool s_output_mounted;
static volatile int64_t s_last_status_us;
static volatile bool s_live_keys;

// ---- Config requests from the app (handled off the link task: NVS writes are slow) ----

typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t data[KLINK_MAX_PAYLOAD];
} cfg_req_t;

static QueueHandle_t s_cfg_q;
static uint8_t s_cfg_rx[sizeof(kcfg_t)];
static uint16_t s_cfg_rx_len;

static void send_result(uint8_t code)
{
    klink_send(KLINK_MSG_CFG_RESULT, &code, 1);
}

static void cfg_task(void *arg)
{
    (void)arg;
    cfg_req_t r;
    for (;;) {
        if (xQueueReceive(s_cfg_q, &r, portMAX_DELAY) != pdTRUE) continue;
        switch (r.type) {
        case KLINK_MSG_CFG_GET: {
            static kcfg_t c;
            settings_get(&c);
            const uint8_t *p = (const uint8_t *)&c;
            for (uint16_t off = 0; off < sizeof(c); off += 48) {
                uint8_t chunk[50];
                uint16_t n = sizeof(c) - off > 48 ? 48 : sizeof(c) - off;
                chunk[0] = (uint8_t)(off & 0xFF);
                chunk[1] = (uint8_t)(off >> 8);
                memcpy(&chunk[2], p + off, n);
                klink_send(KLINK_MSG_CFG_DATA, chunk, (uint8_t)(n + 2));
            }
            uint8_t total[2] = { (uint8_t)(sizeof(c) & 0xFF), (uint8_t)(sizeof(c) >> 8) };
            klink_send(KLINK_MSG_CFG_END, total, 2);
            break;
        }
        case KLINK_MSG_CFG_SET:
            if (r.len >= 2) {
                uint16_t off = (uint16_t)(r.data[0] | (r.data[1] << 8)), n = r.len - 2;
                if (off == 0) s_cfg_rx_len = 0;
                if (off + n <= sizeof(s_cfg_rx)) {
                    memcpy(&s_cfg_rx[off], &r.data[2], n);
                    if (off + n > s_cfg_rx_len) s_cfg_rx_len = off + n;
                }
            }
            break;
        case KLINK_MSG_CFG_COMMIT:
            if (r.len >= 2 && (uint16_t)(r.data[0] | (r.data[1] << 8)) == sizeof(kcfg_t) &&
                s_cfg_rx_len == sizeof(kcfg_t)) {
                static kcfg_t c;
                memcpy(&c, s_cfg_rx, sizeof(c));
                send_result((uint8_t)settings_set(&c));
            } else {
                send_result(1);
            }
            s_cfg_rx_len = 0;
            break;
        case KLINK_MSG_CFG_DEFAULTS:
            send_result((uint8_t)settings_reset());
            break;
        default:
            break;
        }
    }
}

static void link_rx(uint8_t type, const uint8_t *payload, uint8_t len, void *arg)
{
    (void)arg;
    switch (type) {
    case KLINK_MSG_GIP_TO_DEVICE:
        gip_host_from_console(payload, len);
        break;
    case KLINK_MSG_STATUS:
        if (len >= sizeof(link_status_t)) {
            const link_status_t *st = (const link_status_t *)payload;
            s_output_mounted = st->usb_mounted;
            gip_host_set_console_state(st->auth_done, st->want_auth);
            s_last_status_us = esp_timer_get_time();
        }
        break;
    case KLINK_MSG_CFG_GET:
    case KLINK_MSG_CFG_SET:
    case KLINK_MSG_CFG_COMMIT:
    case KLINK_MSG_CFG_DEFAULTS: {
        cfg_req_t r = { .type = type, .len = len };
        memcpy(r.data, payload, len);
        xQueueSend(s_cfg_q, &r, 0);
        break;
    }
    case KLINK_MSG_CAL:
        if (len >= 2) {
            int16_t v = (int16_t)(payload[0] | (payload[1] << 8));
            translate_set_override(v < 0 ? -1.0f : v / 1000.0f);
        }
        break;
    case KLINK_MSG_LIVE:
        if (len >= 1) s_live_keys = payload[0] != 0;
        break;
    default:
        break;
    }
}

// Send this chip's log lines to the LEFT chip, which prints them on its UART (the MAKCU's
// COM port). Also keeps printing to our own UART0.
static int log_forward(const char *fmt, va_list args)
{
    char line[256];
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(line, sizeof(line), fmt, copy);
    va_end(copy);
    if (n > 0) {
        if (n > (int)sizeof(line) - 1) n = sizeof(line) - 1;
        if (!xPortInIsrContext()) {
            for (int off = 0; off < n; off += KLINK_MAX_PAYLOAD) {
                int chunk = (n - off) > KLINK_MAX_PAYLOAD ? KLINK_MAX_PAYLOAD : (n - off);
                klink_send(KLINK_MSG_LOG, line + off, (uint8_t)chunk);
            }
        }
    }
    return vprintf(fmt, args);
}

static void IRAM_ATTR tick_cb(void *arg)
{
    (void)arg;
    xTaskNotifyGive(s_tick_task);
}

// Lowest delay: a new mouse/keyboard report is processed straight away instead of waiting up
// to 1 ms for the next tick. The 1 ms timer is restarted from that moment, so steps stay ~1 ms
// apart (everything that counts milliseconds keeps its timing); a report arriving right after
// a step simply goes out with the next one.
static esp_timer_handle_t s_tick_timer;
static volatile int64_t s_last_step_us;
#define EARLY_STEP_MIN_US 700
static void report_arrived(void)
{
    if (!s_tick_task || !s_tick_timer) return;
    if (esp_timer_get_time() - s_last_step_us < EARLY_STEP_MIN_US) return;
    esp_timer_restart(s_tick_timer, 1000);
    xTaskNotifyGive(s_tick_task);
}

static inline bool src_active(const input_snapshot_t *in, uint8_t src, const int *wheel_hold)
{
    if (src == 0) return false;
    if (src < 0xF0) return input_key_down(in, src);
    if (src < KSRC_WHEEL_UP) return (in->mouse_buttons >> (src - 0xF0)) & 1;
    if (src == KSRC_WHEEL_UP) return wheel_hold[0] > 0;
    if (src == KSRC_WHEEL_DOWN) return wheel_hold[1] > 0;
    return false;
}

// Per-bind state for toggle binds
static bool s_bind_prev[KCFG_MAX_BINDS];
static bool s_bind_latched[KCFG_MAX_BINDS];
static uint8_t s_bind_tap[KCFG_MAX_BINDS];
static uint16_t s_bind_long[KCFG_MAX_BINDS];
#define TAP_GAP_MS 50
static bool s_rapid_on;                        // rapid-click mode (KTGT_RAPID_MODE)
static bool s_rapid_armed;                     // armed since mode on (KTGT_RAPID_ARM)
static uint16_t s_gadget_ms;                   // gadget mode: ms left (0 = off)
static bool s_placing;                         // the fire key is currently placing a gadget (X)
static uint8_t s_rapid_t[KCFG_MAX_BINDS];      // ms into the current click cycle
static uint8_t s_tap_q[KCFG_MAX_BINDS];     // tap presses waiting to be sent
static uint8_t s_tap_gap[KCFG_MAX_BINDS];   // ms of release left after a tap
// "Only while aiming" binds (Siege lean) are sent as clean presses: every key press becomes
// exactly one button press, at least AIM_MIN_ON_MS long with at least AIM_MIN_OFF_MS between
// presses, so a quick tap or fast spam is never too short for the game or merged into one.
// A press made shortly before aiming, or while aim is still settling, fires once aim has settled.
#define AIM_MIN_ON_MS   90
#define AIM_MIN_OFF_MS  50
#define AIM_PRE_MS      250    // a press this long before aiming starts still counts
static uint8_t  s_bind_pend[KCFG_MAX_BINDS];   // presses waiting to be sent
static uint8_t  s_bind_pre[KCFG_MAX_BINDS];    // ms left for a press made before aiming
static bool     s_bind_out[KCFG_MAX_BINDS];    // button currently pressed by this bind
static uint16_t s_bind_t[KCFG_MAX_BINDS];      // ms in the current on/off phase

typedef struct {
    bool aiming;          // an LT bind is active -> ADS aim setup
    float look_x, look_y; // look-with-keys direction (0 if none)
} pad_extra_t;

static void build_pad(const kcfg_t *cfg, const input_snapshot_t *in, pad_state_t *pad, int *wheel_hold,
                      pad_extra_t *ex)
{
    // Wheel clicks become short button presses
    if (in->wheel > 0) wheel_hold[0] = cfg->wheel_hold_ms;
    if (in->wheel < 0) wheel_hold[1] = cfg->wheel_hold_ms;

    uint16_t b = 0;
    bool up = false, down = false, left = false, right = false, walk = false;
    bool lup = false, ldown = false, lleft = false, lright = false;
    uint8_t lt = 0, rt = 0;

    // Pass 1: which binds are on (hold / toggle), and is the player aiming?
    static bool on_now[KCFG_MAX_BINDS];
    bool aiming = false;

    // Rapid-click runs only when mode is on, it has been armed, and a "hold" key is held
    const bool rapid_armed = s_rapid_on && s_rapid_armed;
    bool rapid_hold = false;
    if (rapid_armed) {
        for (int i = 0; i < KCFG_MAX_BINDS; i++)
            if ((cfg->binds[i].flags & KBIND_RAPID_HOLD) && cfg->binds[i].tgt != KTGT_NONE &&
                src_active(in, cfg->binds[i].src, wheel_hold)) rapid_hold = true;
    }

    for (int i = 0; i < KCFG_MAX_BINDS; i++) {
        const kcfg_bind_t *bd = &cfg->binds[i];
        on_now[i] = false;
        if (bd->tgt == KTGT_NONE) continue;
        bool raw = src_active(in, bd->src, wheel_hold);
        bool on = raw;
        const bool edge = raw && !s_bind_prev[i];
        if (bd->tgt == KTGT_RAPID_MODE) {
            if (edge) {
                s_rapid_on = !s_rapid_on;
                s_rapid_armed = false;
                ESP_LOGI(TAG, "rapid-click mode %s", s_rapid_on ? "ON (press the arm key)" : "off");
            }
            on = false;
        } else if (bd->tgt == KTGT_RAPID_ARM) {
            if (edge && s_rapid_on && !s_rapid_armed) {
                s_rapid_armed = true;
                ESP_LOGI(TAG, "rapid-click armed");
            }
            on = false;
        } else if (bd->tgt == KTGT_RAPID_DISARM) {
            if (edge && s_rapid_armed) {
                s_rapid_armed = false;
                ESP_LOGI(TAG, "rapid-click disarmed");
            }
            on = false;
        } else if ((bd->flags & KBIND_NOT_RAPID) && s_rapid_on) {
            on = false;                       // this bind is switched off while rapid-click is on
        } else if ((bd->flags & KBIND_RAPID_HOLD) && rapid_armed) {
            on = false;                       // the "hold" key only enables rapid-click while armed
        } else if ((bd->flags & KBIND_RAPID) && rapid_armed && rapid_hold) {
            // held: click, release, click... (first click straight away)
            if (raw) {
                on = s_rapid_t[i] < KBIND_RAPID_ON_MS;
                if (++s_rapid_t[i] >= KBIND_RAPID_ON_MS + KBIND_RAPID_OFF_MS) s_rapid_t[i] = 0;
            } else {
                on = false;
                s_rapid_t[i] = 0;
            }
        } else if (bd->flags & KBIND_RAPID) {
            s_rapid_t[i] = 0;                 // rapid-click not running: the key works normally
        } else if (bd->flags & KBIND_TAP) {
            // Every key press becomes exactly one click of KBIND_TAP_MS with at least
            // TAP_GAP_MS released in between, so fast spam (Siege crouch spam) is never merged
            // into one press or sent with a gap too short for the game to see.
            if (raw && !s_bind_prev[i] && s_tap_q[i] < 3) s_tap_q[i]++;
            if (s_bind_tap[i] > 0) {
                on = true;
                if (--s_bind_tap[i] == 0) s_tap_gap[i] = TAP_GAP_MS;
            } else if (s_tap_gap[i] > 0) {
                s_tap_gap[i]--;
                on = false;
            } else if (s_tap_q[i] > 0) {
                s_tap_q[i]--;
                s_bind_tap[i] = KBIND_TAP_MS;
                on = true;
            } else {
                on = false;
            }
        } else if (bd->flags & KBIND_TOGGLE) {
            if (raw && !s_bind_prev[i]) s_bind_latched[i] = !s_bind_latched[i];   // press edge flips
            on = s_bind_latched[i];
        } else if (bd->flags & KBIND_LONG) {
            if (raw && !s_bind_prev[i]) s_bind_long[i] = KBIND_LONG_MS;   // press edge: hold at least this long
            on = raw || s_bind_long[i] > 0;
            if (s_bind_long[i] > 0) s_bind_long[i]--;
        }
        // Gadget mode: a gadget key turns it on (again = off); weapon swap (Y) turns it off
        if (raw && !s_bind_prev[i]) {
            if (bd->flags & KBIND_GADGET) s_gadget_ms = s_gadget_ms ? 0 : KBIND_GADGET_MS;
            else if (bd->tgt == KTGT_BUTTON(15)) s_gadget_ms = 0;
        }
        s_bind_prev[i] = raw;
        on_now[i] = on;
        if (on && bd->tgt == KTGT_LT && !(bd->flags & KBIND_AIM_ONLY)) aiming = true;
    }

    // Aim has to be held a moment before "only while aiming" binds fire: the game needs time to
    // actually enter ADS, and until then the same button means something else (Siege: sprint /
    // melee on the stick clicks). A press made during that moment fires once aim has settled.
    static uint16_t s_aim_ms;
    const uint16_t settle_ms = cfg->aim_settle_10ms ? (uint16_t)(cfg->aim_settle_10ms * 10) : 200;
    if (aiming) { if (s_aim_ms < 60000) s_aim_ms++; } else s_aim_ms = 0;
    const bool settled = aiming && s_aim_ms >= settle_ms;

    // Gadget mode ends when aiming or when its time runs out (not in the middle of a place press)
    if (aiming && !s_placing) s_gadget_ms = 0;
    if (s_gadget_ms > 0 && !s_placing) s_gadget_ms--;
    bool fire_on = false;

    // Buttons used by "only while aiming" binds. While aiming, the other binds on those buttons
    // are ignored (Siege: Shift = sprint and V = melee are the stick clicks, which lean while
    // aiming) so they can't lean by accident or hide a lean press.
    uint32_t aim_only_tgts = 0;
    for (int i = 0; i < KCFG_MAX_BINDS; i++)
        if ((cfg->binds[i].flags & KBIND_AIM_ONLY) && cfg->binds[i].tgt <= 31)
            aim_only_tgts |= 1u << cfg->binds[i].tgt;

    // Pass 2: apply them
    static bool s_aim_raw_prev[KCFG_MAX_BINDS];
    for (int i = 0; i < KCFG_MAX_BINDS; i++) {
        const kcfg_bind_t *bd = &cfg->binds[i];
        if (bd->tgt == KTGT_NONE) continue;
        if (bd->flags & KBIND_AIM_ONLY) {
            const bool raw = on_now[i];
            const bool edge = raw && !s_aim_raw_prev[i];
            s_aim_raw_prev[i] = raw;
            if (!aiming) {
                // Not aiming: nothing is sent, but remember a fresh press for a moment
                s_bind_out[i] = false; s_bind_pend[i] = 0; s_bind_t[i] = AIM_MIN_OFF_MS;
                if (raw) s_bind_pre[i] = AIM_PRE_MS;           // held, or let go just before aiming
                else if (s_bind_pre[i] > 0) s_bind_pre[i]--;
                continue;
            }
            if (!settled) {
                if (edge) s_bind_pre[i] = AIM_PRE_MS;   // held until aim settles
                continue;
            }
            if (s_bind_pre[i]) { s_bind_pre[i] = 0; if (!s_bind_pend[i]) s_bind_pend[i] = 1; }
            if (edge && s_bind_pend[i] < 3) s_bind_pend[i]++;
            if (s_bind_t[i] < 60000) s_bind_t[i]++;
            if (s_bind_out[i]) {
                // release once the key is up (or another press is queued) and the press is long enough
                if (s_bind_t[i] >= AIM_MIN_ON_MS && (!raw || s_bind_pend[i])) { s_bind_out[i] = false; s_bind_t[i] = 0; }
            } else if (s_bind_pend[i] && s_bind_t[i] >= AIM_MIN_OFF_MS) {
                s_bind_out[i] = true; s_bind_pend[i]--; s_bind_t[i] = 0;
            }
            if (!s_bind_out[i]) continue;
        } else {
            if (!on_now[i]) continue;
            if (aiming && bd->tgt <= 31 && (aim_only_tgts >> bd->tgt & 1)) continue;
        }
        switch (bd->tgt) {
        case KTGT_LT:         lt = 255; break;
        case KTGT_RT:         rt = 255; break;
        case KTGT_FIRE_OR_PLACE:
            // fire normally; in gadget mode this press places the gadget (X) instead
            fire_on = true;
            if (s_placing || s_gadget_ms > 0) { s_placing = true; b |= PAD_X; }
            else rt = 255;
            break;
        case KTGT_MOVE_UP:    up = true; break;
        case KTGT_MOVE_DOWN:  down = true; break;
        case KTGT_MOVE_LEFT:  left = true; break;
        case KTGT_MOVE_RIGHT: right = true; break;
        case KTGT_WALK:       walk = true; break;
        case KTGT_LOOK_UP:    lup = true; break;
        case KTGT_LOOK_DOWN:  ldown = true; break;
        case KTGT_LOOK_LEFT:  lleft = true; break;
        case KTGT_LOOK_RIGHT: lright = true; break;
        default:
            if (bd->tgt >= 1 && bd->tgt <= 16) b |= (uint16_t)(1u << (bd->tgt - 1));
            break;
        }
    }
    if (s_placing && !fire_on) {        // place press finished: back to normal firing
        s_placing = false;
        s_gadget_ms = 0;
    }
    if (wheel_hold[0] > 0) wheel_hold[0]--;
    if (wheel_hold[1] > 0) wheel_hold[1]--;

    pad->buttons = b;
    pad->lt = lt;
    pad->rt = rt;

    // Sprinting: the left stick click (sprint in most games) pressed while moving forward. It
    // lasts until forward is let go, or aiming / walking / moving back.
    static bool s_sprint, s_ls_prev;
    const bool ls_now = (b & PAD_LS) && !aiming;
    if (ls_now && !s_ls_prev && up && !down) s_sprint = true;
    if (!up || down || aiming || walk) s_sprint = false;
    s_ls_prev = ls_now;

    // Movement keys -> left stick (round gate, so diagonals aren't faster). While sprinting, a
    // diagonal (W+A / W+D) points 25 degrees off forward instead of 45: Siege drops the sprint
    // if the stick points too far to the side.
    float x = (right ? 1.0f : 0.0f) - (left ? 1.0f : 0.0f);
    float y = (up ? 1.0f : 0.0f) - (down ? 1.0f : 0.0f);
    if (x != 0 && y != 0) {
        if (s_sprint && y > 0) { x *= 0.4226f; y = 0.9063f; }
        else { x *= 0.7071f; y *= 0.7071f; }
    }
    float scale = walk ? cfg->walk_scale_pct / 100.0f : 1.0f;
    pad->lx = (int16_t)lrintf(x * scale * 32767.0f);
    pad->ly = (int16_t)lrintf(y * scale * 32767.0f);

    // Look keys -> right stick direction
    float gx = (lright ? 1.0f : 0.0f) - (lleft ? 1.0f : 0.0f);
    float gy = (lup ? 1.0f : 0.0f) - (ldown ? 1.0f : 0.0f);
    if (gx != 0 && gy != 0) { gx *= 0.7071f; gy *= 0.7071f; }
    ex->look_x = gx;
    ex->look_y = gy;
    ex->aiming = lt != 0;
}

static void apply_aim(const kcfg_t *c)
{
    translate_init(c);
    memset(s_bind_prev, 0, sizeof(s_bind_prev));
    memset(s_bind_latched, 0, sizeof(s_bind_latched));
    memset(s_bind_tap, 0, sizeof(s_bind_tap));
    memset(s_bind_pend, 0, sizeof(s_bind_pend));
    memset(s_bind_pre, 0, sizeof(s_bind_pre));
    memset(s_bind_long, 0, sizeof(s_bind_long));
    memset(s_tap_q, 0, sizeof(s_tap_q));
    memset(s_rapid_t, 0, sizeof(s_rapid_t));
    memset(s_tap_gap, 0, sizeof(s_tap_gap));
    memset(s_bind_out, 0, sizeof(s_bind_out));
    memset(s_bind_t, 0, sizeof(s_bind_t));
}

static void send_live_keys(const input_snapshot_t *in)
{
    uint8_t msg[2 + 32];
    uint8_t n = 0;
    for (int u = 1; u < 256 && n < 32; u++) {
        if (input_key_down(in, (uint8_t)u)) msg[2 + n++] = (uint8_t)u;
    }
    msg[0] = in->mouse_buttons;
    msg[1] = n;
    klink_send(KLINK_MSG_KEYS, msg, (uint8_t)(2 + n));
}

static void tick_task(void *arg)
{
    (void)arg;
    static kcfg_t cfg;
    uint32_t gen = 0;
    pad_state_t pad = {0};
    input_snapshot_t in;
    int wheel_hold[2] = {0, 0};
    uint32_t n = 0;
    uint8_t last_mb = 0xFF;
    uint8_t last_keys[32];
    memset(last_keys, 0xFF, sizeof(last_keys));

    settings_get(&cfg);
    gen = settings_generation();
    apply_aim(&cfg);

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        s_last_step_us = esp_timer_get_time();

        if (settings_generation() != gen) {   // the app saved new settings
            settings_get(&cfg);
            gen = settings_generation();
            apply_aim(&cfg);
        }

        input_usb_snapshot(&in);
        pad_extra_t ex;
        build_pad(&cfg, &in, &pad, wheel_hold, &ex);
        int16_t rx, ry;
        translate_step(in.dx, in.dy, ex.aiming, &rx, &ry);
        if (ex.look_x != 0.0f || ex.look_y != 0.0f) {   // look keys override the mouse while held
            rx = (int16_t)lrintf(ex.look_x * 32767.0f);
            ry = (int16_t)lrintf(ex.look_y * 32767.0f);
        }
        pad.rx = rx;
        pad.ry = ry;
        pad.seq++;
        klink_send(KLINK_MSG_PAD_STATE, &pad, sizeof(pad));

        // Live key view for the app's "press a key to bind" (only when something changed)
        if (s_live_keys && (n % 10) == 0 &&
            (in.mouse_buttons != last_mb || memcmp(in.keys, last_keys, sizeof(last_keys)) != 0)) {
            last_mb = in.mouse_buttons;
            memcpy(last_keys, in.keys, sizeof(last_keys));
            send_live_keys(&in);
        }

        // LED: solid = mouse connected and PC/console sees the controller,
        //      slow blink = waiting for devices / output side.
        n++;
        bool linked = (esp_timer_get_time() - s_last_status_us) < 500000;
        bool ok = in.mice > 0 && linked && s_output_mounted;
        gpio_set_level(LED_GPIO, ok ? 1 : ((n / 500) & 1));

        if ((n % 250) == 0) {
            input_status_t ist = { .mice = in.mice, .keyboards = in.keyboards, .fw_version = FW_VERSION };
            klink_send(KLINK_MSG_INPUT_STATUS, &ist, sizeof(ist));
        }
        if ((n % 5000) == 0) {
            ESP_LOGI(TAG, "mice=%d keyboards=%d link=%s output=%s rx_ok=%lu rx_err=%lu",
                     in.mice, in.keyboards, linked ? "up" : "DOWN",
                     s_output_mounted ? "mounted" : "not mounted",
                     (unsigned long)klink_rx_frames(), (unsigned long)klink_rx_errors());
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Project Kestrel input side v%d", FW_VERSION);

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    s_cfg_q = xQueueCreate(16, sizeof(cfg_req_t));
    ESP_ERROR_CHECK(klink_init(false, link_rx, NULL));
    esp_log_set_vprintf(log_forward);
    ESP_LOGI(TAG, "Project Kestrel input side v%d - log forwarding on", FW_VERSION);
    settings_init();
    xTaskCreatePinnedToCore(cfg_task, "cfg", 4096, NULL, 4, NULL, 0);

    input_usb_start();
    gip_host_start();

    xTaskCreatePinnedToCore(tick_task, "tick", 4096, NULL, 15, &s_tick_task, 1);

    const esp_timer_create_args_t targs = {
        .callback = tick_cb,
        .name = "tick_1ms",
    };
    ESP_ERROR_CHECK(esp_timer_create(&targs, &s_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_tick_timer, 1000));
    input_usb_set_report_cb(report_arrived);
}
