/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "dfu_entry.h"
#include "rfid_main.h"
#include "app_status.h"

static uint32_t now, gpregret;
static bool field, can_pause = true, save_tag = true, save_model = true;
static unsigned pauses, resumes, tag_saves, model_saves, resets;
static int pause_budget = -1;
static device_mode_t mode = DEVICE_MODE_TAG;
uint32_t bsp_monotonic_ms(void) { return now; }
bool selection_field_active(void) { return field; }
bool selection_save(void) { model_saves++; return save_model; }
device_mode_t get_device_mode(void) { return mode; }
bool tag_emulation_idle_pause(void) {
    pauses++;
    if (pause_budget == 0) return false;
    if (pause_budget > 0) pause_budget--;
    return can_pause;
}
void tag_emulation_idle_resume(void) { resumes++; }
bool tag_emulation_save(void) { tag_saves++; return save_tag; }
uint32_t sd_power_gpregret_clr(uint32_t index, uint32_t mask) { assert(index == 0); gpregret &= ~mask; return 0; }
uint32_t sd_power_gpregret_set(uint32_t index, uint32_t mask) { assert(index == 0); gpregret |= mask; return 0; }
void NVIC_SystemReset(void) { resets++; }

int main(void) {
    field = true;
    assert(dfu_entry_request() == STATUS_DEVICE_BUSY && pauses == 0 && tag_saves == 0);
    field = false; can_pause = false;
    assert(dfu_entry_request() == STATUS_DEVICE_BUSY && tag_saves == 0);
    can_pause = true; save_tag = false;
    assert(dfu_entry_request() == STATUS_STORAGE_ERROR && resumes == 1 && model_saves == 0);
    save_tag = true; save_model = false;
    assert(dfu_entry_request() == STATUS_STORAGE_ERROR && resumes == 2 && !app_cmd_dfu_pending());
    app_cmd_process(); assert(resets == 0);
    mode = DEVICE_MODE_READER;
    assert(dfu_entry_request() == STATUS_STORAGE_ERROR && resumes == 2 && pauses == 3);
    mode = DEVICE_MODE_TAG; save_model = true; pause_budget = 1;
    assert(dfu_entry_request() == STATUS_DEVICE_BUSY && !app_cmd_dfu_pending());
    pause_budget = -1;
    now = UINT32_MAX - 100u;
    assert(dfu_entry_request() == STATUS_DEVICE_SUCCESS && app_cmd_dfu_pending() && pauses == 7);
    assert(dfu_entry_request() == STATUS_DEVICE_BUSY);
    app_cmd_process(); assert(resets == 0);
    now += 199u; app_cmd_process(); assert(resets == 0);
    gpregret = 0xFFFFu;
    now++; app_cmd_process(); assert(resets == 1 && gpregret == 0xB1);
    puts("DFU entry tests passed (field interlock, save failure, transport delay, timer wrap, official trigger)");
    return 0;
}
