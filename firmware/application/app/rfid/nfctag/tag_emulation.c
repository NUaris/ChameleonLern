#include "crc_utils.h"
#include "nfc_14a.h"
#include "lf_tag_em.h"
#include "nfc_mf1.h"
#include "fds_util.h"
#include "tag_emulation.h"
#include "tag_persistence.h"
#include "selection.h"
#include "rfid_main.h"
#include "app_util_platform.h"
#include "nrf_nfct.h"
#include <string.h>


#define NRF_LOG_MODULE_NAME tag_emu
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();


/*
 * 一个卡槽最多可以同时模拟两种卡，一张ID 125khz em410x，一张IC 13.56mhz 14a。（以后可能可以支持更多）
 * 启动的时候，应当按需启动该启动的场监听器（无数据加载时可不模拟卡，但是需要按需监听场的状态）
 * 如果检索到的卡槽的配置拥有指定的类型的卡片，那么应当进行指定类型的数据的加载，初始化必要的参数
 * 检测到场入场出时，除了需要对相关的LED进行操作外，还需要根据当前数据是否加载来开始模拟卡
 * 在模拟卡，所有的操作应当都是基于RAM中加载的数据进行，在模拟卡结束后，应当将修改的数据进行保存更新到flash
 * 
 *
 *
 * ......
 */

// 标志当前是否在模拟卡中
volatile bool g_is_tag_emulating = false;


// **********************  可持久化参数开始 **********************

/**
 * 标签数据存在于flash中的信息，总长度必须要 4字节（整字）对齐！！！
 */
static uint8_t m_tag_data_buffer_lf[12] ALIGN_U32;
static bool m_lf_loaded, m_hf_loaded;      // 低频卡数据缓冲区
static uint16_t m_tag_data_lf_crc;
static tag_data_buffer_t m_tag_data_lf = { sizeof(m_tag_data_buffer_lf), m_tag_data_buffer_lf, &m_tag_data_lf_crc };

static uint8_t m_tag_data_buffer_hf[4500] ALIGN_U32;    // 高频卡数据缓冲区
static uint16_t m_tag_data_hf_crc;
static tag_data_buffer_t m_tag_data_hf = { sizeof(m_tag_data_buffer_hf), m_tag_data_buffer_hf, &m_tag_data_hf_crc };

/**
 * 八个卡槽，每个卡槽都有其特有的配置
 */
static tag_slot_config_t slotConfig ALIGN_U32 = {
    // 配置激活的卡槽，默认激活第0个卡槽（第一张卡）
    .config = { .activated = 0, .reserved1 = 0, .reserved2 = 0, .reserved3 = 0, },
    // 配置卡槽组
    .group = {
        { .enable = true,  .reserved1 = 0, .reserved2 = 0, .tag_hf = TAG_TYPE_MIFARE_1024, .tag_lf = TAG_TYPE_EM410X, },    // 1
        { .enable = true,  .reserved1 = 0, .reserved2 = 0, .tag_hf = TAG_TYPE_UNKNOWN,     .tag_lf = TAG_TYPE_EM410X, },    // 2
        { .enable = true,  .reserved1 = 0, .reserved2 = 0, .tag_hf = TAG_TYPE_MIFARE_1024, .tag_lf = TAG_TYPE_UNKNOWN, },   // 3
        { .enable = false, .reserved1 = 0, .reserved2 = 0, .tag_hf = TAG_TYPE_UNKNOWN,     .tag_lf = TAG_TYPE_UNKNOWN, },   // 4
        { .enable = false, .reserved1 = 0, .reserved2 = 0, .tag_hf = TAG_TYPE_UNKNOWN,     .tag_lf = TAG_TYPE_UNKNOWN, },   // 5
        { .enable = false, .reserved1 = 0, .reserved2 = 0, .tag_hf = TAG_TYPE_UNKNOWN,     .tag_lf = TAG_TYPE_UNKNOWN, },   // 6
        { .enable = false, .reserved1 = 0, .reserved2 = 0, .tag_hf = TAG_TYPE_UNKNOWN,     .tag_lf = TAG_TYPE_UNKNOWN, },   // 7
        { .enable = false, .reserved1 = 0, .reserved2 = 0, .tag_hf = TAG_TYPE_UNKNOWN,     .tag_lf = TAG_TYPE_UNKNOWN, },   // 8
    },
};
// 卡槽配置特有的CRC，一旦slot配置发生变动，可通过CRC检查出来
static uint16_t m_slot_config_crc;

// **********************  可持久化参数结束 **********************


/**
 * 标签的数据加载到RAM后回调通知的实现操作的映射表，
 * 映射结构为：
 *      场类型         细化的标签类型           加载数据成功后的通知回调      数据保存前的通知回调          初始化数据的实现函数          卡片数据的缓冲区
 */
static tag_base_handler_map_t tag_base_map[] = {
    // 低频ID卡模拟
    { TAG_SENSE_LF,    TAG_TYPE_EM410X,         lf_tag_em410x_data_loadcb,    lf_tag_em410x_data_savecb,    lf_tag_em410x_data_factory,    &m_tag_data_lf },
    // MF1标签模拟
    { TAG_SENSE_HF,    TAG_TYPE_MIFARE_Mini,    nfc_tag_mf1_data_loadcb,      nfc_tag_mf1_data_savecb,      nfc_tag_mf1_data_factory,      &m_tag_data_hf },
    { TAG_SENSE_HF,    TAG_TYPE_MIFARE_1024,    nfc_tag_mf1_data_loadcb,      nfc_tag_mf1_data_savecb,      nfc_tag_mf1_data_factory,      &m_tag_data_hf },
    { TAG_SENSE_HF,    TAG_TYPE_MIFARE_2048,    nfc_tag_mf1_data_loadcb,      nfc_tag_mf1_data_savecb,      nfc_tag_mf1_data_factory,      &m_tag_data_hf },
    { TAG_SENSE_HF,    TAG_TYPE_MIFARE_4096,    nfc_tag_mf1_data_loadcb,      nfc_tag_mf1_data_savecb,      nfc_tag_mf1_data_factory,      &m_tag_data_hf },
    // NTAG标签模拟
    { TAG_SENSE_HF,    TAG_TYPE_NTAG_213,       NULL,                         NULL,                         NULL,                          &m_tag_data_hf },
    { TAG_SENSE_HF,    TAG_TYPE_NTAG_215,       NULL,                         NULL,                         NULL,                          &m_tag_data_hf },
    { TAG_SENSE_HF,    TAG_TYPE_NTAG_216,       NULL,                         NULL,                         NULL,                          &m_tag_data_hf },
};


/**
 * 根据指定的细化标签类型，获得其处理加载的数据的实现函数
 */
static tag_datas_loadcb_t get_data_loadcb_from_tag_type(tag_specific_type_t type) {
    for (int i = 0; i < ARRAY_SIZE(tag_base_map); i++) {
        if (tag_base_map[i].tag_type == type) {
            return tag_base_map[i].data_on_load;
        }
    }
    return NULL;
}

/**
 * 根据指定的细化标签类型，获得其处数据保存前的操作函数
 */
static tag_datas_savecb_t get_data_savecb_from_tag_type(tag_specific_type_t type) {
    for (int i = 0; i < ARRAY_SIZE(tag_base_map); i++) {
        if (tag_base_map[i].tag_type == type) {
            return tag_base_map[i].data_on_save;
        }
    }
    return NULL;
}

/**
 * 根据指定的细化标签类型，获得其处数据工厂初始化的操作函数
 */
static tag_datas_factory_t get_data_factory_from_tag_type(tag_specific_type_t type) {
    for (int i = 0; i < ARRAY_SIZE(tag_base_map); i++) {
        if (tag_base_map[i].tag_type == type) {
            return tag_base_map[i].data_factory;
        }
    }
    return NULL;
}

/**
 * 根据指定的细化标签类型，获得其基础的场感应类型
 */
tag_sense_type_t get_sense_type_from_tag_type(tag_specific_type_t type) {
    for (int i = 0; i < ARRAY_SIZE(tag_base_map); i++) {
        if (tag_base_map[i].tag_type == type) {
            return tag_base_map[i].sense_type;
        }
    }
    return TAG_SENSE_NO;
}

/**
 * 根据类型获取缓冲区信息
 */
tag_data_buffer_t* get_buffer_by_tag_type(tag_specific_type_t type) {
    for (int i = 0; i < ARRAY_SIZE(tag_base_map); i++) {
        if (tag_base_map[i].tag_type == type) {
            return tag_base_map[i].data_buffer;
        }
    }
    return NULL;
}

/**
 * 根据类型加载数据
 */
static bool load_data_by_tag_type(uint8_t slot, tag_specific_type_t type) {
    tag_sense_type_t sense = get_sense_type_from_tag_type(type);
    if (type == TAG_TYPE_UNKNOWN) return true;
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(type);
    tag_datas_loadcb_t callback = get_data_loadcb_from_tag_type(type);
    if (!buffer || !callback || slot >= TAG_MAX_SLOT_NUM) return false;
    fds_slot_record_map_t map;
    get_fds_map_by_slot_sense_type(slot, sense, &map);
    uint16_t length = 0;
    unsigned expected = sense == TAG_SENSE_LF ? 8u : (unsigned)get_information_size_by_tag_type(type, true);
    if (!fds_read_sync_size(map.id, map.key, buffer->length, buffer->buffer, &length) || length != expected) return false;
    if (sense == TAG_SENSE_HF) {
        nfc_tag_mf1_information_t *info = (nfc_tag_mf1_information_t *)buffer->buffer;
        if ((info->res_coll.size != 4 && info->res_coll.size != 7 && info->res_coll.size != 10) ||
            info->config.mode_block_write > NFC_TAG_MF1_WRITE_SHADOW) return false;
    }
    int used = callback(type, buffer);
    if (used <= 0 || used > buffer->length) return false;
    calc_14a_crc_lut(buffer->buffer, used, (uint8_t *)buffer->crc);
    if (sense == TAG_SENSE_HF) m_hf_loaded = true;
    else m_lf_loaded = true;
    return true;
}

/**
 * 根据类型保存数据
 */
static bool save_data_by_tag_type(uint8_t slot, tag_specific_type_t type) {
    if (type == TAG_TYPE_UNKNOWN) return true;
    tag_sense_type_t sense = get_sense_type_from_tag_type(type);
    if ((sense == TAG_SENSE_HF && !m_hf_loaded) || (sense == TAG_SENSE_LF && !m_lf_loaded)) return true;
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(type);
    tag_datas_savecb_t callback = get_data_savecb_from_tag_type(type);
    if (!buffer || !callback) return false;
    int length = callback(type, buffer);
    if (length <= 0) return true;
    if (length > buffer->length) return false;
    uint16_t crc;
    calc_14a_crc_lut(buffer->buffer, length, (uint8_t *)&crc);
    if (crc == *buffer->crc) return true;
    fds_slot_record_map_t map; get_fds_map_by_slot_sense_type(slot, sense, &map);
    if (!fds_write_sync(map.id, map.key, (uint16_t)((length + 3) / 4), buffer->buffer)) return false;
    *buffer->crc = crc;
    return true;
}

/**
 * 根据类型删除数据
 */


/**
 * 加载模拟卡卡片数据，注意，加载仅仅是数据操作，
 * 启动模拟卡请调用 tag_emulation_sense_run 函数，否则不会感应场事件
 */
void tag_emulation_load_data(void) {
    m_hf_loaded = m_lf_loaded = false;
    nfc_tag_mf1_data_loadcb(TAG_TYPE_UNKNOWN, NULL);
    lf_tag_em410x_data_loadcb(TAG_TYPE_UNKNOWN, NULL);
    memset(m_tag_data_buffer_hf, 0, sizeof(m_tag_data_buffer_hf));
    memset(m_tag_data_buffer_lf, 0, sizeof(m_tag_data_buffer_lf));
    uint8_t slot = tag_emulation_get_slot();
    if (slot >= TAG_MAX_SLOT_NUM || !slotConfig.group[slot].enable) return;
    if (tag_emulation_sense_enabled(slot, TAG_SENSE_HF)) load_data_by_tag_type(slot, slotConfig.group[slot].tag_hf);
    if (tag_emulation_sense_enabled(slot, TAG_SENSE_LF)) load_data_by_tag_type(slot, slotConfig.group[slot].tag_lf);
}

/**
 * 保存模拟卡配置数据，在合适的时机，应当调用此函数进行数据的保存
 */
bool tag_emulation_save_data(void) {
    uint8_t slot = tag_emulation_get_slot();
    if (slot >= TAG_MAX_SLOT_NUM) return false;
    bool hf = save_data_by_tag_type(slot, slotConfig.group[slot].tag_hf);
    bool lf = save_data_by_tag_type(slot, slotConfig.group[slot].tag_lf);
    return hf && lf;
}

/**
 * 删除某个卡槽指定的场类型的数据，如果是当前的激活的卡槽的数据，我们还需要动态关闭此卡片的模拟
 */
bool tag_emulation_delete_sense(uint8_t slot, tag_sense_type_t sense) {
    if (slot >= TAG_MAX_SLOT_NUM || (sense != TAG_SENSE_HF && sense != TAG_SENSE_LF) ||
        !tag_emulation_idle_pause()) return false;
    if (slot == tag_emulation_get_slot() && !tag_emulation_save_data()) {
        tag_emulation_idle_resume(); return false;
    }
    fds_slot_record_map_t map; get_fds_map_by_slot_sense_type(slot, sense, &map);
    if (fds_delete_sync(map.id, map.key) < 0) { tag_emulation_idle_resume(); return false; }
    if (sense == TAG_SENSE_HF) slotConfig.group[slot].tag_hf = TAG_TYPE_UNKNOWN;
    else slotConfig.group[slot].tag_lf = TAG_TYPE_UNKNOWN;
    if (slotConfig.group[slot].reserved2 & 0x80)
        slotConfig.group[slot].reserved2 &= (uint8_t)~(sense == TAG_SENSE_HF ? 2 : 1);
    if ((slotConfig.group[slot].reserved2 & 0x80) && !(slotConfig.group[slot].reserved2 & 3))
        slotConfig.group[slot].enable = false;
    if (slotConfig.group[slot].tag_hf == TAG_TYPE_UNKNOWN && slotConfig.group[slot].tag_lf == TAG_TYPE_UNKNOWN)
        slotConfig.group[slot].enable = false;
    if (slot == tag_emulation_get_slot()) tag_emulation_load_data();
    tag_emulation_idle_resume();
    return true;
}

void tag_emulation_delete_data(uint8_t slot, tag_sense_type_t sense) {
    (void)tag_emulation_delete_sense(slot, sense);
}

/**
 * 将某个卡槽的数据设置为出厂的预置数据
 */
bool tag_emulation_factory_data(uint8_t slot, tag_specific_type_t type) {
    if (slot >= TAG_MAX_SLOT_NUM || selection_field_active()) return false;
    tag_datas_factory_t factory = get_data_factory_from_tag_type(type);
    if (!factory) return false;
    if (!tag_emulation_idle_pause()) return false;
    if (slot == tag_emulation_get_slot() && !tag_emulation_save_data()) {
        tag_emulation_idle_resume(); return false;
    }
    bool success = factory(slot, type);
    if (success) {
        if (get_sense_type_from_tag_type(type) == TAG_SENSE_HF) slotConfig.group[slot].tag_hf = type;
        else slotConfig.group[slot].tag_lf = type;
        slotConfig.group[slot].enable = true;
        if (slotConfig.group[slot].reserved2 & 0x80)
            slotConfig.group[slot].reserved2 |= get_sense_type_from_tag_type(type) == TAG_SENSE_HF ? 2 : 1;
        if (tag_emulation_get_slot() == slot) tag_emulation_load_data();
    }
    if (get_device_mode() == DEVICE_MODE_TAG) tag_emulation_sense_run();
    return success;
}

/**
 * 切换场感应监听状态
 * @param enable: 是否使能场感应
 */
static void tag_emulation_sense_switch_all(bool enable) {
    /* Observe both fields even when a slot has no card for that frequency.
       Empty handlers never emit a UID; LF suppresses modulation when unloaded. */
    nfc_tag_14a_sense_switch(enable);
    lf_tag_125khz_sense_switch(enable);
}

/**
 * 切换场感应监听状态
 * @param type: 场感应类型
 * @param enable: 是否使能该类型的场感应
 */
void tag_emulation_sense_switch(tag_sense_type_t type, bool enable) {
    // 检查参数，不允许切换非正常场
    if (type == TAG_SENSE_NO) APP_ERROR_CHECK(NRF_ERROR_INVALID_PARAM);
    // 切换高频
    if (type == TAG_SENSE_HF) nfc_tag_14a_sense_switch(enable);
    // 切换低频
    if (type == TAG_SENSE_LF) lf_tag_125khz_sense_switch(enable);
}

/**
 * 加载模拟卡配置数据，注意，加载仅仅是卡槽配置
 */
void tag_emulation_load_config(void) {
    tag_slot_config_t loaded ALIGN_U32;
    uint16_t length = 0;
    if (fds_read_sync_size(FDS_CONFIG_RECORD_FILE_ID, FDS_CONFIG_RECORD_FILE_KEY,
                           sizeof(loaded), (uint8_t *)&loaded, &length) &&
        length == sizeof(loaded) && loaded.config.activated < TAG_MAX_SLOT_NUM) {
        bool valid = true;
        for (unsigned i = 0; i < TAG_MAX_SLOT_NUM; i++) {
            tag_specific_type_t hf = loaded.group[i].tag_hf, lf = loaded.group[i].tag_lf;
            if ((hf != TAG_TYPE_UNKNOWN && (hf < TAG_TYPE_MIFARE_Mini || hf > TAG_TYPE_MIFARE_4096)) ||
                (lf != TAG_TYPE_UNKNOWN && lf != TAG_TYPE_EM410X)) valid = false;
        }
        if (valid) slotConfig = loaded;
    }
    calc_14a_crc_lut((uint8_t *)&slotConfig, sizeof(slotConfig), (uint8_t *)&m_slot_config_crc);
}

/**
 * 保存模拟卡配置数据
 */
static bool tag_emulation_save_config(void) {
    uint16_t crc;
    calc_14a_crc_lut((uint8_t *)&slotConfig, sizeof(slotConfig), (uint8_t *)&crc);
    if (crc == m_slot_config_crc) return true;
    if (!fds_write_sync(FDS_CONFIG_RECORD_FILE_ID, FDS_CONFIG_RECORD_FILE_KEY,
                        sizeof(slotConfig) / 4, &slotConfig)) return false;
    m_slot_config_crc = crc;
    return true;
}

/**
 * 启动标签模拟
 */
void tag_emulation_sense_run(void) {
    tag_emulation_sense_switch_all(true);
}

/**
 * 停止标签模拟，注意，此函数会绝对屏蔽NFC相关的事件，包括唤醒MCU
 * 如果需要休眠MCU后依旧能通过NFC唤醒，请勿调用此函数
 */
void tag_emulation_sense_end(void) {
    TAG_FIELD_LED_OFF();
    tag_emulation_sense_switch_all(false);
}

/**
 * 初始化标签模拟
 */
void tag_emulation_init(void) {
    tag_emulation_load_config();    // 加载模拟卡的卡槽的配置
    tag_emulation_load_data();      // 加载模拟卡的数据
}

/**
 * 保存标签的数据（从RAM中写入到flash）
 */
bool tag_emulation_idle_pause(void) {
    bool idle;
    CRITICAL_REGION_ENTER();
    idle = !selection_field_active() && !(nrf_nfct_field_status_get() & NRF_NFCT_FIELD_STATE_PRESENT_MASK);
    if (idle && get_device_mode() == DEVICE_MODE_TAG) tag_emulation_sense_end();
    CRITICAL_REGION_EXIT();
    return idle;
}

void tag_emulation_idle_resume(void) {
    if (get_device_mode() == DEVICE_MODE_TAG) tag_emulation_sense_run();
}

bool tag_emulation_save(void) {
    if (!tag_emulation_idle_pause()) return false;
    bool result = tag_emulation_save_data() && tag_emulation_save_config();
    tag_emulation_idle_resume();
    return result;
}

/**
 * 获取当前激活的卡槽索引
 */
uint8_t tag_emulation_get_slot(void) {
    return slotConfig.config.activated;
}

/**
 * 设置当前激活的卡槽索引
 */
void tag_emulation_set_slot(uint8_t index) {
    if (index >= TAG_MAX_SLOT_NUM) return;
    slotConfig.config.activated = index;    // 重设到新切换的卡槽上
}

/**
 * 切换到指定索引的卡槽上，此函数将自动完成数据加载
 */
bool tag_emulation_change_slot(uint8_t index, bool sense_disable) {
    if (index >= TAG_MAX_SLOT_NUM || !slotConfig.group[index].enable || selection_field_active()) return false;
    uint8_t previous = tag_emulation_get_slot();
    if (index == previous) return true;
    if (sense_disable && !tag_emulation_idle_pause()) return false;
    if (!tag_emulation_save_data()) {
        if (sense_disable) tag_emulation_sense_run();
        return false;
    }
    tag_emulation_set_slot(index);
    tag_emulation_load_data();
    bool success = m_hf_loaded || m_lf_loaded;
    if (!success) { tag_emulation_set_slot(previous); tag_emulation_load_data(); }
    if (sense_disable) tag_emulation_sense_run();
    return success;
}

/**
 * 判断指定卡槽是否启用了
 */
bool get_tag_emulation_slot_enable(uint8_t slot) {
    // 直接返回对应卡槽的使能状态
    return slot < TAG_MAX_SLOT_NUM && slotConfig.group[slot].enable;
}

/**
 * 设置指定卡槽是否启用
 */
void set_tag_emulation_slot_enable(uint8_t slot, bool enable) {
    // 直接设置对应卡槽的使能状态
    if (slot < TAG_MAX_SLOT_NUM) slotConfig.group[slot].enable = enable;
}

/**
 * 寻找下一个有效使能的卡槽
 */
uint8_t find_next_tag_emulation_slot(uint8_t current) {
    if (current >= TAG_MAX_SLOT_NUM) return 0;
    uint8_t slot = current;
    for (unsigned i = 0; i < TAG_MAX_SLOT_NUM; i++) {
        slot = (uint8_t)((slot + 1) % TAG_MAX_SLOT_NUM);
        if (tag_emulation_slot_available(slot)) return slot;
    }
    return current;
}

/**
 * 寻找上一个有效使能的卡槽
 */
uint8_t find_prev_tag_emulation_slot(uint8_t current) {
    if (current >= TAG_MAX_SLOT_NUM) return 0;
    uint8_t slot = current;
    for (unsigned i = 0; i < TAG_MAX_SLOT_NUM; i++) {
        slot = (uint8_t)((slot + 7) % TAG_MAX_SLOT_NUM);
        if (tag_emulation_slot_available(slot)) return slot;
    }
    return current;
}

/**
 * 将指定的卡槽的卡槽指定的场类型的卡设置为指定的类型
 */
bool tag_emulation_change_type(uint8_t slot, tag_specific_type_t type) {
    if (slot >= TAG_MAX_SLOT_NUM || selection_field_active() || !get_data_loadcb_from_tag_type(type)) return false;
    bool active = slot == tag_emulation_get_slot();
    if (active && !tag_emulation_idle_pause()) return false;
    if (active && !tag_emulation_save_data()) { tag_emulation_idle_resume(); return false; }
    tag_sense_type_t sense = get_sense_type_from_tag_type(type);
    if (sense == TAG_SENSE_HF) slotConfig.group[slot].tag_hf = type;
    else if (sense == TAG_SENSE_LF) slotConfig.group[slot].tag_lf = type;
    else return false;
    if (active) {
        tag_emulation_load_data();
        if (get_device_mode() == DEVICE_MODE_TAG) tag_emulation_sense_run();
    }
    return true;
}


tag_specific_type_t tag_emulation_slot_type(uint8_t slot, tag_sense_type_t sense) {
    if (slot >= TAG_MAX_SLOT_NUM || (sense != TAG_SENSE_HF && sense != TAG_SENSE_LF)) return TAG_TYPE_UNKNOWN;
    return sense == TAG_SENSE_HF ? slotConfig.group[slot].tag_hf : slotConfig.group[slot].tag_lf;
}

bool tag_emulation_slot_available(uint8_t slot) {
    if (!get_tag_emulation_slot_enable(slot)) return false;
    for (unsigned i = TAG_SENSE_LF; i <= TAG_SENSE_HF; i++) {
        tag_sense_type_t sense = (tag_sense_type_t)i;
        tag_specific_type_t type = tag_emulation_slot_type(slot, sense);
        if (!tag_emulation_sense_enabled(slot, sense) || type == TAG_TYPE_UNKNOWN || !get_data_loadcb_from_tag_type(type)) continue;
        fds_slot_record_map_t map; get_fds_map_by_slot_sense_type(slot, sense, &map);
        if (fds_exists(map.id, map.key)) return true;
    }
    return false;
}

bool tag_emulation_set_em410x(const uint8_t id[5]) {
    if (!id || selection_field_active()) return false;
    if (!tag_emulation_idle_pause()) return false;
    uint8_t slot = tag_emulation_get_slot();
    slotConfig.group[slot].tag_lf = TAG_TYPE_EM410X; slotConfig.group[slot].enable = true;
    if (slotConfig.group[slot].reserved2 & 0x80) slotConfig.group[slot].reserved2 |= 1;
    memset(m_tag_data_buffer_lf, 0, sizeof(m_tag_data_buffer_lf));
    memcpy(m_tag_data_buffer_lf, id, 5);
    lf_tag_em410x_data_loadcb(TAG_TYPE_EM410X, &m_tag_data_lf); m_lf_loaded = true;
    /* Force a first save even if CRC happens to equal the previous buffer's CRC. */
    uint16_t crc; calc_14a_crc_lut(m_tag_data_buffer_lf, 5, (uint8_t *)&crc); m_tag_data_lf_crc = (uint16_t)~crc;
    bool success = tag_emulation_save();
    if (get_device_mode() == DEVICE_MODE_TAG) tag_emulation_sense_run();
    return success;
}

bool tag_emulation_set_mf1_blocks(uint8_t first, uint8_t count, const uint8_t *data) {
    if (!data || !count || !m_hf_loaded || selection_field_active()) return false;
    uint8_t slot = tag_emulation_get_slot();
    tag_specific_type_t type = slotConfig.group[slot].tag_hf;
    if ((uint16_t)first + count > nfc_tag_mf1_block_count(type)) return false;
    if (!tag_emulation_idle_pause()) return false;
    nfc_tag_mf1_information_t *info = (nfc_tag_mf1_information_t *)m_tag_data_buffer_hf;
    memcpy(info->memory[first], data, count * 16u);
    if (get_device_mode() == DEVICE_MODE_TAG) tag_emulation_sense_run();
    return true;
}

uint8_t *tag_emulation_active_data(tag_sense_type_t sense) {
    if (sense == TAG_SENSE_HF && m_hf_loaded) return m_tag_data_buffer_hf;
    if (sense == TAG_SENSE_LF && m_lf_loaded) return m_tag_data_buffer_lf;
    return NULL;
}

bool tag_emulation_get_em410x(uint8_t id[5]) {
    if (!id || !m_lf_loaded) return false;
    memcpy(id, m_tag_data_buffer_lf, 5); return true;
}

/* reserved2 was zero in alpha.2. A high-bit marker preserves that layout and
 * gives legacy records their original shared-enable behavior. */
bool tag_emulation_sense_enabled(uint8_t slot, tag_sense_type_t sense) {
    if (slot >= TAG_MAX_SLOT_NUM || (sense != TAG_SENSE_HF && sense != TAG_SENSE_LF) || !slotConfig.group[slot].enable) return false;
    uint8_t mask = slotConfig.group[slot].reserved2;
    return !(mask & 0x80) || (mask & (sense == TAG_SENSE_HF ? 2 : 1));
}

bool tag_emulation_enable_sense(uint8_t slot, tag_sense_type_t sense, bool enable) {
    if (slot >= TAG_MAX_SLOT_NUM || (sense != TAG_SENSE_HF && sense != TAG_SENSE_LF) || !tag_emulation_idle_pause()) return false;
    if (slot == tag_emulation_get_slot() && !tag_emulation_save_data()) { tag_emulation_idle_resume(); return false; }
    uint8_t mask = slotConfig.group[slot].reserved2;
    if (!(mask & 0x80)) mask = slotConfig.group[slot].enable ? 0x83 : 0x80;
    uint8_t bit = sense == TAG_SENSE_HF ? 2 : 1;
    mask = enable ? mask | bit : mask & (uint8_t)~bit;
    slotConfig.group[slot].reserved2 = mask;
    slotConfig.group[slot].enable = (mask & 3) != 0;
    if (slot == tag_emulation_get_slot()) tag_emulation_load_data();
    tag_emulation_idle_resume(); return true;
}

bool tag_emulation_select_empty_slot(uint8_t slot) {
    if (slot >= TAG_MAX_SLOT_NUM || !get_tag_emulation_slot_enable(slot) || !tag_emulation_idle_pause()) return false;
    if (!tag_emulation_save_data()) { tag_emulation_idle_resume(); return false; }
    tag_emulation_set_slot(slot); tag_emulation_load_data();
    tag_emulation_idle_resume(); return true;
}
