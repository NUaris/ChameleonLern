/* SPDX-License-Identifier: GPL-3.0-only */
#include "selection_core.h"
#include <string.h>

static uint32_t hash_bytes(uint32_t hash, const uint8_t *data, size_t length) {
    while (length--) hash = (hash ^ *data++) * 16777619u;
    return hash;
}

static uint32_t crc32(const uint8_t *data, size_t length) {
    uint32_t crc = UINT32_MAX;
    while (length--) {
        crc ^= *data++;
        for (unsigned i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static uint16_t get16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] << 8 | p[1]); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (uint16_t)(v >> 16)); put16(p + 2, (uint16_t)v); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p) << 16 | get16(p + 2); }
static unsigned distance(unsigned a, unsigned b) { return a > b ? a - b : b - a; }

void sel_model_init(sel_model_t *m) {
    memset(m, 0, sizeof(*m));
    m->config.min_score = 70;
    m->config.margin = 12;
    m->config.min_observations = 2;
    m->config.ble_weight = 60;
    m->config.reader_weight = 30;
    m->config.time_weight = 10;
    m->config.scan_period_ms = 8000;
    m->config.scan_window_ms = 1200;
    for (unsigned i = 0; i < SEL_SAMPLES; i++) m->samples[i].slot = SEL_NONE;
}

bool sel_config_valid(const sel_config_t *c) {
    return c && c->mode <= SEL_AUTO && c->min_score >= 40 && c->min_score <= 100 &&
           c->margin >= 5 && c->margin <= 50 && c->min_observations >= 1 &&
           c->min_observations <= 20 && c->reserved == 0 &&
           c->ble_weight + c->reader_weight + c->time_weight == 100 &&
           c->ble_weight + c->reader_weight > 0 && c->scan_window_ms >= 200 &&
           c->scan_window_ms <= 5000 && c->scan_period_ms >= c->scan_window_ms + 500 &&
           c->scan_period_ms <= 60000;
}

bool sel_context_valid(const sel_context_t *c) {
    if (!c || c->beacon_count > SEL_BEACONS || c->reader_count > SEL_READER_TOKENS ||
        c->field > 2 || c->time_valid > 1 || (c->time_valid && (c->minute >= 1440 || c->weekday > 6)))
        return false;
    for (unsigned i = 0; i < c->beacon_count; i++) {
        if (!c->beacons[i].key || c->beacons[i].rssi > 0 || c->beacons[i].rssi < -127) return false;
        for (unsigned j = 0; j < i; j++)
            if (c->beacons[i].key == c->beacons[j].key) return false;
    }
    return true;
}

static unsigned ble_similarity(const sel_context_t *a, const sel_context_t *b) {
    unsigned sum = 0, matches = 0;
    for (unsigned i = 0; i < a->beacon_count; i++) {
        for (unsigned j = 0; j < b->beacon_count; j++) {
            if (a->beacons[i].key == b->beacons[j].key) {
                unsigned d = distance((unsigned)(a->beacons[i].rssi + 127),
                                      (unsigned)(b->beacons[j].rssi + 127));
                sum += d >= 40 ? 40 : 100 - d * 3 / 2;
                matches++;
                break;
            }
        }
    }
    unsigned union_count = a->beacon_count + b->beacon_count - matches;
    return union_count ? sum / union_count : 0;
}

static unsigned reader_similarity(const sel_context_t *a, const sel_context_t *b) {
    if (!a->reader_count || !b->reader_count || a->field != b->field) return 0;
    unsigned max = a->reader_count > b->reader_count ? a->reader_count : b->reader_count;
    unsigned min = a->reader_count < b->reader_count ? a->reader_count : b->reader_count;
    unsigned matches = 0;
    for (unsigned i = 0; i < min; i++) {
        /* Low three bits are a coarse timing bucket; command/block identity dominates. */
        if ((a->reader[i] >> 3) == (b->reader[i] >> 3)) {
            matches += a->reader[i] == b->reader[i] ? 100 : 85;
        }
    }
    return matches / max;
}

static unsigned time_similarity(const sel_context_t *a, const sel_context_t *b) {
    unsigned d = distance(a->minute, b->minute);
    if (d > 720) d = 1440 - d;
    unsigned score = d >= 180 ? 0 : 100 - d * 100 / 180;
    if (a->weekday != b->weekday) {
        bool same_kind = (a->weekday < 5) == (b->weekday < 5);
        score = score * (same_kind ? 80 : 30) / 100;
    }
    return score;
}

static unsigned similarity(const sel_config_t *cfg, const sel_context_t *a,
                           const sel_context_t *b, uint8_t *evidence) {
    unsigned total = 0, weight = 0;
    *evidence = 0;
    if (a->beacon_count && b->beacon_count && cfg->ble_weight) {
        unsigned s = ble_similarity(a, b);
        total += s * cfg->ble_weight; weight += cfg->ble_weight;
        if (s >= 50) *evidence |= 1;
    }
    if (a->reader_count && b->reader_count && cfg->reader_weight) {
        unsigned s = reader_similarity(a, b);
        total += s * cfg->reader_weight; weight += cfg->reader_weight;
        /* Poll/anticollision alone is shared by many readers. Require an auth/RATS token. */
        bool specific = false;
        for (unsigned i = 0; i < a->reader_count; i++)
            if ((a->reader[i] >> 11) >= 4) specific = true;
        if (s >= 70 && specific) *evidence |= 2;
    }
    if (a->time_valid && b->time_valid && cfg->time_weight) {
        total += time_similarity(a, b) * cfg->time_weight; weight += cfg->time_weight;
        *evidence |= 4;
    }
    /* A clock match alone never identifies a card. */
    return weight && (*evidence & 3) ? total / weight : 0;
}

uint8_t sel_sample_count(const sel_model_t *m) {
    uint8_t count = 0;
    for (unsigned i = 0; i < SEL_SAMPLES; i++) if (m->samples[i].slot != SEL_NONE) count++;
    return count;
}

bool sel_learn(sel_model_t *m, const sel_context_t *c, uint8_t slot) {
    if (!m || slot >= SEL_SLOTS || !sel_context_valid(c) || (!c->beacon_count && !c->reader_count)) return false;
    unsigned chosen = SEL_SAMPLES;
    uint32_t oldest = UINT32_MAX;
    bool duplicate = false;
    for (unsigned i = 0; i < SEL_SAMPLES; i++) {
        sel_sample_t *s = &m->samples[i];
        uint8_t evidence;
        if (s->slot == slot && s->context.time_valid == c->time_valid &&
            s->context.beacon_count == c->beacon_count && s->context.reader_count == c->reader_count &&
            s->context.field == c->field && similarity(&m->config, c, &s->context, &evidence) >= 85) {
            chosen = i; duplicate = true; break;
        }
        if (s->slot == SEL_NONE) { chosen = i; oldest = 0; }
        else if (oldest && s->order < oldest) { oldest = s->order; chosen = i; }
    }
    if (chosen == SEL_SAMPLES) return false;
    sel_sample_t *s = &m->samples[chosen];
    uint8_t observations = duplicate ? s->observations : 0;
    memset(s, 0, sizeof(*s));
    s->context = *c;
    s->slot = slot;
    s->observations = observations < 255 ? observations + 1 : 255;
    s->order = ++m->order;
    if (!m->order) {
        /* Preserve ordering on the rare uint32 wrap. */
        for (unsigned i = 0; i < SEL_SAMPLES; i++) m->samples[i].order = i + 1;
        m->order = SEL_SAMPLES + 1;
        s->order = m->order;
    }
    return true;
}

void sel_forget(sel_model_t *m, uint8_t slot) {
    for (unsigned i = 0; i < SEL_SAMPLES; i++) {
        if (slot == SEL_NONE || m->samples[i].slot == slot) {
            memset(&m->samples[i], 0, sizeof(m->samples[i]));
            m->samples[i].slot = SEL_NONE;
        }
    }
}

sel_prediction_t sel_predict(const sel_model_t *m, const sel_context_t *c, uint8_t eligible) {
    sel_prediction_t p;
    memset(&p, 0, sizeof(p)); p.slot = SEL_NONE; p.reason = SEL_REASON_EMPTY;
    uint8_t support[SEL_SLOTS] = {0}, evidence[SEL_SLOTS] = {0};
    if (!sel_context_valid(c) || (!c->beacon_count && !c->reader_count)) {
        p.reason = SEL_REASON_NO_CONTEXT; return p;
    }
    for (unsigned i = 0; i < SEL_SAMPLES; i++) {
        const sel_sample_t *s = &m->samples[i];
        if (s->slot >= SEL_SLOTS || !(eligible & (1u << s->slot))) continue;
        uint8_t e;
        unsigned score = similarity(&m->config, c, &s->context, &e);
        if (score > p.scores[s->slot] || (score == p.scores[s->slot] && s->observations > support[s->slot])) {
            p.scores[s->slot] = (uint8_t)score; support[s->slot] = s->observations; evidence[s->slot] = e;
        }
    }
    unsigned second = 0;
    for (unsigned i = 0; i < SEL_SLOTS; i++) {
        if (p.scores[i] > p.score) { second = p.score; p.score = p.scores[i]; p.slot = (uint8_t)i; }
        else if (p.scores[i] > second) second = p.scores[i];
    }
    if (p.slot == SEL_NONE) return p;
    p.margin = (uint8_t)(p.score - second); p.observations = support[p.slot]; p.evidence = evidence[p.slot];
    if (p.score < m->config.min_score) p.reason = SEL_REASON_LOW_SCORE;
    else if (p.margin < m->config.margin) p.reason = SEL_REASON_AMBIGUOUS;
    else if (p.observations < m->config.min_observations) p.reason = SEL_REASON_SUPPORT;
    else p.reason = SEL_REASON_READY;
    return p;
}

bool sel_model_encode(const sel_model_t *m, uint8_t *d, size_t length) {
    if (!m || !d || length != SEL_MODEL_BYTES || !sel_config_valid(&m->config)) return false;
    memset(d, 0, length); memcpy(d, "CLRN", 4); d[4] = 1; put16(d + 6, (uint16_t)length);
    put32(d + 12, m->order);
    const sel_config_t *cfg = &m->config;
    d[16] = cfg->mode; d[17] = cfg->min_score; d[18] = cfg->margin; d[19] = cfg->min_observations;
    d[20] = cfg->ble_weight; d[21] = cfg->reader_weight; d[22] = cfg->time_weight;
    put16(d + 24, cfg->scan_period_ms); put16(d + 26, cfg->scan_window_ms);
    d[28] = sel_sample_count(m);
    for (unsigned i = 0; i < SEL_SAMPLES; i++) {
        const sel_sample_t *s = &m->samples[i]; const sel_context_t *c = &s->context;
        uint8_t *p = d + 32 + i * 68;
        p[60] = s->slot;
        if (s->slot == SEL_NONE) continue;
        if (s->slot >= SEL_SLOTS || !s->observations || !sel_context_valid(c)) return false;
        p[0] = c->beacon_count; p[1] = c->reader_count; p[2] = c->field; p[3] = c->time_valid;
        put16(p + 4, c->minute); p[6] = c->weekday;
        for (unsigned j = 0; j < c->beacon_count; j++) { put32(p + 8 + j * 6, c->beacons[j].key); p[12 + j * 6] = (uint8_t)c->beacons[j].rssi; }
        for (unsigned j = 0; j < c->reader_count; j++) put16(p + 44 + j * 2, c->reader[j]);
        p[61] = s->observations; put32(p + 64, s->order);
    }
    put32(d + 8, crc32(d + 12, length - 12));
    return true;
}

bool sel_model_decode(sel_model_t *m, const uint8_t *d, size_t length) {
    if (!m || !d || length != SEL_MODEL_BYTES || memcmp(d, "CLRN", 4) || d[4] != 1 || d[5] ||
        get16(d + 6) != length || get32(d + 8) != crc32(d + 12, length - 12)) return false;
    sel_model_t tmp; sel_model_init(&tmp); tmp.order = get32(d + 12);
    sel_config_t *cfg = &tmp.config;
    cfg->mode = d[16]; cfg->min_score = d[17]; cfg->margin = d[18]; cfg->min_observations = d[19];
    cfg->ble_weight = d[20]; cfg->reader_weight = d[21]; cfg->time_weight = d[22]; cfg->reserved = d[23];
    cfg->scan_period_ms = get16(d + 24); cfg->scan_window_ms = get16(d + 26);
    if (!sel_config_valid(cfg)) return false;
    for (unsigned i = 0; i < SEL_SAMPLES; i++) {
        const uint8_t *p = d + 32 + i * 68; sel_sample_t *s = &tmp.samples[i]; sel_context_t *c = &s->context;
        s->slot = p[60]; if (s->slot == SEL_NONE) continue;
        s->observations = p[61]; s->order = get32(p + 64);
        c->beacon_count = p[0]; c->reader_count = p[1]; c->field = p[2]; c->time_valid = p[3];
        c->minute = get16(p + 4); c->weekday = p[6];
        if (s->slot >= SEL_SLOTS || !s->observations || c->beacon_count > SEL_BEACONS || c->reader_count > SEL_READER_TOKENS) return false;
        for (unsigned j = 0; j < c->beacon_count; j++) { c->beacons[j].key = get32(p + 8 + j * 6); c->beacons[j].rssi = (int8_t)p[12 + j * 6]; }
        for (unsigned j = 0; j < c->reader_count; j++) c->reader[j] = get16(p + 44 + j * 2);
        if (!sel_context_valid(c)) return false;
    }
    if (sel_sample_count(&tmp) != d[28]) return false;
    *m = tmp; return true;
}

uint32_t sel_ble_identity(const uint8_t address[6], uint8_t type, const uint8_t *ad, size_t length) {
    if (!address || (!ad && length)) return 0;
    uint32_t hash = 2166136261u;
    if (type == 0 || (type == 1 && (address[5] & 0xc0) == 0xc0)) {
        hash = hash_bytes(hash, &type, 1); hash = hash_bytes(hash, address, 6);
        return hash ? hash : 1;
    }
    /* Rotating private addresses are excluded. Hash only stable AD identifiers. */
    bool found = false;
    for (size_t pos = 0; pos < length;) {
        size_t n = ad[pos++]; if (!n) break;
        if (n > length - pos) return 0;
        uint8_t kind = ad[pos]; const uint8_t *payload = ad + pos + 1;
        size_t count = n - 1;
        if (((kind == 0x09 || kind == 0x08) && count >= 4) || ((kind == 0x06 || kind == 0x07) && count >= 16)) {
            hash = hash_bytes(hash, &kind, 1); hash = hash_bytes(hash, payload, count); found = count > 0 || found;
        } else if (kind == 0xff && count >= 25 && payload[0] == 0x4c && payload[1] == 0 &&
                   payload[2] == 2 && payload[3] == 0x15) {
            /* iBeacon UUID, major/minor are stable; exclude calibrated power. */
            hash = hash_bytes(hash, &kind, 1); hash = hash_bytes(hash, payload, 24); found = true;
        } else if (kind == 0x16 && count >= 20 && payload[0] == 0xaa && payload[1] == 0xfe && payload[2] == 0) {
            /* Eddystone UID: namespace + instance, excluding its power byte. */
            hash = hash_bytes(hash, &kind, 1); hash = hash_bytes(hash, payload + 4, 16); found = true;
        }
        pos += n;
    }
    return found ? (hash ? hash : 1) : 0;
}

bool sel_clock_sync(sel_clock_t *clock, uint32_t utc, int16_t timezone, uint32_t now) {
    if (!clock || utc < 1577836800u || utc > 4102444800u || timezone < -720 || timezone > 840) return false;
    clock->utc = utc; clock->timezone_minutes = timezone; clock->last_ms = now;
    clock->remainder_ms = 0; clock->valid = true; return true;
}

void sel_clock_update(sel_clock_t *clock, uint32_t now) {
    if (!clock->valid) return;
    uint64_t elapsed = (uint32_t)(now - clock->last_ms) + (uint64_t)clock->remainder_ms;
    clock->utc += (uint32_t)(elapsed / 1000); clock->remainder_ms = (uint16_t)(elapsed % 1000); clock->last_ms = now;
}

void sel_clock_context(const sel_clock_t *clock, sel_context_t *c) {
    c->time_valid = clock->valid;
    if (!clock->valid) { c->minute = 0; c->weekday = 0; return; }
    uint64_t local = (uint64_t)((int64_t)clock->utc + clock->timezone_minutes * 60);
    c->minute = (uint16_t)((local % 86400) / 60);
    c->weekday = (uint8_t)((local / 86400 + 3) % 7); /* Monday = 0; epoch was Thursday. */
}
