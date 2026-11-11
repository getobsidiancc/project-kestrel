// Project Kestrel - OUTPUT side (LEFT chip)
//
// Receives controller state from the RIGHT chip over the internal UART link and
// presents it to the PC/console as a wired controller (Xbox One or Xbox 360 mode,
// see output.h). In Xbox One mode it also relays the console's security handshake
// to the real Xbox controller on the RIGHT chip's hub.

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "kestrel_link.h"
#include "output.h"
#include "cfg_bridge.h"

static const char *TAG = "kestrel_out";

#define LED_GPIO 9
#define FW_VERSION 5
#define LINK_TIMEOUT_US 100000      // no data for 100 ms -> centre sticks, release buttons

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static pad_state_t s_pad;
static volatile int64_t s_last_rx_us;
static TaskHandle_t s_usb_task;
static input_status_t s_input_status;

static void link_rx(uint8_t type, const uint8_t *payload, uint8_t len, void *arg)
{
    (void)arg;
    switch (type) {
    case KLINK_MSG_LOG:
        // RIGHT chip's log text: pass on as-is (never blocks this task)
        pc_write((const char *)payload, len);
        break;
    case KLINK_MSG_INPUT_STATUS:
        if (len >= sizeof(input_status_t)) memcpy(&s_input_status, payload, sizeof(input_status_t));
        break;
    case KLINK_MSG_PAD_STATE:
        if (len >= sizeof(pad_state_t)) {
            taskENTER_CRITICAL(&s_lock);
            memcpy(&s_pad, payload, sizeof(pad_state_t));
            taskEXIT_CRITICAL(&s_lock);
            s_last_rx_us = esp_timer_get_time();
            xTaskNotifyGive(s_usb_task);
        }
        break;
    case KLINK_MSG_CFG_DATA:
    case KLINK_MSG_CFG_END:
    case KLINK_MSG_CFG_RESULT:
    case KLINK_MSG_KEYS:
        cfg_bridge_link_rx(type, payload, len);
        break;
    case KLINK_MSG_GIP_TO_CONSOLE:
    case KLINK_MSG_AUTH_DEV_STATUS:
        output_link_rx(type, payload, len);
        if (type == KLINK_MSG_GIP_TO_CONSOLE) xTaskNotifyGive(s_usb_task);
        break;
    default:
        break;
    }
}

static void usb_report_task(void *arg)
{
    (void)arg;
    pad_state_t pad;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));

        taskENTER_CRITICAL(&s_lock);
        pad = s_pad;
        taskEXIT_CRITICAL(&s_lock);
        if (esp_timer_get_time() - s_last_rx_us > LINK_TIMEOUT_US) {
            memset(&pad, 0, sizeof(pad));   // fail safe: neutral controller
        }
        pad.seq = 0;
        output_poll(&pad);
        cfg_bridge_pad(&pad);
    }
}

static void status_task(void *arg)
{
    (void)arg;
    uint32_t n = 0;
    for (;;) {
        link_status_t st = {
            .usb_mounted = output_mounted(),
            .fw_version = FW_VERSION,
            .auth_done = output_auth_done(),
            .want_auth = output_wants_auth(),
        };
        klink_send(KLINK_MSG_STATUS, &st, sizeof(st));

        bool link_ok = (esp_timer_get_time() - s_last_rx_us) < LINK_TIMEOUT_US;
        // LED: solid = mounted + link up, fast blink = link down, slow blink = not mounted
        n++;
        int level = (link_ok && st.usb_mounted) ? 1 : (!link_ok ? (n & 1) : ((n / 4) & 1));
        gpio_set_level(LED_GPIO, level);

        if ((n % 40) == 0) {
            ESP_LOGI(TAG, "usb=%s link=%s mice=%d keyboards=%d %s rx_ok=%lu rx_err=%lu",
                     st.usb_mounted ? "mounted" : "not mounted", link_ok ? "up" : "DOWN",
                     s_input_status.mice, s_input_status.keyboards, output_status(),
                     (unsigned long)klink_rx_frames(), (unsigned long)klink_rx_errors());
        }
        vTaskDelay(pdMS_TO_TICKS(125));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Project Kestrel output side v%d (%s mode)", FW_VERSION,
             KESTREL_OUTPUT_MODE == OUTPUT_XBOX_ONE ? "Xbox One" : "Xbox 360");

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    xTaskCreatePinnedToCore(usb_report_task, "usb_report", 6144, NULL, 12, &s_usb_task, 1);
    output_start();
    ESP_ERROR_CHECK(klink_init(true, link_rx, NULL));
    xTaskCreatePinnedToCore(status_task, "status", 3072, NULL, 3, NULL, 0);
    cfg_bridge_start(&s_input_status, FW_VERSION);
}
