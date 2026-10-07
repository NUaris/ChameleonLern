/* SPDX-License-Identifier: GPL-3.0-only */
#include "selection.h"
#include "tag_emulation.h"
#include "rfid_main.h"
#include "ble_main.h"
#include "bsp_time.h"
#include "learning_platform.h"
#include "app_status.h"
#include "learning_commands.h"
#include "storage_ids.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include <string.h>

#define MODEL_FILE CL_MODEL_FILE
#define MODEL_KEY CL_MODEL_KEY
#define SWITCH_HOLD_MS 10000u
#define FIELD_SETTLE_MS 350u

static sel_model_t model;
static sel_clock_t clock_state;
static uint8_t model_bytes[SEL_MODEL_BYTES] __attribute__((aligned(4)));
static struct { uint32_t key, seen; int8_t rssi; } beacons[SEL_BEACONS];
static volatile bool hf_field, lf_field, session_pending;
static volatile uint8_t reader_count, pending_slot = SEL_NONE;
static volatile uint16_t reader_tokens[SEL_READER_TOKENS];
static volatile uint32_t reader_last_tick, session_end, last_field_change, dropped;
static sel_context_t last_session;
static uint32_t last_session_ms, switched_since, last_prediction, last_save;
static uint32_t switch_count;
static bool have_session, switch_hold, dirty, save_error;
static volatile bool manual_latched, manual_session, manual_completed;
static volatile uint8_t manual_target = SEL_NONE;
static sel_prediction_t prediction;

static uint16_t read16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] << 8 | p[1]); }
static uint32_t read32(const uint8_t *p) { return (uint32_t)read16(p) << 16 | read16(p + 2); }
static void write16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void write32(uint8_t *p, uint32_t v) { write16(p, (uint16_t)(v >> 16)); write16(p + 2, (uint16_t)v); }

bool selection_field_active(void) { return hf_field || lf_field; }
bool selection_background_enabled(void) { return model.config.mode != SEL_OFF; }

void selection_field_event(uint8_t field, bool present) {
    uint32_t now = bsp_monotonic_ms();
    if (present && !selection_field_active() && manual_latched && pending_slot == SEL_NONE &&
        tag_emulation_get_slot() == manual_target) manual_session = true;
    if (field == 2) {
        hf_field = present;
        if (present) {
            if (session_pending) { dropped++; session_pending = false; }
            reader_count = 0; reader_last_tick = app_timer_cnt_get();
        } else { session_end = now; session_pending = true; }
    } else if (field == 1) lf_field = present;
    if (!selection_field_active() && manual_session) manual_completed = true;
    last_field_change = now;
    g_is_tag_emulating = selection_field_active();
}

void selection_reader_command(uint8_t kind, uint8_t parameter) {
    if (!hf_field || kind > 31) return;
    uint32_t tick = app_timer_cnt_get();
    uint32_t elapsed = app_timer_cnt_diff_compute(tick, reader_last_tick);
    reader_last_tick = tick;
    uint8_t timing = elapsed < 13 ? 0 : elapsed < 49 ? 1 : elapsed < 164 ? 2 : elapsed < 656 ? 3 : 4;
    uint16_t identity = (uint16_t)((kind << 11) | (parameter << 3));
    /* Repeated polling adds no identifying information and cannot exhaust the prefix. */
    if (reader_count && (reader_tokens[reader_count - 1] & 0xfff8) == identity) return;
    if (reader_count < SEL_READER_TOKENS) reader_tokens[reader_count++] = (uint16_t)(identity | timing);
}

void selection_ble_report(uint32_t key, int8_t rssi) {
    if (!key || rssi > 0 || rssi < -110) return;
    uint32_t now = bsp_monotonic_ms(); unsigned position = SEL_BEACONS;
    for (unsigned i = 0; i < SEL_BEACONS; i++) {
        if (beacons[i].key == key) {
            beacons[i].rssi = (int8_t)((beacons[i].rssi * 3 + rssi) / 4);
            beacons[i].seen = now; return;
        }
        if (!beacons[i].key || (uint32_t)(now - beacons[i].seen) > SEL_CONTEXT_TTL_MS) position = i;
    }
    if (position == SEL_BEACONS) {
        position = 0;
        for (unsigned i = 1; i < SEL_BEACONS; i++) if (beacons[i].rssi < beacons[position].rssi) position = i;
        if (rssi <= beacons[position].rssi) return;
    }
    beacons[position].key = key; beacons[position].rssi = rssi; beacons[position].seen = now;
}

static sel_context_t snapshot(bool include_reader) {
    sel_context_t c; memset(&c, 0, sizeof(c));
    uint32_t now = bsp_monotonic_ms();
    CRITICAL_REGION_ENTER();
    for (unsigned i = 0; i < SEL_BEACONS; i++) {
        if (beacons[i].key && (uint32_t)(now - beacons[i].seen) <= SEL_CONTEXT_TTL_MS) {
            c.beacons[c.beacon_count].key = beacons[i].key;
            c.beacons[c.beacon_count++].rssi = beacons[i].rssi;
        }
    }
    CRITICAL_REGION_EXIT();
    if (include_reader && have_session && (uint32_t)(now - last_session_ms) <= SEL_CONTEXT_TTL_MS) {
        c.reader_count = last_session.reader_count; c.field = last_session.field;
        memcpy(c.reader, last_session.reader, sizeof(c.reader));
    }
    sel_clock_update(&clock_state, now); sel_clock_context(&clock_state, &c);
    return c;
}

static uint8_t eligible(void) {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < SEL_SLOTS; i++) if (learning_slot_available(i)) mask |= (uint8_t)(1u << i);
    return mask;
}

void selection_init(void) {
    sel_model_init(&model);
    uint16_t length = 0;
    if (learning_model_read(MODEL_FILE, MODEL_KEY, sizeof(model_bytes), model_bytes, &length))
        (void)sel_model_decode(&model, model_bytes, length);
    prediction.slot = SEL_NONE;
    prediction.reason = SEL_REASON_OFF;
    last_save = bsp_monotonic_ms();
}

bool selection_save(void) {
    if (!dirty) return true;
    if (selection_field_active() || !sel_model_encode(&model, model_bytes, sizeof(model_bytes))) return false;
    if (!learning_idle_pause()) return false;
    bool result = learning_model_write(MODEL_FILE, MODEL_KEY, sizeof(model_bytes) / 4, model_bytes);
    learning_idle_resume();
    last_save = bsp_monotonic_ms(); /* Back off failed writes as well as successful writes. */
    save_error = !result;
    if (result) { dirty = false; last_save = bsp_monotonic_ms(); }
    return result;
}

bool selection_manual_slot(uint8_t slot) {
    if (slot >= SEL_SLOTS || !learning_slot_available(slot)) return false;
    pending_slot = slot;
    manual_target = slot; manual_latched = true; manual_session = false; manual_completed = false;
    if (model.config.mode != SEL_OFF && get_device_mode() == DEVICE_MODE_TAG) {
        sel_context_t c = snapshot(false);
        if (sel_learn(&model, &c, slot)) dirty = true;
    }
    return true;
}

bool selection_management_slot(uint8_t slot) {
    if (learning_slot_available(slot)) return selection_manual_slot(slot);
    /* GUI selects an empty slot before uploading its card. Do not learn an
     * empty identity, and keep the ordinary field interlock for this path. */
    if (!learning_select_empty_slot(slot)) return false;
    pending_slot = SEL_NONE;
    manual_target = slot; manual_latched = true; manual_session = false; manual_completed = false;
    learning_refresh_slot();
    return true;
}

void selection_process(void) {
    uint32_t now = bsp_monotonic_ms();
    sel_clock_update(&clock_state, now);
    if (session_pending) {
        sel_context_t c = snapshot(false);
        CRITICAL_REGION_ENTER();
        if (session_pending && !hf_field) {
            c.field = 2; c.reader_count = reader_count;
            for (unsigned i = 0; i < c.reader_count; i++) c.reader[i] = reader_tokens[i];
            last_session_ms = session_end; session_pending = false;
        }
        CRITICAL_REGION_EXIT();
        if (c.reader_count) { last_session = c; have_session = true; }
    }
    bool field = selection_field_active();
    ble_environment_process(model.config.mode != SEL_OFF && get_device_mode() == DEVICE_MODE_TAG && !field,
                            model.config.scan_period_ms, model.config.scan_window_ms);
    if (field) { prediction.reason = SEL_REASON_FIELD; return; }
    if ((uint32_t)(now - last_field_change) < FIELD_SETTLE_MS) return;
    if (pending_slot != SEL_NONE) {
        uint8_t slot = pending_slot;
        if (learning_change_slot(slot, get_device_mode() == DEVICE_MODE_TAG)) {
            learning_refresh_slot();
        }
        pending_slot = SEL_NONE;
    }
    if (manual_completed) {
        /* An explicit choice labels the completed interaction, never an automatic prediction. */
        sel_context_t confirmed = snapshot(true);
        if (model.config.mode != SEL_OFF && learning_slot_available(manual_target) &&
            sel_learn(&model, &confirmed, manual_target)) dirty = true;
        manual_completed = false; manual_session = false; manual_latched = false;
    }
    if (dirty && (uint32_t)(now - last_save) >= 30000) (void)selection_save();
    if ((uint32_t)(now - last_prediction) < 1000) return;
    last_prediction = now;
    sel_context_t c = snapshot(true);
    prediction = sel_predict(&model, &c, eligible());
    if (model.config.mode == SEL_OFF) { prediction.reason = SEL_REASON_OFF; return; }
    if (get_device_mode() != DEVICE_MODE_TAG) return;
    if (manual_latched) { prediction.reason = SEL_REASON_MANUAL; return; }
    if (switch_hold && (uint32_t)(now - switched_since) < SWITCH_HOLD_MS) { prediction.reason = SEL_REASON_COOLDOWN; return; }
    switch_hold = false;
    if (prediction.reason != SEL_REASON_READY || model.config.mode != SEL_AUTO) return;
    if (prediction.slot == tag_emulation_get_slot()) { prediction.reason = SEL_REASON_KEEP; return; }
    if (learning_change_slot(prediction.slot, true)) {
        learning_refresh_slot(); switch_count++;
        switched_since = now; switch_hold = true;
    } else prediction.reason = SEL_REASON_INVALID_SLOT;
}

uint16_t selection_command(uint16_t cmd, const uint8_t *data, uint16_t length, uint8_t *out, uint16_t *size) {
    *size = 0;
    if (length && !data) return STATUS_PAR_ERR;
    if (cmd == DATA_CMD_SELECTION_STATUS && !length) {
        memset(out, 0, 30); out[0] = 1; out[1] = model.config.mode; out[2] = tag_emulation_get_slot();
        out[3] = prediction.slot; out[4] = prediction.score; out[5] = prediction.margin;
        out[6] = prediction.reason; out[7] = prediction.evidence; out[8] = sel_sample_count(&model);
        out[9] = clock_state.valid; out[10] = hf_field; out[11] = lf_field;
        out[12] = pending_slot; out[13] = dirty; out[14] = ble_environment_active(); out[15] = save_error;
        write32(out + 16, clock_state.utc); write32(out + 20, switch_count); write32(out + 24, dropped);
        out[28] = eligible(); out[29] = prediction.observations; *size = 30;
    } else if (cmd == DATA_CMD_SELECTION_CONFIG_SET && length == 12) {
        sel_config_t config;
        memset(&config, 0, sizeof(config));
        config.mode = data[0]; config.min_score = data[1]; config.margin = data[2]; config.min_observations = data[3];
        config.ble_weight = data[4]; config.reader_weight = data[5]; config.time_weight = data[6]; config.reserved = data[7];
        config.scan_period_ms = read16(data + 8); config.scan_window_ms = read16(data + 10);
        if (!sel_config_valid(&config)) return STATUS_PAR_ERR;
        model.config = config; dirty = true;
    } else if (cmd == DATA_CMD_SELECTION_TIME_SYNC && length == 6) {
        if (!sel_clock_sync(&clock_state, read32(data), (int16_t)read16(data + 4), bsp_monotonic_ms())) return STATUS_PAR_ERR;
    } else if (cmd == DATA_CMD_SELECTION_TRAIN && length == 2 && data[0] < SEL_SLOTS && data[1] <= 1) {
        if (!learning_slot_available(data[0])) return STATUS_PAR_ERR;
        sel_context_t c = snapshot(false);
        if (data[1]) {
            if (!have_session || (uint32_t)(bsp_monotonic_ms() - last_session_ms) > SEL_CONTEXT_TTL_MS) return STATUS_NO_CONTEXT;
            c = last_session;
        }
        if (!sel_learn(&model, &c, data[0])) return STATUS_NO_CONTEXT;
        dirty = true;
    } else if (cmd == DATA_CMD_SELECTION_FORGET && length == 1 && (data[0] < SEL_SLOTS || data[0] == SEL_NONE)) {
        if (selection_field_active()) return STATUS_DEVICE_BUSY;
        sel_forget(&model, data[0]); dirty = true;
        if (!selection_save()) return STATUS_STORAGE_ERROR;
    } else if (cmd == DATA_CMD_SELECTION_PREDICT && !length) {
        sel_context_t c = snapshot(true); sel_prediction_t p = sel_predict(&model, &c, eligible());
        out[0] = p.slot; out[1] = p.score; out[2] = p.margin; out[3] = p.reason;
        out[4] = p.evidence; out[5] = p.observations; memcpy(out + 6, p.scores, 8); *size = 14;
    } else if (cmd == DATA_CMD_SELECTION_SAMPLES && !length) {
        for (unsigned i = 0; i < SEL_SAMPLES; i++) {
            const sel_sample_t *s = &model.samples[i]; if (s->slot == SEL_NONE) continue;
            uint8_t *p = out + *size; p[0] = s->slot; p[1] = s->observations; p[2] = s->context.beacon_count;
            p[3] = s->context.reader_count; p[4] = s->context.field; p[5] = s->context.time_valid; p[6] = s->context.weekday;
            write16(p + 7, s->context.minute); write32(p + 9, s->order); *size += 13;
        }
    } else if (cmd == DATA_CMD_SELECTION_SAVE && !length) {
        if (selection_field_active()) return STATUS_DEVICE_BUSY;
        if (!selection_save() || !learning_tag_save()) return STATUS_STORAGE_ERROR;
    } else if (cmd == DATA_CMD_SELECTION_CONFIG_GET && !length) {
        const sel_config_t *c = &model.config;
        out[0] = c->mode; out[1] = c->min_score; out[2] = c->margin; out[3] = c->min_observations;
        out[4] = c->ble_weight; out[5] = c->reader_weight; out[6] = c->time_weight; out[7] = 0;
        write16(out + 8, c->scan_period_ms); write16(out + 10, c->scan_window_ms); *size = 12;
    } else return STATUS_PAR_ERR;
    return STATUS_DEVICE_SUCCESS;
}
