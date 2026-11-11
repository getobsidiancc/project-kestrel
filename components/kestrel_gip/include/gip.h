// Project Kestrel - Xbox One GIP (Game Input Protocol) message helpers
//
// C port of GP2040-CE's XGIPProtocol (src/drivers/shared/xgip_protocol.cpp).
// Original: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info), MIT License.
// See LICENSE-GP2040-CE.txt in this component.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GIP_PACKET_MAX     64
#define GIP_MAX_CHUNK_SIZE 0x3A    // 64-byte USB packet minus a six-byte fragmented header
#define GIP_DATA_MAX       1024

// Commands
#define GIP_CMD_ACK            0x01
#define GIP_CMD_ANNOUNCE       0x02
#define GIP_CMD_KEEPALIVE      0x03
#define GIP_CMD_DESCRIPTOR     0x04
#define GIP_CMD_POWER_MODE     0x05
#define GIP_CMD_AUTH           0x06
#define GIP_CMD_VIRTUAL_KEY    0x07
#define GIP_CMD_RUMBLE         0x09
#define GIP_CMD_LED            0x0A
#define GIP_CMD_FINAL_AUTH     0x1E
#define GIP_CMD_INPUT          0x20

typedef struct __attribute__((packed)) {
    uint8_t command;
    uint8_t client : 3;
    uint8_t reserved : 1;
    uint8_t needs_ack : 1;
    uint8_t internal : 1;
    uint8_t chunk_start : 1;
    uint8_t chunked : 1;
    uint8_t sequence;
    uint8_t length;
} gip_header_t;

typedef struct {
    gip_header_t header;
    uint16_t actual_rx;
    uint16_t total_sent;
    uint16_t chunks_sent;
    bool chunk_ended;
    bool valid;
    uint8_t packet[GIP_PACKET_MAX];
    uint16_t packet_len;
    uint8_t data[GIP_DATA_MAX];
    uint16_t data_len;
} gip_msg_t;

void     gip_reset(gip_msg_t *m);
// Parse one USB packet into m (handles fragmented messages). Check m->valid afterwards.
bool     gip_parse(gip_msg_t *m, const uint8_t *buf, uint16_t len);
void     gip_set_attributes(gip_msg_t *m, uint8_t cmd, uint8_t seq, bool internal, bool chunked, bool needs_ack);
bool     gip_set_data(gip_msg_t *m, const uint8_t *buf, uint16_t len);
// Build the next outgoing packet (the next fragment if chunked) into m->packet / m->packet_len.
uint8_t *gip_generate(gip_msg_t *m);
// Build an ACK for the last parsed packet into m->packet / m->packet_len.
uint8_t *gip_generate_ack(gip_msg_t *m);

static inline bool gip_ack_required(const gip_msg_t *m) { return m->header.needs_ack; }
static inline bool gip_end_of_chunk(const gip_msg_t *m) { return m->chunk_ended; }

// GIP_AUTH / GIP_FINAL_AUTH, or an ACK of one (acked command in byte 5)
static inline bool gip_is_auth_packet(const uint8_t *d, uint16_t len)
{
    if (len < 4) return false;
    if (d[0] == GIP_CMD_AUTH || d[0] == GIP_CMD_FINAL_AUTH) return true;
    return d[0] == GIP_CMD_ACK && len >= 6 && (d[5] == GIP_CMD_AUTH || d[5] == GIP_CMD_FINAL_AUTH);
}

#ifdef __cplusplus
}
#endif
