/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef SELECTION_H
#define SELECTION_H
#include "selection_core.h"

void selection_init(void);
void selection_process(void);
void selection_field_event(uint8_t field, bool present);
bool selection_field_active(void);
void selection_reader_command(uint8_t kind, uint8_t parameter);
void selection_ble_report(uint32_t key, int8_t rssi);
bool selection_manual_slot(uint8_t slot);
bool selection_save(void);
bool selection_background_enabled(void);
/* Returns a protocol status; response length never exceeds 512. */
uint16_t selection_command(uint16_t cmd, const uint8_t *data, uint16_t length,
                           uint8_t *response, uint16_t *response_length);
#endif
