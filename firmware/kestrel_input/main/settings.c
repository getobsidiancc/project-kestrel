// Project Kestrel - saved settings (RIGHT chip, NVS)

#include "settings.h"

#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "kestrel_config.h"

static const char *TAG = "settings";
static const char *NVS_NS = "kestrel";
static const char *NVS_KEY = "cfg";

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static kcfg_t s_cfg;
static volatile uint32_t s_gen;

static bool finite_in(float v, float lo, float hi)
{
    return isfinite(v) && v >= lo && v <= hi;
}

static bool valid_aim(const kcfg_aim_t *a)
{
    if (!finite_in(a->cm_per_360, 0.5f, 500) || !finite_in(a->y_ratio, 0.1f, 10)) return false;
    if (a->npoints < 2 || a->npoints > KCFG_CURVE_POINTS) return false;
    for (int i = 0; i < a->npoints; i++) {
        if (!finite_in(a->defl[i], 0, 1) || !finite_in(a->dps[i], 0, 10000)) return false;
        if (i > 0 && (a->defl[i] <= a->defl[i - 1] || a->dps[i] < a->dps[i - 1])) return false;
    }
    return a->dps[a->npoints - 1] > 0;
}

static bool valid(const kcfg_t *c)
{
    if (c->magic != KCFG_MAGIC || c->version != KCFG_VERSION || c->size != sizeof(kcfg_t)) return false;
    if (!finite_in(c->mouse_dpi, 50, 64000) || !finite_in(c->smoothing, 0, 0.95f)) return false;
    if (c->dz_shape > 1 || c->window_ms < 1 || c->window_ms > 32 || c->walk_scale_pct > 100) return false;
    if (!valid_aim(&c->hip) || !valid_aim(&c->ads)) return false;
    for (int i = 0; i < KCFG_MAX_BINDS; i++) {
        if (c->binds[i].tgt > KTGT_MAX) return false;
    }
    return true;
}

// Old single-curve settings (v1) -> v2. Keeps sensitivity, feel and every keybind.
static bool upgrade_v1(const kcfg_v1_t *o, kcfg_t *n)
{
    if (o->magic != KCFG_MAGIC || o->version != 1 || o->size != sizeof(kcfg_v1_t)) return false;
    *n = KESTREL_FACTORY_DEFAULTS;
    n->mouse_dpi = o->mouse_dpi;
    n->smoothing = o->smoothing;
    n->invert_y = o->invert_y;
    n->dz_shape = o->dz_shape;
    n->window_ms = o->window_ms;
    n->walk_scale_pct = o->walk_scale_pct;
    n->wheel_hold_ms = o->wheel_hold_ms;

    kcfg_aim_t a = { .cm_per_360 = o->cm_per_360, .y_ratio = o->y_ratio, .npoints = 6 };
    float dz = o->deadzone, ex = o->game_curve_exp > 0.1f ? o->game_curve_exp : 1.0f;
    for (int k = 0; k < 6; k++) {
        float t = k / 5.0f;
        a.defl[k] = dz + (1.0f - dz) * t;
        a.dps[k] = o->game_max_turn_dps * powf(t, ex);
    }
    if (dz >= 1.0f) return false;
    n->hip = a;
    n->ads = a;
    n->ads_enabled = 0;

    memset(n->binds, 0, sizeof(n->binds));
    int j = 0;
    for (int i = 0; i < KCFG_V1_MAX_BINDS; i++) {
        if (o->binds[i].src && o->binds[i].tgt) {
            n->binds[j].src = o->binds[i].src;
            n->binds[j].tgt = o->binds[i].tgt;
            n->binds[j].flags = 0;
            j++;
        }
    }
    return valid(n);
}

static int save(const kcfg_t *c)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return 2;
    esp_err_t err = nvs_set_blob(h, NVS_KEY, c, sizeof(*c));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK ? 0 : 2;
}

static void apply(const kcfg_t *c)
{
    taskENTER_CRITICAL(&s_lock);
    s_cfg = *c;
    s_gen++;
    taskEXIT_CRITICAL(&s_lock);
}

void settings_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    static uint8_t blob[sizeof(kcfg_t)];
    size_t len = sizeof(blob);
    nvs_handle_t h;
    bool have = false;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        have = nvs_get_blob(h, NVS_KEY, blob, &len) == ESP_OK;
        nvs_close(h);
    }

    kcfg_t c;
    if (have && len == sizeof(kcfg_t) && (memcpy(&c, blob, sizeof(c)), valid(&c))) {
        ESP_LOGI(TAG, "loaded saved settings (%.0f DPI, hipfire %.1f cm/360)", c.mouse_dpi, c.hip.cm_per_360);
        apply(&c);
    } else if (have && len == sizeof(kcfg_v1_t) && upgrade_v1((const kcfg_v1_t *)blob, &c)) {
        ESP_LOGI(TAG, "upgraded saved settings from v1 (keybinds and sensitivity kept)");
        apply(&c);
        save(&c);
    } else {
        ESP_LOGI(TAG, "no saved settings - using factory defaults");
        apply(&KESTREL_FACTORY_DEFAULTS);
    }
}

void settings_get(kcfg_t *out)
{
    taskENTER_CRITICAL(&s_lock);
    *out = s_cfg;
    taskEXIT_CRITICAL(&s_lock);
}

int settings_set(const kcfg_t *cfg)
{
    if (!valid(cfg)) {
        ESP_LOGW(TAG, "rejected settings from the app (invalid values)");
        return 1;
    }
    apply(cfg);
    int r = save(cfg);
    ESP_LOGI(TAG, "new settings applied%s (hipfire %.1f cm/360, ADS %s)", r ? " but NOT saved" : " and saved",
             cfg->hip.cm_per_360, cfg->ads_enabled ? "on" : "off");
    return r;
}

int settings_reset(void)
{
    apply(&KESTREL_FACTORY_DEFAULTS);
    int r = save(&KESTREL_FACTORY_DEFAULTS);
    ESP_LOGI(TAG, "settings reset to factory defaults");
    return r;
}

uint32_t settings_generation(void)
{
    return s_gen;
}
