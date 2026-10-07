/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LEARNING_PLATFORM_H
#define LEARNING_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>
bool learning_slot_available(uint8_t slot);
bool learning_hf_field_present(void);
void learning_resume_tag_mode(void);
bool learning_reader_abandoned(void);
bool learning_change_slot(uint8_t slot, bool pause);
bool learning_select_empty_slot(uint8_t slot);
bool learning_tag_save(void);
bool learning_idle_pause(void);
void learning_idle_resume(void);
void learning_refresh_slot(void);
bool learning_model_read(uint16_t id, uint16_t key, uint16_t max, uint8_t *out, uint16_t *actual);
bool learning_model_write(uint16_t id, uint16_t key, uint16_t words, void *data);
#define STATUS_DEVICE_SUCCESS 0x68
#define STATUS_DEVICE_BUSY 0x6a
#define STATUS_STORAGE_ERROR 0x70
#define STATUS_NO_CONTEXT 0x6c
#endif
