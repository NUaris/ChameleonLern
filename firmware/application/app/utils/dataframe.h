#ifndef DATA_PACK_H
#define DATA_PACK_H

#include <stdint.h>
#include <stdbool.h>


#define DATA_FRAME_MAX_DATA 512
typedef enum { DATA_FRAME_USB, DATA_FRAME_BLE } data_frame_channel_t;
void data_frame_receive_from(data_frame_channel_t channel, const uint8_t *data, uint16_t length);
void data_frame_tick(uint32_t now_ms);
void data_frame_reset_channel(data_frame_channel_t channel);
data_frame_channel_t data_frame_source(void);

// Data frame process callback
typedef void (*data_frame_cbk_t)(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);
// TX buffer
typedef struct {
    uint8_t* const buffer;
    uint16_t length;
} data_frame_tx_t;


void data_frame_receive(uint8_t *data, uint16_t length);
void data_frame_process(void);
void on_data_frame_complete(data_frame_cbk_t callback);

data_frame_tx_t* data_frame_make(
    uint16_t cmd, 
    uint16_t status, 
    uint16_t length, 
    uint8_t *data
);


#endif
