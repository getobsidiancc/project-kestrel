// Project Kestrel - chip-to-chip link (UART framing + CRC)

#include "kestrel_link.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "klink";

static klink_rx_cb_t s_cb;
static void *s_cb_arg;
static SemaphoreHandle_t s_tx_lock;
static volatile uint32_t s_rx_frames;
static volatile uint32_t s_rx_errors;

static uint16_t crc16_ccitt(uint16_t crc, const uint8_t *data, size_t len)
{
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

esp_err_t klink_send(uint8_t type, const void *payload, uint8_t len)
{
    if (len > KLINK_MAX_PAYLOAD) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t frame[4 + KLINK_MAX_PAYLOAD + 2];
    frame[0] = KLINK_SOF0;
    frame[1] = KLINK_SOF1;
    frame[2] = type;
    frame[3] = len;
    if (len) {
        memcpy(&frame[4], payload, len);
    }
    uint16_t crc = crc16_ccitt(0xFFFF, &frame[2], (size_t)len + 2);
    frame[4 + len] = (uint8_t)(crc & 0xFF);
    frame[5 + len] = (uint8_t)(crc >> 8);

    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    int n = uart_write_bytes(KLINK_UART_NUM, frame, (size_t)len + 6);
    xSemaphoreGive(s_tx_lock);
    return (n == len + 6) ? ESP_OK : ESP_FAIL;
}

typedef enum { ST_SOF0, ST_SOF1, ST_TYPE, ST_LEN, ST_PAYLOAD, ST_CRC0, ST_CRC1 } rx_state_t;

static void klink_rx_task(void *arg)
{
    (void)arg;
    uint8_t buf[128];
    rx_state_t st = ST_SOF0;
    uint8_t type = 0, len = 0, idx = 0, crc_lo = 0;
    uint8_t payload[KLINK_MAX_PAYLOAD];

    for (;;) {
        int n = uart_read_bytes(KLINK_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(10));
        for (int i = 0; i < n; i++) {
            uint8_t b = buf[i];
            switch (st) {
            case ST_SOF0:
                if (b == KLINK_SOF0) st = ST_SOF1;
                break;
            case ST_SOF1:
                st = (b == KLINK_SOF1) ? ST_TYPE : (b == KLINK_SOF0 ? ST_SOF1 : ST_SOF0);
                break;
            case ST_TYPE:
                type = b;
                st = ST_LEN;
                break;
            case ST_LEN:
                len = b;
                idx = 0;
                if (len > KLINK_MAX_PAYLOAD) {
                    s_rx_errors++;
                    st = ST_SOF0;
                } else {
                    st = len ? ST_PAYLOAD : ST_CRC0;
                }
                break;
            case ST_PAYLOAD:
                payload[idx++] = b;
                if (idx >= len) st = ST_CRC0;
                break;
            case ST_CRC0:
                crc_lo = b;
                st = ST_CRC1;
                break;
            case ST_CRC1: {
                uint8_t hdr[2] = { type, len };
                uint16_t crc = crc16_ccitt(0xFFFF, hdr, 2);
                crc = crc16_ccitt(crc, payload, len);
                uint16_t got = (uint16_t)crc_lo | ((uint16_t)b << 8);
                if (crc == got) {
                    s_rx_frames++;
                    if (s_cb) s_cb(type, payload, len, s_cb_arg);
                } else {
                    s_rx_errors++;
                }
                st = ST_SOF0;
                break;
            }
            }
        }
    }
}

esp_err_t klink_init(bool is_left, klink_rx_cb_t cb, void *cb_arg)
{
    s_cb = cb;
    s_cb_arg = cb_arg;
    s_tx_lock = xSemaphoreCreateMutex();

    const uart_config_t cfg = {
        .baud_rate = KLINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(KLINK_UART_NUM, 2048, 2048, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(KLINK_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(KLINK_UART_NUM,
                                 is_left ? KLINK_LEFT_TX_PIN : KLINK_RIGHT_TX_PIN,
                                 is_left ? KLINK_LEFT_RX_PIN : KLINK_RIGHT_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    BaseType_t ok = xTaskCreatePinnedToCore(klink_rx_task, "klink_rx", 4096, NULL, 10, NULL, 1);
    ESP_LOGI(TAG, "link up (%s side, %d baud)", is_left ? "LEFT" : "RIGHT", KLINK_BAUD);
    return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

uint32_t klink_rx_frames(void) { return s_rx_frames; }
uint32_t klink_rx_errors(void) { return s_rx_errors; }
