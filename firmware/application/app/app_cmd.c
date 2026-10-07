#include "fds_util.h"
#include "bsp_time.h"
#include "bsp_delay.h"
#include "usb_main.h"
#include "rfid_main.h"
#include "ble_main.h"
#include "syssleep.h"
#include "tag_emulation.h"
#include "hex_utils.h"
#include "data_cmd.h"
#include "app_cmd.h"
#include "selection.h"
#include "dfu_entry.h"
#include "gui_protocol.h"
#include <string.h>


#define NRF_LOG_MODULE_NAME app_cmd
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();


data_frame_tx_t* cmd_processor_get_version(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    const uint8_t version[] = {CHAMELEON_PROTOCOL_MAJOR, CHAMELEON_PROTOCOL_MINOR};
    if (length) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    return data_frame_make(cmd, STATUS_DEVICE_SUCCESS, sizeof(version), (uint8_t *)version);
}

data_frame_tx_t* cmd_processor_change_device_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length == 1 && data[0] <= 1) {
        if (selection_field_active()) return data_frame_make(cmd, STATUS_DEVICE_BUSY, 0, NULL);
        if (data[0] == 1) {
            if (get_device_mode() == DEVICE_MODE_TAG && !tag_emulation_idle_pause())
                return data_frame_make(cmd, STATUS_DEVICE_BUSY, 0, NULL);
            reader_mode_enter();
        } else {
            tag_mode_enter();
        }
    } else {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_DEVICE_SUCCESS, 0, NULL);
}

data_frame_tx_t* cmd_processor_get_device_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    device_mode_t mode = get_device_mode();
    if (mode == DEVICE_MODE_READER) {
        status = 1;
    } else {
        status = 0;
    }
    uint8_t mode_byte = (uint8_t)status;
    return data_frame_make(cmd, STATUS_DEVICE_SUCCESS, 1, &mode_byte);
}

data_frame_tx_t* cmd_processor_14a_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    picc_14a_tag_t taginfo;
    uint8_t response[15];
    if (length) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    status = pcd_14a_reader_scan_auto(&taginfo);
    if (status != HF_TAG_OK) return data_frame_make(cmd, status, 0, NULL);
    if (taginfo.uid_len != 4 && taginfo.uid_len != 7 && taginfo.uid_len != 10)
        return data_frame_make(cmd, HF_ERRSTAT, 0, NULL);
    response[0] = taginfo.uid_len;
    memcpy(response + 1, taginfo.uid, taginfo.uid_len);
    memcpy(response + 1 + taginfo.uid_len, taginfo.atqa, 2);
    response[taginfo.uid_len + 3] = taginfo.sak;
    response[taginfo.uid_len + 4] = 0; /* No ATS captured by this reader path. */
    return data_frame_make(cmd, status, taginfo.uid_len + 5, response);
}

data_frame_tx_t* cmd_processor_detect_mf1_support(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    status = Check_STDMifareNT_Support();
    return data_frame_make(cmd, status, 0, NULL);
}

data_frame_tx_t* cmd_processor_detect_mf1_nt_level(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    status = Check_WeakNested_Support();
    uint8_t level = status == NESTED_TAG_IS_STATIC ? 0 : status == HF_TAG_OK ? 1 : 2;
    if (status != HF_TAG_OK && status != NESTED_TAG_IS_STATIC && status != NESTED_TAG_IS_HARD)
        return data_frame_make(cmd, status, 0, NULL);
    return data_frame_make(cmd, HF_TAG_OK, 1, &level);
}

data_frame_tx_t* cmd_processor_detect_mf1_darkside(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    status = Check_Darkside_Support();
    return data_frame_make(cmd, status, 0, NULL);
}

data_frame_tx_t* cmd_processor_mf1_darkside_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    DarksideCore dc;
	if (length == 4) {
		status = Darkside_Recover_Key(data[1], data[0], data[2], data[3], &dc);
		if (status == HF_TAG_OK) {
			length = sizeof(DarksideCore);
			data = (uint8_t *)(&dc);
		} else {
			length = 0;
		}
	} else {
		status = STATUS_PAR_ERR;
		length = 0;
	}
    return data_frame_make(cmd, status, length, data);
}

data_frame_tx_t* cmd_processor_detect_nested_dist(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    NestedDist nd;
	if (length == 8) {
		status = Nested_Distacne_Detect(data[1], data[0], &data[2], &nd);
		if (status == HF_TAG_OK) {
			// 探测完成
			length = sizeof(NestedDist);
			data = (uint8_t *)(&nd);
		} else {
			length = 0;
		}
	} else {
		status = STATUS_PAR_ERR;
		length = 0;
	}
    return data_frame_make(cmd, status, 0, NULL);
}

data_frame_tx_t* cmd_processor_mf1_nt_distance(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    NestedDist nd;
	if (length == 8) {
		status = Nested_Distacne_Detect(data[1], data[0], &data[2], &nd);
		if (status == HF_TAG_OK) {
			// 探测完成
			length = sizeof(NestedDist);
			data = (uint8_t *)(&nd);
		} else {
			length = 0;
		}
	} else {
		status = STATUS_PAR_ERR;
		length = 0;
	}
    return data_frame_make(cmd, status, length, data);
}

data_frame_tx_t* cmd_processor_mf1_nested_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    NestedCore ncs[SETS_NR];
	if (length == 10) {
		status = Nested_Recover_Key(bytes_to_num(&data[2], 6), data[1], data[0], data[9], data[8], ncs);
		if (status == HF_TAG_OK) {
			length = sizeof(ncs);
			data = (uint8_t *)(&ncs);
		} else {
			length = 0;
		}
	} else {
		status = STATUS_PAR_ERR;
		length = 0;
	}
    return data_frame_make(cmd, status, length, data);
}

data_frame_tx_t* cmd_processor_mf1_auth_one_key_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length == 8) {
		status = auth_key_use_522_hw(data[1], data[0], &data[2]);
        pcd_14a_reader_mf1_unauth();
	} else {
		status = STATUS_PAR_ERR;
	}
    return data_frame_make(cmd, status, 0, NULL);
}

data_frame_tx_t* cmd_processor_mf1_read_one_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t block[16] = { 0x00 };
	if (length == 8) {
		status = auth_key_use_522_hw(data[1], data[0], &data[2]);
		if (status == HF_TAG_OK) {
			// 直接调用标准读取API去读取卡片
			status = pcd_14a_reader_mf1_read(data[1], block);
			if (status == HF_TAG_OK) {
				length = 16;
			} else {
				length = 0;
			}
		} else {
			length = 0;
		}
	} else {
		length = 0;
		status = STATUS_PAR_ERR;
	}
    return data_frame_make(cmd, status, length, block);
}

data_frame_tx_t* cmd_processor_mf1_write_one_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length == 24) {
		status = auth_key_use_522_hw(data[1], data[0], &data[2]);
		if (status == HF_TAG_OK) {
			// 直接调用标准写入API去写入卡片
			status = pcd_14a_reader_mf1_write(data[1], &data[8]);
		} else {
			length = 0;
		}
	} else {
		status = STATUS_PAR_ERR;
	}
    return data_frame_make(cmd, status, 0, NULL);
}

data_frame_tx_t* cmd_processor_em410x_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t id_buffer[5] = { 0x00 };
	status = PcdScanEM410X(id_buffer);
    return data_frame_make(cmd, status, sizeof(id_buffer), id_buffer);
}

data_frame_tx_t* cmd_processor_write_em410x_2_t57(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    // 写入T55XX的标签需要提供一个5个Byte长度的卡号
	// 并且还需要提供至少一个新的密钥，与一个旧的密钥
	if (length >= 13 && (length - 9) % 4 == 0) {
		status = PcdWriteT55XX(
			data, 				// 传入UID
			data + 5, 			// 传入newkey
			data + 9,			// 传入oldkey
			(length - 9) / 4 	// 传入减去newkey + uid后的剩余的oldkey的密钥组数
		);
	} else {
		status = STATUS_PAR_ERR;
	}
    return data_frame_make(cmd, status, 0, NULL);
}

data_frame_tx_t* cmd_processor_set_slot_activated(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    status = length == 1 && gui_protocol_select_slot(data[0]) ? STATUS_DEVICE_SUCCESS : STATUS_PAR_ERR;
    return data_frame_make(cmd, status, 0, NULL);
}

data_frame_tx_t* cmd_processor_set_slot_tag_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    // 需要确保传过来的标签类型是有效的
    if (length == 1 && data[0] != TAG_TYPE_UNKNOWN) {
        // 取出上位机传过来的标签类型
        tag_specific_type_t tag_type = data[0];
        // 获得当前使能的卡槽
        uint8_t slot_index_now = tag_emulation_get_slot();
        // 将当前的卡槽切换到指定的模拟卡类型
        status = tag_emulation_change_type(slot_index_now, tag_type) ? STATUS_DEVICE_SUCCESS : STATUS_DEVICE_BUSY;
	} else {
        status = STATUS_PAR_ERR;
    }
    return data_frame_make(cmd, status, 0, NULL);
}

data_frame_tx_t* cmd_processor_set_slot_data_default(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    // 需要确保传过来的标签类型是有效的
    if (length == 2 && data[0] < TAG_MAX_SLOT_NUM && data[1] != TAG_TYPE_UNKNOWN) {
        uint8_t target_init_slot_num = data[0];    // 获得要操作的卡槽
        tag_specific_type_t tag_type = data[1];    // 取出上位机传过来的标签类型
        // 重置当前的卡槽为缺省数据，如果失败，则可能是并未实现此API的缺省
        status = selection_field_active() ? STATUS_DEVICE_BUSY :
            tag_emulation_factory_data(target_init_slot_num, tag_type) ? STATUS_DEVICE_SUCCESS : STATUS_STORAGE_ERROR;
	} else {
        status = STATUS_PAR_ERR;
    }
    return data_frame_make(cmd, status, 0, NULL);
}

/**
 * before reader run, reset reader and on antenna,
 * we must to wait some time, to init picc(power).
 */
data_frame_tx_t* before_reader_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    device_mode_t mode = get_device_mode();
    if (mode == DEVICE_MODE_READER) {
        pcd_14a_reader_reset();
        pcd_14a_reader_antenna_on();
        bsp_delay_ms(8);
        return NULL;
    } else {
        return data_frame_make(cmd, STATUS_DEVIEC_MODE_ERROR, 0, NULL);
    }
}

/**
 * after reader run, off antenna, to keep battery.
 */
data_frame_tx_t* after_reader_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    pcd_14a_reader_antenna_off();
    return NULL;
}

/**
 * (cmd -> process) function map, the map struct is:
 *            cmd code                        before process               cmd processor                                after process
 */

static data_frame_tx_t *cmd_learning(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    static uint8_t output[DATA_FRAME_MAX_DATA];
    uint16_t size;
    status = selection_command(cmd, data, length, output, &size);
    return data_frame_make(cmd, status, size, output);
}

static data_frame_tx_t *cmd_card_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (selection_field_active()) return data_frame_make(cmd, STATUS_DEVICE_BUSY, 0, NULL);
    if (get_device_mode() != DEVICE_MODE_READER) return data_frame_make(cmd, STATUS_DEVIEC_MODE_ERROR, 0, NULL);
    status = STATUS_PAR_ERR;
    if (cmd == DATA_CMD_SET_EM410X_DATA && length == 5)
        status = tag_emulation_set_em410x(data) ? STATUS_DEVICE_SUCCESS : STATUS_STORAGE_ERROR;
    else if (cmd == DATA_CMD_SET_MF1_DATA && length >= 18 && data[1] && length == 2u + 16u * data[1])
        status = tag_emulation_set_mf1_blocks(data[0], data[1], data + 2) ? STATUS_DEVICE_SUCCESS : STATUS_PAR_ERR;
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_slots(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t slots[25]; slots[0] = tag_emulation_get_slot();
    if (length) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    for (uint8_t i = 0; i < TAG_MAX_SLOT_NUM; i++) {
        slots[1 + i*3] = get_tag_emulation_slot_enable(i);
        slots[2 + i*3] = tag_emulation_slot_type(i, TAG_SENSE_HF);
        slots[3 + i*3] = tag_emulation_slot_type(i, TAG_SENSE_LF);
    }
    return data_frame_make(cmd, STATUS_DEVICE_SUCCESS, sizeof(slots), slots);
}

static data_frame_tx_t *before_lf_reader(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return get_device_mode() == DEVICE_MODE_READER ? NULL : data_frame_make(cmd, STATUS_DEVIEC_MODE_ERROR, 0, NULL);
}

static data_frame_tx_t *cmd_enter_dfu(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    return data_frame_make(cmd, dfu_entry_request(), 0, NULL);
}

static data_frame_tx_t *cmd_device_identity(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    if (cmd == DATA_CMD_GET_DEVICE_MODEL) {
        uint8_t model = CHAMELEON_DEVICE_MODEL;
        return data_frame_make(cmd, STATUS_DEVICE_SUCCESS, 1, &model);
    }
    static uint8_t version[] = CHAMELEON_PROJECT_VERSION;
    return data_frame_make(cmd, STATUS_DEVICE_SUCCESS, sizeof(version) - 1, version);
}

static data_frame_tx_t *cmd_capabilities(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);

static data_frame_tx_t *cmd_gui_management(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    static uint8_t output[DATA_FRAME_MAX_DATA];
    uint16_t size = 0;
    status = gui_protocol_command(cmd, data, length, output, &size);
    return data_frame_make(cmd, status, size, output);
}

static cmd_data_map_t m_data_cmd_map[] = {
#define GUI_ENTRY(id) {id, NULL, cmd_gui_management, NULL},
    GUI_COMMANDS(GUI_ENTRY)
#undef GUI_ENTRY
    {DATA_CMD_ENTER_BOOTLOADER, NULL, cmd_enter_dfu, NULL},
    {DATA_CMD_GET_DEVICE_MODEL, NULL, cmd_device_identity, NULL},
    {DATA_CMD_GET_GIT_VERSION, NULL, cmd_device_identity, NULL},
    {DATA_CMD_GET_DEVICE_CAPABILITIES, NULL, cmd_capabilities, NULL},
    {DATA_CMD_SELECTION_STATUS, NULL, cmd_learning, NULL},
    {DATA_CMD_SELECTION_CONFIG_SET, NULL, cmd_learning, NULL},
    {DATA_CMD_SELECTION_TIME_SYNC, NULL, cmd_learning, NULL},
    {DATA_CMD_SELECTION_TRAIN, NULL, cmd_learning, NULL},
    {DATA_CMD_SELECTION_FORGET, NULL, cmd_learning, NULL},
    {DATA_CMD_SELECTION_PREDICT, NULL, cmd_learning, NULL},
    {DATA_CMD_SELECTION_SAMPLES, NULL, cmd_learning, NULL},
    {DATA_CMD_SELECTION_SAVE, NULL, cmd_learning, NULL},
    {DATA_CMD_SELECTION_CONFIG_GET, NULL, cmd_learning, NULL},
    {DATA_CMD_SET_EM410X_DATA, NULL, cmd_card_data, NULL},
    {DATA_CMD_SET_MF1_DATA, NULL, cmd_card_data, NULL},
    {DATA_CMD_GET_SLOT_INFO, NULL, cmd_slots, NULL},
    {    DATA_CMD_GET_APP_VERSION,            NULL,                        cmd_processor_get_version,                   NULL                },
    {    DATA_CMD_CHANGE_DEVICE_MODE,         NULL,                        cmd_processor_change_device_mode,            NULL                },
    {    DATA_CMD_GET_DEVICE_MODE,            NULL,                        cmd_processor_get_device_mode,               NULL                },

    {    DATA_CMD_SCAN_14A_TAG,               before_reader_run,           cmd_processor_14a_scan,                      after_reader_run    },
    {    DATA_CMD_MF1_SUPPORT_DETECT,         before_reader_run,           cmd_processor_detect_mf1_support,            after_reader_run    },
    {    DATA_CMD_MF1_NT_LEVEL_DETECT,        before_reader_run,           cmd_processor_detect_mf1_nt_level,           after_reader_run    },

    {    DATA_CMD_MF1_DARKSIDE_ACQUIRE,       before_reader_run,           cmd_processor_mf1_darkside_acquire,          after_reader_run    },
    {    DATA_CMD_MF1_NT_DIST_DETECT,         before_reader_run,           cmd_processor_mf1_nt_distance,               after_reader_run    },
    {    DATA_CMD_MF1_NESTED_ACQUIRE,         before_reader_run,           cmd_processor_mf1_nested_acquire,            after_reader_run    },

    {    DATA_CMD_MF1_CHECK_ONE_KEY_BLOCK,    before_reader_run,           cmd_processor_mf1_auth_one_key_block,        after_reader_run    },
    {    DATA_CMD_MF1_READ_ONE_BLOCK,         before_reader_run,           cmd_processor_mf1_read_one_block,            after_reader_run    },
    {    DATA_CMD_MF1_WRITE_ONE_BLOCK,        before_reader_run,           cmd_processor_mf1_write_one_block,           after_reader_run    },

    {    DATA_CMD_SCAN_EM410X_TAG,            before_lf_reader,                        cmd_processor_em410x_scan,                   NULL                },
    {    DATA_CMD_WRITE_EM410X_TO_T5577,      before_lf_reader,                        cmd_processor_write_em410x_2_t57,            NULL                },
    
    {    DATA_CMD_SET_SLOT_ACTIVATED,         NULL,                        cmd_processor_set_slot_activated,            NULL                },
    
};


/**@brief Function for prcoess data frame(cmd)
 */
static data_frame_tx_t *cmd_capabilities(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t capabilities[2 * ARRAY_SIZE(m_data_cmd_map)];
    if (length) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    for (unsigned i = 0; i < ARRAY_SIZE(m_data_cmd_map); i++) {
        capabilities[2*i] = (uint8_t)(m_data_cmd_map[i].cmd >> 8);
        capabilities[2*i+1] = (uint8_t)m_data_cmd_map[i].cmd;
    }
    return data_frame_make(cmd, STATUS_DEVICE_SUCCESS, sizeof(capabilities), capabilities);
}

void on_data_frame_received(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (app_cmd_dfu_pending()) {
        data_frame_tx_t *busy = data_frame_make(cmd, STATUS_DEVICE_BUSY, 0, NULL);
        if (data_frame_source() == DATA_FRAME_BLE) ble_command_write(busy->buffer, busy->length);
        else usb_cdc_write(busy->buffer, busy->length);
        return;
    }
    data_frame_tx_t* response = NULL;
    bool is_cmd_support = false;
    // print info
    NRF_LOG_INFO("Data frame: cmd = %02x, status = %02x, length = %d", cmd, status, length);

    for (int i = 0; i < ARRAY_SIZE(m_data_cmd_map); i++) {
        if (m_data_cmd_map[i].cmd == cmd) {
            is_cmd_support = true;
            if (m_data_cmd_map[i].cmd_before != NULL) {
                data_frame_tx_t* before_resp = m_data_cmd_map[i].cmd_before(cmd, status, length, data);
                if (before_resp != NULL) {
                    // some problem found before run cmd.
                    response = before_resp;
                    break;
                }
            }
            if (m_data_cmd_map[i].cmd_processor != NULL) response = m_data_cmd_map[i].cmd_processor(cmd, status, length, data);
            if (m_data_cmd_map[i].cmd_after != NULL) {
                data_frame_tx_t* after_resp = m_data_cmd_map[i].cmd_after(cmd, status, length, data);
                if (after_resp != NULL) {
                    // some problem found after run cmd.
                    response = after_resp;
                    break;
                }
            }
            break;
        }
    }
    if (is_cmd_support) {
        // check and response
        if (response != NULL) {
            if (data_frame_source() == DATA_FRAME_BLE) ble_command_write(response->buffer, response->length);
            else usb_cdc_write(response->buffer, response->length);
        }
    } else {
        // response cmd unsupport.
        response = data_frame_make(cmd, STATUS_INVALID_CMD, 0, NULL);
        if (data_frame_source() == DATA_FRAME_BLE) ble_command_write(response->buffer, response->length);
            else usb_cdc_write(response->buffer, response->length);
        NRF_LOG_INFO("Data frame cmd invalid: %d,", cmd);
    }
}
