// Project Kestrel - mouse -> right stick translation
//
// Pipeline (runs every 1 ms):
//   1. average mouse counts over a short window      -> counts per second
//   2. counts/s / DPI * 2.54                           -> cm per second
//   3. cm/s / cm_per_360 * 360                         -> wanted turn speed (deg/s)
//   4. look the speed up in the game's measured response table (hipfire or ADS) and
//      interpolate the stick position that produces it. The table starts at the game's
//      deadzone, so tiny movements still turn; speeds above the table's top are capped.
//   5. optional smoothing, then convert to a 16-bit stick value

#include "translate.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WINDOW 32
#define SLOW_MIN_COUNTS 3   // fewer counts than this in the window = slow move, average longer
#define SLOW_MAX_CPS    1500 // counts/s; the longer average is only used below this speed

static float s_dpi = 800.0f, s_smoothing;
static uint8_t s_window = 8;
static bool s_invert_y, s_axial, s_ads_enabled;
static float s_hip_fast;    // fast-turn boost gain (0 = off)
#define FAST_KNEE_FRAC 0.12f // boost starts at this fraction of the game's top speed (~150 deg/s):
                             // slower, precise aim stays exactly 1:1, swipes speed up
static kcfg_aim_t s_hip, s_ads;

static int32_t s_hist_x[MAX_WINDOW];
static int32_t s_hist_y[MAX_WINDOW];
static uint8_t s_hist_pos;
static int64_t s_sum_x, s_sum_y;
static float s_out_x, s_out_y;
static volatile float s_override = -1.0f;

void translate_init(const kcfg_t *c)
{
    s_dpi = c->mouse_dpi > 50 ? c->mouse_dpi : 800.0f;
    s_smoothing = c->smoothing;
    s_window = c->window_ms < 1 ? 1 : (c->window_ms > MAX_WINDOW ? MAX_WINDOW : c->window_ms);
    s_invert_y = c->invert_y != 0;
    s_axial = c->dz_shape != 0;
    s_ads_enabled = c->ads_enabled != 0;
    s_hip_fast = c->hip_fast_pct / 100.0f;
    s_hip = c->hip;
    s_ads = c->ads;
    memset(s_hist_x, 0, sizeof(s_hist_x));
    memset(s_hist_y, 0, sizeof(s_hist_y));
    s_hist_pos = 0;
    s_sum_x = s_sum_y = 0;
    s_out_x = s_out_y = 0.0f;
}

void translate_set_override(float x_deflection)
{
    s_override = x_deflection;
}

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Full-tilt boost: games like Siege turn at one speed just below full stick and a much higher
// one at full stick, with nothing in between, and the boost only builds while the stick STAYS
// at full. A table whose last step is tiny in stick travel but big in speed is treated that
// way: a fast mouse move goes to full stick and stays there (so the boost kicks in) until the
// mouse clearly slows down again.
#define BOOST_STEP_MAX_DEFL 0.03f
#define BOOST_ENTER         0.25f   // go to full tilt above this far into the step
#define BOOST_EXIT          0.10f   // leave it below this far (and after BOOST_MIN_MS)
#define BOOST_MIN_MS        40
static bool s_boost_on;
static uint16_t s_boost_ms;

// Wanted turn speed (deg/s, >= 0) -> stick deflection (0..1) using the response table.
static float invert(const kcfg_aim_t *a, float speed)
{
    const int n = a->npoints;
    if (n >= 3) {
        const float d0 = a->defl[n - 2], d1 = a->defl[n - 1];
        const float s0 = a->dps[n - 2], s1 = a->dps[n - 1];
        if (d1 - d0 <= BOOST_STEP_MAX_DEFL && s1 > s0 * 1.3f) {
            const float gap = s1 - s0;
            if (s_boost_on) {
                if (s_boost_ms < 60000) s_boost_ms++;
                // mouse slowed right down: leave at once; just under the threshold: only after a moment
                if (speed < s0 || (speed < s0 + BOOST_EXIT * gap && s_boost_ms >= BOOST_MIN_MS)) s_boost_on = false;
                else return d1;
            } else if (speed >= s0 + BOOST_ENTER * gap) {
                s_boost_on = true; s_boost_ms = 0;
                return d1;
            }
            if (speed > s0) return d0;
        }
    }
    if (speed <= 0.0f) return 0.0f;
    for (int i = 0; i + 1 < n; i++) {
        if (speed <= a->dps[i + 1]) {
            float s0 = a->dps[i], s1 = a->dps[i + 1];
            float t = (s1 > s0) ? (speed - s0) / (s1 - s0) : 1.0f;
            if (speed < s0) t = 0.0f;          // below the first measured point
            return a->defl[i] + clampf(t, 0.0f, 1.0f) * (a->defl[i + 1] - a->defl[i]);
        }
    }
    return a->defl[n - 1];                     // faster than the game can turn: full stick
}

void translate_step(int32_t dx, int32_t dy, bool ads, int16_t *out_rx, int16_t *out_ry)
{
    // 1. Adaptive average. Fast moves use the short window (snappy). A slow move sends counts
    //    only every few ms, and a short window then flips between "moving" and "stopped" -
    //    the stick snaps on and off and the aim shakes. So while the short window holds only
    //    a few counts, look further back (2x, 4x ... up to MAX_WINDOW ms) to get a steady speed.
    s_hist_x[s_hist_pos] = dx;
    s_hist_y[s_hist_pos] = dy;
    s_hist_pos = (uint8_t)((s_hist_pos + 1) % MAX_WINDOW);
    uint8_t w = s_window;
    int64_t short_x = 0, short_y = 0;
    // How far back a slow move may look: 4x the window setting (3 ms -> 12 ms), so the window
    // setting in the app also tunes this. And if the mouse sent nothing in the last 2 windows it
    // has stopped: no look-back at all, the aim stops at once (no float).
    const uint8_t max_w = (uint8_t)(s_window * 4 > MAX_WINDOW ? MAX_WINDOW : s_window * 4);
    int64_t recent = 0;
    for (uint8_t k = 1; k <= (uint8_t)(s_window * 2 > MAX_WINDOW ? MAX_WINDOW : s_window * 2); k++) {
        const uint8_t p = (uint8_t)((s_hist_pos + MAX_WINDOW - k) % MAX_WINDOW);
        recent += llabs(s_hist_x[p]) + llabs(s_hist_y[p]);
    }
    for (;;) {
        s_sum_x = s_sum_y = 0;
        for (uint8_t k = 1; k <= w; k++) {
            const uint8_t p = (uint8_t)((s_hist_pos + MAX_WINDOW - k) % MAX_WINDOW);
            s_sum_x += s_hist_x[p];
            s_sum_y += s_hist_y[p];
        }
        if (w == s_window) { short_x = s_sum_x; short_y = s_sum_y; }
        if (recent == 0 || llabs(s_sum_x) + llabs(s_sum_y) >= SLOW_MIN_COUNTS || w >= max_w) break;
        w = (uint8_t)(w * 2 > max_w ? max_w : w * 2);
    }
    // The longer look-back is only for slow moves. If it finds a fast move instead, the mouse
    // has just stopped after a swipe: use the short window so the aim stops at once.
    if (w > s_window && (llabs(s_sum_x) + llabs(s_sum_y)) * 1000 / w > SLOW_MAX_CPS) {
        w = s_window; s_sum_x = short_x; s_sum_y = short_y;
    }

    const float ov = s_override;
    if (ov >= 0.0f) {
        *out_rx = (int16_t)lrintf(clampf(ov, 0.0f, 1.0f) * 32767.0f);
        *out_ry = 0;
        return;
    }

    const kcfg_aim_t *a = (ads && s_ads_enabled) ? &s_ads : &s_hip;

    // 2 + 3. counts/s -> cm/s -> deg/s
    const float counts_per_s_x = (float)s_sum_x * 1000.0f / (float)w;
    const float counts_per_s_y = (float)s_sum_y * 1000.0f / (float)w;
    const float deg_per_count = (2.54f / s_dpi) / a->cm_per_360 * 360.0f;
    float tx = counts_per_s_x * deg_per_count;
    float ty = counts_per_s_y * deg_per_count * a->y_ratio;   // y_ratio = up/down speed vs left/right

    // Hipfire fast-turn boost: below the knee the turn is exactly 1:1; above it every extra
    // deg/s counts (1 + gain) times, so fast swipes get to the game's top speed sooner.
    if (a == &s_hip && s_hip_fast > 0.0f && a->npoints >= 2) {
        const int n = a->npoints;
        float top = a->dps[n - 1];
        if (n >= 3 && a->defl[n - 1] - a->defl[n - 2] <= BOOST_STEP_MAX_DEFL) top = a->dps[n - 2];
        const float knee = FAST_KNEE_FRAC * top;
        const float mag = sqrtf(tx * tx + ty * ty);
        if (mag > knee && mag > 1e-3f) {
            const float f = (knee + (mag - knee) * (1.0f + s_hip_fast)) / mag;
            tx *= f; ty *= f;
        }
    }

    // 4. invert the game's response
    float sx = 0.0f, sy = 0.0f;
    if (s_axial) {
        sx = copysignf(invert(a, fabsf(tx)), tx);
        sy = copysignf(invert(a, fabsf(ty)), ty);
        float m = sqrtf(sx * sx + sy * sy);
        if (m > 1.0f) { sx /= m; sy /= m; }
    } else {
        float mag = sqrtf(tx * tx + ty * ty);
        if (mag > 1e-3f) {
            float out = invert(a, mag);
            sx = out * tx / mag;
            sy = out * ty / mag;
        }
    }

    // 5. smoothing (simple exponential filter)
    const float k = clampf(s_smoothing, 0.0f, 0.95f);
    s_out_x = k * s_out_x + (1.0f - k) * sx;
    s_out_y = k * s_out_y + (1.0f - k) * sy;
    const float dz = a->defl[0];
    if (sx == 0.0f && sy == 0.0f && fabsf(s_out_x) < dz && fabsf(s_out_y) < dz) {
        s_out_x = s_out_y = 0.0f;   // mouse stopped: centre now instead of creeping inside the deadzone
    }

    // Mouse +y is "moved toward you" (look down); stick +y is up.
    float y = s_invert_y ? s_out_y : -s_out_y;
    *out_rx = (int16_t)lrintf(clampf(s_out_x, -1.0f, 1.0f) * 32767.0f);
    *out_ry = (int16_t)lrintf(clampf(y, -1.0f, 1.0f) * 32767.0f);
}
