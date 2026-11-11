// Project Kestrel - Xbox One GIP message helpers
//
// C port of GP2040-CE's XGIPProtocol (src/drivers/shared/xgip_protocol.cpp).
// Original: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info), MIT License.

#include "gip.h"

#include <string.h>

// GIP lengths use unsigned LEB128
static bool read_leb128(const uint8_t *buf, uint16_t len, uint16_t *offset, uint32_t *value)
{
    *value = 0;
    for (uint8_t shift = 0; shift < 28 && *offset < len; shift += 7) {
        uint8_t b = buf[(*offset)++];
        *value |= (uint32_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) return true;
    }
    return false;
}

void gip_reset(gip_msg_t *m)
{
    memset(m, 0, sizeof(*m));
}

bool gip_parse(gip_msg_t *m, const uint8_t *buf, uint16_t len)
{
    m->valid = false;
    if (buf == NULL || len < sizeof(gip_header_t) || len > GIP_PACKET_MAX) {
        gip_reset(m);
        return false;
    }

    gip_header_t h;
    memcpy(&h, buf, sizeof(h));
    uint16_t off = 3;
    uint32_t plen = 0, chunk_off = 0;
    if (!read_leb128(buf, len, &off, &plen) ||
        (h.chunked && !read_leb128(buf, len, &off, &chunk_off)) ||
        plen > (uint32_t)(len - off)) {
        return false;
    }

    if (h.command == GIP_CMD_ACK) {
        if (h.internal != 1 || h.chunked || plen != 9 || off + plen != len) return false;
        m->header = h;           // ACKs must not discard an incoming fragmented message
        m->packet_len = len;
        m->valid = true;
        return true;
    }

    if (h.chunked) {
        if (h.chunk_start) {
            if (plen == 0 || chunk_off > GIP_DATA_MAX || plen > chunk_off) return false;
            gip_reset(m);
            m->data_len = (uint16_t)chunk_off;
            chunk_off = 0;
        } else {
            if (chunk_off > m->actual_rx || chunk_off > m->data_len || plen > m->data_len - chunk_off) return false;
            if (plen == 0) {
                if (off == 5 && len == 6 && buf[5] == 0) off++;   // some devices pad the terminal header
                if (off != len || chunk_off != m->data_len || m->actual_rx != m->data_len) return false;
                m->header = h;
                m->packet_len = len;
                m->chunk_ended = true;
                m->valid = true;
                return true;
            }
            if (m->chunks_sent == 0) return false;
        }
        memcpy(&m->data[chunk_off], &buf[off], plen);
        m->actual_rx = (uint16_t)(chunk_off + plen);
        m->chunks_sent++;
        m->chunk_ended = false;
    } else {
        gip_reset(m);
        memcpy(m->data, &buf[off], plen);
        m->actual_rx = (uint16_t)plen;
        m->data_len = (uint16_t)plen;
    }
    m->header = h;
    m->packet_len = len;
    m->valid = true;
    return false;
}

void gip_set_attributes(gip_msg_t *m, uint8_t cmd, uint8_t seq, bool internal, bool chunked, bool needs_ack)
{
    m->header.reserved = 0;
    m->header.command = cmd;
    m->header.sequence = seq;
    m->header.internal = internal;
    m->header.chunked = chunked;
    m->header.needs_ack = needs_ack;
}

bool gip_set_data(gip_msg_t *m, const uint8_t *buf, uint16_t len)
{
    if ((len && buf == NULL) || len > GIP_DATA_MAX ||
        (!m->header.chunked && len > GIP_PACKET_MAX - sizeof(gip_header_t))) {
        return false;
    }
    if (len) memcpy(m->data, buf, len);
    m->data_len = len;
    return true;
}

uint8_t *gip_generate(gip_msg_t *m)
{
    if (m->header.chunked && m->data_len < GIP_MAX_CHUNK_SIZE) {
        m->header.chunk_start = 0;
        m->header.chunked = 0;
        m->header.needs_ack = 1;
    }
    if (!m->header.chunked) {
        m->header.length = (uint8_t)m->data_len;
        memcpy(m->packet, &m->header, sizeof(gip_header_t));
        memcpy(&m->packet[4], m->data, m->data_len);
        m->packet_len = (uint16_t)(sizeof(gip_header_t) + m->data_len);
    } else {
        uint16_t to_send = m->data_len - m->total_sent;
        m->header.chunk_start = (m->chunks_sent == 0);
        // ACK the first, every fifth, and final data fragment
        m->header.needs_ack = (m->chunks_sent == 0 || (m->chunks_sent + 1) % 5 == 0 ||
                               to_send <= GIP_MAX_CHUNK_SIZE);
        if (to_send > GIP_MAX_CHUNK_SIZE) to_send = GIP_MAX_CHUNK_SIZE;
        if (to_send == 0) {
            m->header.needs_ack = 0;
            m->header.chunk_start = 0;
            m->chunk_ended = true;
        }
        uint16_t chunk_off = m->header.chunk_start ? m->data_len : m->total_sent;
        m->header.length = (uint8_t)to_send;
        if (to_send != 0 && chunk_off < 0x80) {
            m->header.length |= 0x80;     // keep a six-byte header
            memcpy(m->packet, &m->header, sizeof(gip_header_t));
            m->packet[4] = 0;
            m->packet[5] = (uint8_t)chunk_off;
        } else {
            memcpy(m->packet, &m->header, sizeof(gip_header_t));
            m->packet[4] = (uint8_t)((chunk_off & 0x7F) | 0x80);
            m->packet[5] = (uint8_t)(chunk_off >> 7);
        }
        memcpy(&m->packet[6], &m->data[m->total_sent], to_send);
        m->packet_len = (uint16_t)(sizeof(gip_header_t) + 2 + to_send);
        m->total_sent += to_send;
        if (to_send != 0) m->chunks_sent++;
    }
    return m->packet;
}

uint8_t *gip_generate_ack(gip_msg_t *m)
{
    uint8_t *p = m->packet;
    p[0] = GIP_CMD_ACK;
    p[1] = 0x20;
    p[2] = m->header.sequence;
    p[3] = 0x09;
    p[4] = 0x00;
    p[5] = m->header.command;
    p[6] = 0x20;
    uint16_t rx = m->actual_rx;
    p[7] = (uint8_t)(rx & 0xFF);
    p[8] = (uint8_t)(rx >> 8);
    p[9] = 0x00;
    p[10] = 0x00;
    if (m->header.chunked) {
        uint16_t left = m->data_len - rx;
        p[11] = (uint8_t)(left & 0xFF);
        p[12] = (uint8_t)(left >> 8);
    } else {
        p[11] = 0;
        p[12] = 0;
    }
    m->packet_len = 13;
    return p;
}
