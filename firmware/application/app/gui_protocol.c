/* SPDX-License-Identifier: GPL-3.0-only */
#include "gui_protocol.h"
#include "tag_emulation.h"
#include "nfc_mf1.h"
#include "selection.h"
#include "fds_util.h"
#include "app_status.h"
#include "storage_ids.h"
#include <string.h>

/* Separate private record; no change to existing card or learning layouts. */
#define NICK_FILE CL_NICK_FILE
#define NICK_MAX 30u /* sixteen length-prefixed names fit the 512-byte frame */
static uint8_t names[8][2][32] __attribute__((aligned(4)));

static uint16_t read16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static void write16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
static tag_specific_type_t local_type(uint16_t type) {
    switch (type) {
    case 100: return TAG_TYPE_EM410X;
    case 1000: return TAG_TYPE_MIFARE_Mini;
    case 1001: return TAG_TYPE_MIFARE_1024;
    case 1002: return TAG_TYPE_MIFARE_2048;
    case 1003: return TAG_TYPE_MIFARE_4096;
    default: return TAG_TYPE_UNKNOWN;
    }
}
static uint16_t wire_type(tag_specific_type_t type) {
    switch (type) {
    case TAG_TYPE_EM410X: return 100;
    case TAG_TYPE_MIFARE_Mini: return 1000;
    case TAG_TYPE_MIFARE_1024: return 1001;
    case TAG_TYPE_MIFARE_2048: return 1002;
    case TAG_TYPE_MIFARE_4096: return 1003;
    default: return 0;
    }
}
static bool valid_slot_sense(const uint8_t *p, uint16_t length) {
    return length >= 2 && p[0] < 8 && (p[1] == TAG_SENSE_HF || p[1] == TAG_SENSE_LF);
}
void gui_protocol_init(void) {
    memset(names, 0, sizeof(names));
    for (unsigned slot = 0; slot < 8; slot++) for (unsigned sense = 0; sense < 2; sense++) {
        uint16_t length = 0;
        uint8_t *name = names[slot][sense];
        if (!fds_read_sync_size(NICK_FILE, 1 + slot * 2 + sense, 32, name, &length) ||
            length != 32 || name[0] > NICK_MAX) memset(name, 0, 32);
    }
}
bool gui_protocol_select_slot(uint8_t slot) {
    return selection_management_slot(slot);
}

uint16_t gui_protocol_command(uint16_t cmd, const uint8_t *data, uint16_t length,
                              uint8_t out[512], uint16_t *size) {
    *size = 0;
    if (length && !data) return STATUS_PAR_ERR;
    switch (cmd) {
    case 1018:
        if (length) return STATUS_PAR_ERR;
        out[0] = tag_emulation_get_slot(); *size = 1; break;
    case 1019:
    case 1023:
        if (length) return STATUS_PAR_ERR;
        for (uint8_t i = 0; i < 8; i++) {
            if (cmd == 1019) {
                write16(out + i * 4, wire_type(tag_emulation_slot_type(i, TAG_SENSE_HF)));
                write16(out + i * 4 + 2, wire_type(tag_emulation_slot_type(i, TAG_SENSE_LF)));
            } else {
                out[i * 2] = tag_emulation_sense_enabled(i, TAG_SENSE_HF);
                out[i * 2 + 1] = tag_emulation_sense_enabled(i, TAG_SENSE_LF);
            }
        }
        *size = cmd == 1019 ? 32 : 16; break;
    case 1004:
    case 1005: {
        /* Keep the private CLI's two-byte initialization request working. */
        if (length != 3 && !(cmd == 1005 && length == 2)) return STATUS_PAR_ERR;
        tag_specific_type_t type = length == 3 ? local_type(read16(data + 1)) : (tag_specific_type_t)data[1];
        if (data[0] >= 8 || !wire_type(type)) return STATUS_PAR_ERR;
        if (selection_field_active()) return STATUS_DEVICE_BUSY;
        bool ok = cmd == 1005 ? tag_emulation_factory_data(data[0], type) : tag_emulation_change_type(data[0], type);
        return ok ? STATUS_DEVICE_SUCCESS : STATUS_STORAGE_ERROR;
    }
    case 1006:
        if (length != 3 || !valid_slot_sense(data, length) || data[2] > 1) return STATUS_PAR_ERR;
        return tag_emulation_enable_sense(data[0], (tag_sense_type_t)data[1], data[2]) ? STATUS_DEVICE_SUCCESS : STATUS_DEVICE_BUSY;
    case 1024:
        if (length != 2 || !valid_slot_sense(data, length)) return STATUS_PAR_ERR;
        if (selection_field_active()) return STATUS_DEVICE_BUSY;
        return tag_emulation_delete_sense(data[0], (tag_sense_type_t)data[1]) ? STATUS_DEVICE_SUCCESS : STATUS_STORAGE_ERROR;
    case 1007:
    case 1008:
    case 1021: {
        if (!valid_slot_sense(data, length)) return STATUS_PAR_ERR;
        uint8_t *name = names[data[0]][data[1] == TAG_SENSE_HF ? 0 : 1];
        if (cmd == 1007) {
            if (length < 3 || length > 2 + NICK_MAX) return STATUS_PAR_ERR;
            name[0] = length - 2; memcpy(name + 1, data + 2, name[0]);
        } else {
            if (length != 2) return STATUS_PAR_ERR;
            if (cmd == 1021) memset(name, 0, 32);
            else { *size = name[0]; memcpy(out, name + 1, *size); }
        }
        break;
    }
    case 1038:
        if (length) return STATUS_PAR_ERR;
        for (unsigned i = 0; i < 8; i++) for (unsigned j = 0; j < 2; j++) {
            uint8_t count = names[i][j][0];
            out[(*size)++] = count;
            memcpy(out + *size, names[i][j] + 1, count); *size += count;
        }
        break;
    case 1009:
        if (length) return STATUS_PAR_ERR;
        if (selection_field_active() || !tag_emulation_idle_pause()) return STATUS_DEVICE_BUSY;
        { bool ok = true;
          for (unsigned i = 0; i < 8; i++) for (unsigned j = 0; j < 2; j++) {
              uint8_t *name = names[i][j];
              if (name[0] ? !fds_write_sync(NICK_FILE, 1 + i * 2 + j, 8, name) :
                            fds_delete_sync(NICK_FILE, 1 + i * 2 + j) < 0) ok = false;
          }
          tag_emulation_idle_resume();
          if (!ok || !tag_emulation_save()) return STATUS_STORAGE_ERROR;
        }
        break;
    case 5000:
        if (length != 5) return STATUS_PAR_ERR;
        if (selection_field_active()) return STATUS_DEVICE_BUSY;
        return tag_emulation_set_em410x(data) ? STATUS_DEVICE_SUCCESS : STATUS_STORAGE_ERROR;
    case 5001:
        if (length) return STATUS_PAR_ERR;
        if (!tag_emulation_get_em410x(out)) return STATUS_PAR_ERR;
        *size = 5; break;
    default: {
        nfc_tag_mf1_information_t *info = (nfc_tag_mf1_information_t *)tag_emulation_active_data(TAG_SENSE_HF);
        if (!info) return STATUS_PAR_ERR;
        if (cmd == 4000) {
            if (length < 17 || (length - 1) % 16) return STATUS_PAR_ERR;
            if (selection_field_active()) return STATUS_DEVICE_BUSY;
            return tag_emulation_set_mf1_blocks(data[0], (length - 1) / 16, data + 1) ? STATUS_DEVICE_SUCCESS : STATUS_PAR_ERR;
        }
        if (cmd == 4008) {
            if (length != 2 || !data[1] || data[1] > 32 ||
                (uint16_t)data[0] + data[1] > nfc_tag_mf1_block_count(tag_emulation_slot_type(tag_emulation_get_slot(), TAG_SENSE_HF))) return STATUS_PAR_ERR;
            *size = 16u * data[1]; memcpy(out, info->memory[data[0]], *size); break;
        }
        if (cmd == 4001 || cmd == 4018) {
            if (cmd == 4018) {
                if (length) return STATUS_PAR_ERR;
                uint8_t uid = info->res_coll.size, ats = info->res_coll.ats.length;
                out[0] = uid; memcpy(out + 1, info->res_coll.uid, uid);
                memcpy(out + 1 + uid, info->res_coll.atqa, 2);
                out[uid + 3] = info->res_coll.sak[0]; out[uid + 4] = ats;
                memcpy(out + uid + 5, info->res_coll.ats.data, ats); *size = uid + ats + 5; break;
            }
            if (length < 9 || (data[0] != 4 && data[0] != 7 && data[0] != 10) ||
                length < data[0] + 5u || length != data[0] + 5u + data[data[0] + 4]) return STATUS_PAR_ERR;
            if (!tag_emulation_idle_pause()) return STATUS_DEVICE_BUSY;
            uint8_t uid = data[0], ats = data[uid + 4];
            info->res_coll.size = (nfc_tag_14a_uid_size)uid;
            memcpy(info->res_coll.uid, data + 1, uid); memcpy(info->res_coll.atqa, data + 1 + uid, 2);
            info->res_coll.sak[0] = data[uid + 3]; info->res_coll.ats.length = ats;
            memcpy(info->res_coll.ats.data, data + uid + 5, ats);
            info->config.use_mf1_coll_res = 0;
            tag_emulation_idle_resume(); break;
        }
        if (cmd == 4009) {
            if (length) return STATUS_PAR_ERR;
            out[0] = 0; out[1] = info->config.mode_gen1a_magic; out[2] = 0;
            out[3] = info->config.use_mf1_coll_res; out[4] = info->config.mode_block_write; *size = 5; break;
        }
        if (cmd == 4010 || cmd == 4012 || cmd == 4014 || cmd == 4016) {
            if (length) return STATUS_PAR_ERR;
            out[0] = cmd == 4010 ? info->config.mode_gen1a_magic : cmd == 4014 ? info->config.use_mf1_coll_res :
                     cmd == 4016 ? info->config.mode_block_write : 0; *size = 1; break;
        }
        if (cmd == 4011 || cmd == 4015 || cmd == 4017) {
            if (length != 1 || data[0] > (cmd == 4017 ? 3 : 1)) return STATUS_PAR_ERR;
            if (!tag_emulation_idle_pause()) return STATUS_DEVICE_BUSY;
            if (cmd == 4011) info->config.mode_gen1a_magic = data[0];
            else if (cmd == 4015) info->config.use_mf1_coll_res = data[0];
            else info->config.mode_block_write = (nfc_tag_mf1_write_mode_t)data[0];
            tag_emulation_idle_resume(); break;
        }
        return STATUS_INVALID_CMD;
    }
    }
    return STATUS_DEVICE_SUCCESS;
}
