/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef SELECTION_CORE_H
#define SELECTION_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SEL_SLOTS 8
#define SEL_BEACONS 6
#define SEL_READER_TOKENS 8
#define SEL_SAMPLES 32
#define SEL_NONE 255
#define SEL_MODEL_BYTES (32 + SEL_SAMPLES * 68)
#define SEL_CONTEXT_TTL_MS 20000u

typedef enum { SEL_OFF, SEL_OBSERVE, SEL_AUTO } sel_mode_t;
typedef enum {
    SEL_REASON_OFF, SEL_REASON_EMPTY, SEL_REASON_NO_CONTEXT,
    SEL_REASON_LOW_SCORE, SEL_REASON_AMBIGUOUS, SEL_REASON_SUPPORT,
    SEL_REASON_MANUAL, SEL_REASON_FIELD, SEL_REASON_COOLDOWN,
    SEL_REASON_READY, SEL_REASON_KEEP, SEL_REASON_INVALID_SLOT
} sel_reason_t;

typedef struct {
    uint32_t key;
    int8_t rssi;
} sel_beacon_t;

typedef struct {
    uint8_t beacon_count, reader_count, field, time_valid;
    uint16_t minute;
    uint8_t weekday;
    sel_beacon_t beacons[SEL_BEACONS];
    uint16_t reader[SEL_READER_TOKENS];
} sel_context_t;

typedef struct {
    uint8_t mode, min_score, margin, min_observations;
    uint8_t ble_weight, reader_weight, time_weight, reserved;
    uint16_t scan_period_ms, scan_window_ms;
} sel_config_t;

typedef struct {
    sel_context_t context;
    uint32_t order;
    uint8_t slot, observations;
} sel_sample_t;

typedef struct {
    sel_config_t config;
    sel_sample_t samples[SEL_SAMPLES];
    uint32_t order;
} sel_model_t;

typedef struct {
    uint8_t slot, score, margin, evidence, observations, reason;
    uint8_t scores[SEL_SLOTS];
} sel_prediction_t;

typedef struct {
    uint32_t utc, last_ms;
    uint16_t remainder_ms;
    int16_t timezone_minutes;
    bool valid;
} sel_clock_t;

void sel_model_init(sel_model_t *model);
bool sel_config_valid(const sel_config_t *config);
bool sel_context_valid(const sel_context_t *context);
bool sel_learn(sel_model_t *model, const sel_context_t *context, uint8_t slot);
void sel_forget(sel_model_t *model, uint8_t slot);
uint8_t sel_sample_count(const sel_model_t *model);
sel_prediction_t sel_predict(const sel_model_t *model, const sel_context_t *context,
                             uint8_t eligible_slots);
bool sel_model_encode(const sel_model_t *model, uint8_t *data, size_t length);
bool sel_model_decode(sel_model_t *model, const uint8_t *data, size_t length);
uint32_t sel_ble_identity(const uint8_t address[6], uint8_t address_type,
                         const uint8_t *advertising, size_t length);
bool sel_clock_sync(sel_clock_t *clock, uint32_t utc, int16_t timezone, uint32_t now_ms);
void sel_clock_update(sel_clock_t *clock, uint32_t now_ms);
void sel_clock_context(const sel_clock_t *clock, sel_context_t *context);

#endif
