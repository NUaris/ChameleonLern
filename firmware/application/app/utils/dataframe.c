#include "dataframe.h"
#include "hex_utils.h"
#include <string.h>

#if defined(__GNUC__)
#define QUEUE_BARRIER() __asm__ volatile("" ::: "memory")
#else
#define QUEUE_BARRIER() __schedule_barrier()
#endif
#define QUEUE_DEPTH 4
#define FRAME_SIZE (DATA_FRAME_MAX_DATA + 10)

typedef struct {
    uint16_t cmd, status, length;
    uint32_t generation;
    uint8_t data[DATA_FRAME_MAX_DATA];
} packet_t;

typedef struct {
    uint8_t bytes[FRAME_SIZE], checksum;
    uint16_t position, payload;
    uint32_t last_ms;
    packet_t packets[QUEUE_DEPTH];
    volatile uint8_t head, tail;
    volatile uint32_t generation;
} receiver_t;

/* Each channel has one producer: USB in the main loop, BLE in its IRQ callback. */
static receiver_t receivers[2];
static data_frame_cbk_t callback;
static data_frame_channel_t active_channel;
static uint8_t tx_bytes[FRAME_SIZE];
static data_frame_tx_t tx = {.buffer = tx_bytes};
static volatile uint32_t current_ms;

static void reset(receiver_t *r) { r->position = 0; r->checksum = 0; r->payload = 0; }

data_frame_tx_t *data_frame_make(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length > DATA_FRAME_MAX_DATA || (length && !data)) return NULL;
    tx_bytes[0] = 0x11; tx_bytes[1] = 0xef;
    num_to_bytes(cmd, 2, tx_bytes + 2); num_to_bytes(status, 2, tx_bytes + 4);
    num_to_bytes(length, 2, tx_bytes + 6);
    uint8_t sum = 0;
    for (unsigned i = 0; i < 8; i++) sum += tx_bytes[i];
    tx_bytes[8] = (uint8_t)(0u - sum);
    if (length) memcpy(tx_bytes + 9, data, length);
    sum = 0;
    for (unsigned i = 0; i < length; i++) sum += tx_bytes[9 + i];
    tx_bytes[9 + length] = (uint8_t)(0u - sum); tx.length = length + 10;
    return &tx;
}

void data_frame_receive_from(data_frame_channel_t channel, const uint8_t *data, uint16_t length) {
    if (channel > DATA_FRAME_BLE || (!data && length)) return;
    receiver_t *r = &receivers[channel];
    for (unsigned i = 0; i < length; i++) {
        uint8_t byte = data[i];
        r->last_ms = current_ms;
        if (!r->position && byte != 0x11) continue;
        r->bytes[r->position] = byte;
        r->checksum = (uint8_t)(r->checksum + byte);
        if ((r->position == 1 || r->position == 8) && r->checksum) { reset(r); if (byte == 0x11) { r->bytes[0] = byte; r->checksum = byte; r->position = 1; } continue; }
        if (r->position == 8) {
            r->payload = (uint16_t)bytes_to_num(r->bytes + 6, 2);
            if (r->payload > DATA_FRAME_MAX_DATA) { reset(r); continue; }
        }
        if (r->position >= 9 && r->position == r->payload + 9) {
            uint8_t next = (uint8_t)((r->head + 1) % QUEUE_DEPTH);
            if (!r->checksum && next != r->tail) {
                packet_t *p = &r->packets[r->head];
                p->generation = r->generation;
                p->cmd = (uint16_t)bytes_to_num(r->bytes + 2, 2);
                p->status = (uint16_t)bytes_to_num(r->bytes + 4, 2); p->length = r->payload;
                memcpy(p->data, r->bytes + 9, p->length);
                QUEUE_BARRIER();
                r->head = next;
            }
            reset(r); continue;
        }
        r->position++;
        if (r->position >= FRAME_SIZE) reset(r);
    }
}

void data_frame_reset_channel(data_frame_channel_t channel) {
    if (channel > DATA_FRAME_BLE) return;
    receiver_t *r = &receivers[channel];
    reset(r); r->generation++;
}

void data_frame_receive(uint8_t *data, uint16_t length) { data_frame_receive_from(DATA_FRAME_USB, data, length); }
void data_frame_tick(uint32_t now) {
    current_ms = now;
    for (unsigned i = 0; i < 2; i++) if (receivers[i].position && (uint32_t)(now - receivers[i].last_ms) > 2000) reset(&receivers[i]);
}

data_frame_channel_t data_frame_source(void) { return active_channel; }
void data_frame_process(void) {
    for (unsigned i = 0; i < 2; i++) {
        receiver_t *r = &receivers[i]; if (r->head == r->tail) continue;
        packet_t *p = &r->packets[r->tail]; active_channel = (data_frame_channel_t)i;
        QUEUE_BARRIER();
        if (callback && p->generation == r->generation) callback(p->cmd, p->status, p->length, p->length ? p->data : NULL);
        QUEUE_BARRIER();
        r->tail = (uint8_t)((r->tail + 1) % QUEUE_DEPTH);
    }
}
void on_data_frame_complete(data_frame_cbk_t cb) { callback = cb; }
