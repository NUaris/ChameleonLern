/* SPDX-License-Identifier: GPL-3.0-only */
#include "dfu_entry.h"
#include "rfid_main.h"
#include "selection.h"
#include "tag_emulation.h"
#include "bsp_time.h"
#include "app_status.h"
#include "app_error.h"
#include "nrf_soc.h"
#include "nrf.h"

static bool dfu_pending;
static uint32_t requested_at;

bool app_cmd_dfu_pending(void) { return dfu_pending; }

uint16_t dfu_entry_request(void) {
    if (dfu_pending || selection_field_active()) return STATUS_DEVICE_BUSY;
    if (get_device_mode() == DEVICE_MODE_TAG && !tag_emulation_idle_pause())
        return STATUS_DEVICE_BUSY;
    if (!tag_emulation_save() || !selection_save()) {
        if (get_device_mode() == DEVICE_MODE_TAG) tag_emulation_idle_resume();
        return STATUS_STORAGE_ERROR;
    }
    /* Saving may resume sensing. Recheck the physical field and pause again
     * before the acknowledgment-to-reset interval.
     */
    if (get_device_mode() == DEVICE_MODE_TAG && !tag_emulation_idle_pause())
        return STATUS_DEVICE_BUSY;
    requested_at = bsp_monotonic_ms(); dfu_pending = true;
    return STATUS_DEVICE_SUCCESS;
}

/* Acknowledge first, then service USB/BLE for 200 ms before resetting.
 * GPREGRET 0xB1 is the existing official bootloader's DFU trigger.
 */
void app_cmd_process(void) {
    if (!dfu_pending || (uint32_t)(bsp_monotonic_ms() - requested_at) < 200u) return;
    APP_ERROR_CHECK(sd_power_gpregret_clr(0, 0xffffffff));
    APP_ERROR_CHECK(sd_power_gpregret_set(0, 0xB1));
    NVIC_SystemReset();
}
