/* SPDX-License-Identifier: GPL-3.0-only */
#include "learning_platform.h"
#include "selection.h"
#include "tag_emulation.h"
#include "tag_persistence.h"
#include "fds_util.h"
#include "rfid_main.h"
#include "rgb_marquee.h"

bool learning_slot_available(uint8_t slot) {
    if (slot >= TAG_MAX_SLOT_NUM) return false;
    tag_slot_specific_type_t types;
    tag_emulation_get_specific_types_by_slot(slot, &types);
    for (unsigned sense = TAG_SENSE_LF; sense <= TAG_SENSE_HF; sense++) {
        tag_specific_type_t type = sense == TAG_SENSE_HF ? types.tag_hf : types.tag_lf;
        fds_slot_record_map_t map;
        get_fds_map_by_slot_sense_type_for_dump(slot, sense, &map);
        if (type != TAG_TYPE_UNDEFINED && is_slot_enabled(slot, sense) && fds_is_exists(map.id, map.key)) return true;
    }
    return false;
}
bool learning_change_slot(uint8_t slot, bool pause) {
    if (selection_field_active() || !learning_slot_available(slot)) return false;
    tag_emulation_change_slot(slot, pause);
    return tag_emulation_get_slot() == slot;
}
bool learning_select_empty_slot(uint8_t slot) {
    if (slot >= TAG_MAX_SLOT_NUM || selection_field_active()) return false;
    tag_emulation_change_slot(slot, get_device_mode() == DEVICE_MODE_TAG);
    return tag_emulation_get_slot() == slot;
}
bool learning_idle_pause(void) {
    if (selection_field_active()) return false;
    if (get_device_mode() == DEVICE_MODE_TAG) tag_emulation_sense_end();
    return true;
}
void learning_idle_resume(void) {
    if (get_device_mode() == DEVICE_MODE_TAG) tag_emulation_sense_run();
}
bool learning_tag_save(void) {
    if (!learning_idle_pause()) return false;
    tag_emulation_save();
    learning_idle_resume();
    return true;
}
void learning_refresh_slot(void) {
    /* Keep upstream USB/charging animations intact; refresh only on a switch. */
    apply_slot_change(tag_emulation_get_slot(), tag_emulation_get_slot());
}
bool learning_model_read(uint16_t id, uint16_t key, uint16_t max, uint8_t *out, uint16_t *actual) {
    *actual = max;
    return fds_read_sync(id, key, actual, out);
}
bool learning_model_write(uint16_t id, uint16_t key, uint16_t words, void *data) {
    return fds_write_sync(id, key, words * 4u, data);
}
