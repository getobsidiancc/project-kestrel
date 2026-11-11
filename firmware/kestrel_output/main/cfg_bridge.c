// Project Kestrel - config app bridge (LEFT chip), see cfg_bridge.h
//
// Everything sent to the PC (log lines, @KST replies, live view) goes through a ring buffer
// that a low-priority task drains to the UART. Writers never wait: if the PC link is busy
// the text is dropped. That way the USB/controller path can never be held up by the PC.

#include "cfg_bridge.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "kestrel_cfg.h"
#include "output.h"

static const char *TAG = "cfg";

#define PC_UART UART_NUM_0
#define CMD_LINE_MAX 1100
#define PC_OUT_BUF 16384
#define PC_BAUD 921600    // the config app, monitor.ps1 and test scripts use this speed

static const input_status_t *s_input_status;
static int s_left_fw;
static volatile bool s_live;

static RingbufHandle_t s_out;
static volatile uint32_t s_dropped;

static portMUX_TYPE s_pad_lock = portMUX_INITIALIZER_UNLOCKED;
static pad_state_t s_pad_copy;

static uint8_t s_get_buf[sizeof(kcfg_t)];
static uint16_t s_get_len;
static volatile bool s_waiting_result;
static volatile int64_t s_result_deadline;

// ---- Non-blocking output -------------------------------------------------------------

void pc_write(const char *s, size_t len)
{
    if (!s_out || len == 0) return;
    if (xRingbufferSend(s_out, s, len, 0) != pdTRUE) s_dropped++;
}

static void pc_printf(const char *fmt, ...)
{
    char line[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n > 0) pc_write(line, n < (int)sizeof(line) ? (size_t)n : sizeof(line) - 1);
}

// ESP_LOG output goes through the ring buffer too
static int log_vprintf(const char *fmt, va_list ap)
{
    char line[256];
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    if (n > 0) pc_write(line, n < (int)sizeof(line) ? (size_t)n : sizeof(line) - 1);
    return n;
}

static void pc_tx_task(void *arg)
{
    (void)arg;
    int64_t last_pad = 0;
    for (;;) {
        size_t len = 0;
        uint8_t *data = xRingbufferReceiveUpTo(s_out, &len, pdMS_TO_TICKS(10), 1024);
        if (data) {
            uart_write_bytes(PC_UART, data, len);
            vRingbufferReturnItem(s_out, data);
        }
        // Live view, ~30 times a second
        int64_t now = esp_timer_get_time();
        if (s_live && now - last_pad >= 33000) {
            last_pad = now;
            pad_state_t p;
            taskENTER_CRITICAL(&s_pad_lock);
            p = s_pad_copy;
            taskEXIT_CRITICAL(&s_pad_lock);
            pc_printf("@KST PAD %u %u %u %d %d %d %d\n", p.buttons, p.lt, p.rt, p.lx, p.ly, p.rx, p.ry);
        }
    }
}

// ---- Commands from the PC --------------------------------------------------------------

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static void print_hex_line(const char *prefix, const uint8_t *d, uint16_t n)
{
    static char line[16 + 2 * sizeof(kcfg_t) + 2];
    int p = snprintf(line, sizeof(line), "%s", prefix);
    for (uint16_t i = 0; i < n; i++) p += snprintf(line + p, sizeof(line) - p, "%02x", d[i]);
    line[p++] = '\n';
    pc_write(line, (size_t)p);
}

static void start_waiting(void)
{
    s_waiting_result = true;
    s_result_deadline = esp_timer_get_time() + 2000000;
}

static void handle_line(char *line)
{
    size_t n = strlen(line);
    while (n && (line[n - 1] == '\r' || line[n - 1] == '\n' || line[n - 1] == ' ')) line[--n] = 0;
    if (strncmp(line, "KST ", 4) != 0) return;
    char *cmd = line + 4;

    if (strcmp(cmd, "PING") == 0) {
        pc_printf("@KST PONG %d %d %s\n", s_left_fw, s_input_status ? s_input_status->fw_version : 0,
                  KESTREL_OUTPUT_MODE == OUTPUT_XBOX_ONE ? "xbox_one" : "xinput");
    } else if (strcmp(cmd, "GET") == 0) {
        s_get_len = 0;
        klink_send(KLINK_MSG_CFG_GET, NULL, 0);
    } else if (strncmp(cmd, "SET ", 4) == 0) {
        const char *hex = cmd + 4;
        if (strlen(hex) != 2 * sizeof(kcfg_t)) {
            pc_printf("@KST RESULT 1\n");
            return;
        }
        static uint8_t buf[sizeof(kcfg_t)];
        for (size_t i = 0; i < sizeof(kcfg_t); i++) {
            int hi = hexval(hex[2 * i]), lo = hexval(hex[2 * i + 1]);
            if (hi < 0 || lo < 0) {
                pc_printf("@KST RESULT 1\n");
                return;
            }
            buf[i] = (uint8_t)(hi << 4 | lo);
        }
        for (uint16_t off = 0; off < sizeof(kcfg_t); off += 48) {
            uint8_t chunk[50];
            uint16_t c = sizeof(kcfg_t) - off > 48 ? 48 : sizeof(kcfg_t) - off;
            chunk[0] = (uint8_t)(off & 0xFF);
            chunk[1] = (uint8_t)(off >> 8);
            memcpy(&chunk[2], &buf[off], c);
            klink_send(KLINK_MSG_CFG_SET, chunk, (uint8_t)(c + 2));
        }
        uint8_t total[2] = { (uint8_t)(sizeof(kcfg_t) & 0xFF), (uint8_t)(sizeof(kcfg_t) >> 8) };
        start_waiting();
        klink_send(KLINK_MSG_CFG_COMMIT, total, 2);
    } else if (strcmp(cmd, "DEFAULTS") == 0) {
        start_waiting();
        klink_send(KLINK_MSG_CFG_DEFAULTS, NULL, 0);
    } else if (strncmp(cmd, "CAL ", 4) == 0) {
        int v = atoi(cmd + 4);
        if (v > 1000) v = 1000;
        int16_t s = (int16_t)(v < 0 ? -1 : v);
        uint8_t p[2] = { (uint8_t)(s & 0xFF), (uint8_t)((uint16_t)s >> 8) };
        klink_send(KLINK_MSG_CAL, p, 2);
        pc_printf("@KST CAL %d\n", s);
    } else if (strncmp(cmd, "LIVE ", 5) == 0) {
        uint8_t on = atoi(cmd + 5) ? 1 : 0;
        s_live = on;
        klink_send(KLINK_MSG_LIVE, &on, 1);
        pc_printf("@KST LIVE %d\n", on);
    } else {
        pc_printf("@KST ERR unknown command\n");
    }
}

static void pc_rx_task(void *arg)
{
    (void)arg;
    static char line[CMD_LINE_MAX + 1];
    size_t len = 0;
    uint8_t buf[128];
    for (;;) {
        int n = uart_read_bytes(PC_UART, buf, sizeof(buf), pdMS_TO_TICKS(50));
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r') {
                if (len) {
                    line[len] = 0;
                    handle_line(line);
                    len = 0;
                }
            } else if (len < CMD_LINE_MAX) {
                line[len++] = c;
            } else {
                len = 0;   // too long, drop
            }
        }
        if (s_waiting_result && esp_timer_get_time() > s_result_deadline) {
            s_waiting_result = false;
            pc_printf("@KST RESULT 3\n");
        }
    }
}

// ---- Messages from the RIGHT chip ---------------------------------------------------------

void cfg_bridge_link_rx(uint8_t type, const uint8_t *p, uint8_t len)
{
    switch (type) {
    case KLINK_MSG_CFG_DATA:
        if (len >= 2) {
            uint16_t off = (uint16_t)(p[0] | (p[1] << 8)), n = len - 2;
            if (off + n <= sizeof(s_get_buf)) {
                memcpy(&s_get_buf[off], &p[2], n);
                if (off + n > s_get_len) s_get_len = off + n;
            }
        }
        break;
    case KLINK_MSG_CFG_END:
        if (len >= 2 && (uint16_t)(p[0] | (p[1] << 8)) == s_get_len) {
            print_hex_line("@KST CFG ", s_get_buf, s_get_len);
        } else {
            pc_printf("@KST ERR incomplete settings (%d bytes)\n", s_get_len);
        }
        s_get_len = 0;
        break;
    case KLINK_MSG_CFG_RESULT:
        if (len >= 1) {
            s_waiting_result = false;
            pc_printf("@KST RESULT %d\n", p[0]);
        }
        break;
    case KLINK_MSG_KEYS:
        if (len >= 2) {
            char out[16 + 32 * 3 + 2];
            int q = snprintf(out, sizeof(out), "@KST KEYS %02x ", p[0]);
            uint8_t cnt = p[1];
            for (uint8_t i = 0; i < cnt && 2 + i < len && q < (int)sizeof(out) - 4; i++) {
                q += snprintf(out + q, sizeof(out) - q, i ? ",%02x" : "%02x", p[2 + i]);
            }
            out[q++] = '\n';
            pc_write(out, (size_t)q);
        }
        break;
    default:
        break;
    }
}

void cfg_bridge_pad(const pad_state_t *pad)
{
    if (!s_live) return;
    taskENTER_CRITICAL(&s_pad_lock);
    s_pad_copy = *pad;
    taskEXIT_CRITICAL(&s_pad_lock);
}

void cfg_bridge_start(const input_status_t *input_status, int left_fw)
{
    s_input_status = input_status;
    s_left_fw = left_fw;
    s_out = xRingbufferCreate(PC_OUT_BUF, RINGBUF_TYPE_BYTEBUF);
    ESP_ERROR_CHECK(uart_driver_install(PC_UART, 2048, 8192, 0, NULL, 0));
    uart_set_baudrate(PC_UART, PC_BAUD);   // 8x the boot-time 115200
    esp_log_set_vprintf(log_vprintf);
    xTaskCreatePinnedToCore(pc_tx_task, "pc_tx", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(pc_rx_task, "pc_rx", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "config app bridge ready on the COM port");
}
