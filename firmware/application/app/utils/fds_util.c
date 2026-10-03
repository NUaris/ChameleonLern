#include "fds_util.h"
#include "app_timer.h"
#include "nrf_soc.h"
#include "app_error.h"
#include <string.h>

#define NRF_LOG_MODULE_NAME fds_sync
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();

static struct {
    uint16_t id, key;
    uint8_t operation;
    volatile bool waiting, completed;
    volatile ret_code_t result;
} operation;

static bool find_record(uint16_t id, uint16_t key, fds_record_desc_t *desc) {
    fds_find_token_t token = {0};
    return fds_record_find(id, key, desc, &token) == NRF_SUCCESS;
}

bool fds_exists(uint16_t id, uint16_t key) {
    fds_record_desc_t desc;
    return find_record(id, key, &desc);
}

bool fds_read_sync_size(uint16_t id, uint16_t key, uint16_t max_length,
                        uint8_t *buffer, uint16_t *actual_length) {
    fds_flash_record_t record;
    fds_record_desc_t desc;
    if (actual_length) *actual_length = 0;
    if (!buffer || !find_record(id, key, &desc) || fds_record_open(&desc, &record) != NRF_SUCCESS)
        return false;
    uint32_t length = record.p_header->length_words * 4u;
    bool valid = length <= max_length;
    if (valid) {
        memcpy(buffer, record.p_data, length);
        if (actual_length) *actual_length = (uint16_t)length;
    }
    /* Every successful open must be closed, including undersized destination buffers. */
    ret_code_t closed = fds_record_close(&desc);
    return valid && closed == NRF_SUCCESS;
}

bool fds_read_sync(uint16_t id, uint16_t key, uint16_t max_length, uint8_t *buffer) {
    return fds_read_sync_size(id, key, max_length, buffer, NULL);
}

static bool wait_complete(void) {
    uint32_t start = app_timer_cnt_get();
    while (!operation.completed) {
        if (app_timer_cnt_diff_compute(app_timer_cnt_get(), start) > APP_TIMER_TICKS(10000)) {
            /* A queued flash operation still owns its source buffer: reset rather than reuse it. */
            APP_ERROR_HANDLER(NRF_ERROR_TIMEOUT);
            return false;
        }
        (void)sd_app_evt_wait();
    }
    operation.waiting = false;
    return operation.result == NRF_SUCCESS;
}

static void begin(uint8_t kind, uint16_t id, uint16_t key) {
    operation.id = id; operation.key = key; operation.operation = kind;
    operation.completed = false; operation.result = NRF_ERROR_BUSY; operation.waiting = true;
}

static ret_code_t write_record(uint16_t id, uint16_t key, uint16_t words, void *buffer) {
    fds_record_desc_t desc;
    fds_record_t record = {.file_id = id, .key = key, .data = {.p_data = buffer, .length_words = words}};
    if (find_record(id, key, &desc)) return fds_record_update(&desc, &record);
    return fds_record_write(&desc, &record);
}

bool fds_write_sync(uint16_t id, uint16_t key, uint16_t words, void *buffer) {
    if (operation.waiting || !buffer || !words || ((uintptr_t)buffer & 3u)) return false;
    for (unsigned attempt = 0; attempt < 2; attempt++) {
        begin(FDS_EVT_WRITE, id, key);
        ret_code_t result = write_record(id, key, words, buffer);
        if (result == NRF_SUCCESS) return wait_complete();
        operation.waiting = false;
        if (result != FDS_ERR_NO_SPACE_IN_FLASH || attempt) return false;
        begin(FDS_EVT_GC, 0, 0);
        if (fds_gc() != NRF_SUCCESS) { operation.waiting = false; return false; }
        if (!wait_complete()) return false;
    }
    return false;
}

int fds_delete_sync(uint16_t id, uint16_t key) {
    if (operation.waiting) return -1;
    int count = 0;
    fds_record_desc_t desc;
    while (find_record(id, key, &desc)) {
        begin(FDS_EVT_DEL_RECORD, id, key);
        if (fds_record_delete(&desc) != NRF_SUCCESS) { operation.waiting = false; return -1; }
        if (!wait_complete()) return -1;
        count++;
    }
    return count;
}

static void fds_evt_handler(const fds_evt_t *evt) {
    if (!operation.waiting) return;
    bool matches = evt->id == operation.operation;
    if (operation.operation == FDS_EVT_WRITE && (evt->id == FDS_EVT_WRITE || evt->id == FDS_EVT_UPDATE))
        matches = evt->write.file_id == operation.id && evt->write.record_key == operation.key;
    if (operation.operation == FDS_EVT_DEL_RECORD && evt->id == FDS_EVT_DEL_RECORD)
        matches = evt->del.file_id == operation.id && evt->del.record_key == operation.key;
    if (matches) { operation.result = evt->result; operation.completed = true; }
}

void fds_util_init(void) {
    APP_ERROR_CHECK(fds_register(fds_evt_handler));
    begin(FDS_EVT_INIT, 0, 0);
    ret_code_t result = fds_init();
    APP_ERROR_CHECK(result);
    APP_ERROR_CHECK_BOOL(wait_complete());
}
