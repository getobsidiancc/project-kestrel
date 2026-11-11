// Project Kestrel - minimal HID report descriptor decoder (see hidparse.h)

#include "hidparse.h"

#include <string.h>

#define PAGE_GENERIC_DESKTOP 0x01
#define PAGE_KEYBOARD        0x07
#define PAGE_BUTTON          0x09
#define USAGE_X              0x30
#define USAGE_Y              0x31
#define USAGE_WHEEL          0x38

typedef struct {
    uint32_t usage_page;
    int32_t  logical_min, logical_max;
    uint32_t report_size, report_count, report_id;
} globals_t;

int32_t hp_extract(const uint8_t *data, size_t len, uint16_t bitoff, uint8_t size, bool is_signed)
{
    if (size == 0 || size > 32 || (size_t)(bitoff + size) > len * 8) return 0;
    uint32_t v = 0;
    for (uint8_t i = 0; i < size; i++) {
        uint16_t bit = bitoff + i;
        if (data[bit >> 3] & (1u << (bit & 7))) v |= (1u << i);
    }
    if (is_signed && size < 32 && (v & (1u << (size - 1)))) v |= ~((1u << size) - 1);
    return (int32_t)v;
}

static void add(hp_layout_t *o, hp_field_t f)
{
    if (o->nfields < HP_MAX_FIELDS) o->f[o->nfields++] = f;
}

bool hidparse(const uint8_t *desc, size_t len, hp_layout_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!desc || len == 0) return false;

    globals_t g = {0}, stack[4];
    int sp = 0;
    uint32_t usages[16];
    int n_usages = 0;
    uint32_t usage_min = 0, usage_max = 0;
    bool has_min = false, has_max = false;
    static uint16_t offsets[256];
    memset(offsets, 0, sizeof(offsets));

    size_t i = 0;
    while (i < len) {
        uint8_t b = desc[i];
        if (b == 0xFE) {                          // long item: skip
            if (i + 1 >= len) return false;
            i += 3 + desc[i + 1];
            continue;
        }
        uint8_t bsize = (uint8_t)((b & 3) == 3 ? 4 : (b & 3));
        uint8_t type = (b >> 2) & 3;
        uint8_t tag = b >> 4;
        if (i + 1 + bsize > len) return false;
        uint32_t u = 0;
        for (uint8_t k = 0; k < bsize; k++) u |= (uint32_t)desc[i + 1 + k] << (8 * k);
        int32_t s = (int32_t)u;
        if (bsize == 1) s = (int8_t)u;
        else if (bsize == 2) s = (int16_t)u;
        i += 1 + bsize;

        if (type == 1) {                          // global
            switch (tag) {
            case 0: g.usage_page = u; break;
            case 1: g.logical_min = s; break;
            case 2: g.logical_max = (g.logical_min >= 0 && bsize < 4) ? (int32_t)u : s; break;
            case 7: g.report_size = u; break;
            case 8: g.report_id = u & 0xFF; out->uses_report_ids = true; break;
            case 9: g.report_count = u; break;
            case 10: if (sp < 4) stack[sp++] = g; break;
            case 11: if (sp > 0) g = stack[--sp]; break;
            default: break;
            }
        } else if (type == 2) {                   // local
            switch (tag) {
            case 0: if (n_usages < 16) usages[n_usages++] = (bsize == 4) ? u : ((g.usage_page << 16) | u); break;
            case 1: usage_min = u; has_min = true; break;
            case 2: usage_max = u; has_max = true; break;
            default: break;
            }
        } else if (type == 0) {                   // main
            if (tag == 8) {                       // Input
                const bool constant = u & 0x01, variable = u & 0x02;
                const uint8_t rid = (uint8_t)g.report_id;
                const uint16_t base = offsets[rid];
                const uint32_t size = g.report_size, count = g.report_count;
                uint32_t page = g.usage_page;
                if (n_usages > 0 && (usages[0] >> 16)) page = usages[0] >> 16;

                if (!constant && size > 0 && size <= 32 && count > 0 && count <= 255) {
                    if (page == PAGE_KEYBOARD) {
                        hp_field_t f = { .report_id = rid, .size = (uint8_t)size, .count = (uint8_t)count,
                                         .bitoff = base };
                        if (variable) {
                            f.kind = HPF_KEY_BITMAP;
                            f.usage_min = (uint16_t)(has_min ? usage_min : (n_usages ? (usages[0] & 0xFFFF) : 0));
                            f.usage_max = (uint16_t)(f.usage_min + count - 1);
                        } else {
                            f.kind = HPF_KEY_ARRAY;
                            f.usage_min = (uint16_t)(has_min ? usage_min : 0);
                            f.usage_max = (uint16_t)(has_max ? usage_max : (g.logical_max > 0 ? g.logical_max : 255));
                        }
                        add(out, f);
                        out->key_count += (uint16_t)(f.kind == HPF_KEY_BITMAP ? count : (f.usage_max - f.usage_min + 1));
                    } else if (page == PAGE_BUTTON && variable) {
                        hp_field_t f = { .kind = HPF_BUTTONS, .report_id = rid, .size = (uint8_t)size,
                                         .count = (uint8_t)count, .bitoff = base,
                                         .usage_min = (uint16_t)(has_min ? usage_min : (n_usages ? (usages[0] & 0xFFFF) : 1)) };
                        add(out, f);
                    } else if (page == PAGE_GENERIC_DESKTOP && variable) {
                        for (uint32_t k = 0; k < count; k++) {
                            uint32_t us = (k < (uint32_t)n_usages) ? (usages[k] & 0xFFFF)
                                         : (n_usages ? (usages[n_usages - 1] & 0xFFFF) : (has_min ? usage_min + k : 0));
                            hp_field_t f = { .report_id = rid, .size = (uint8_t)size, .count = 1,
                                             .bitoff = (uint16_t)(base + k * size), .is_signed = g.logical_min < 0 };
                            if (us == USAGE_X) { f.kind = HPF_X; add(out, f); out->xy_bits = (uint8_t)size; }
                            else if (us == USAGE_Y) { f.kind = HPF_Y; add(out, f); }
                            else if (us == USAGE_WHEEL) { f.kind = HPF_WHEEL; add(out, f); out->has_wheel = true; }
                        }
                    }
                }
                offsets[rid] = (uint16_t)(base + size * count);
            }
            // Input, Output, Feature, Collection, End Collection all clear the locals
            n_usages = 0;
            has_min = has_max = false;
            usage_min = usage_max = 0;
        }
    }

    bool x = false, y = false;
    for (int k = 0; k < out->nfields; k++) {
        if (out->f[k].kind == HPF_X) x = true;
        if (out->f[k].kind == HPF_Y) y = true;
    }
    out->has_xy = x && y;
    return true;
}
