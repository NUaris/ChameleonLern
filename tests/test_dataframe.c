/* SPDX-License-Identifier: GPL-3.0-only */
#include "dataframe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned received;
static uint16_t last_cmd, last_length;
static data_frame_channel_t source;
static uint8_t output[512];
static void callback(uint16_t cmd,uint16_t status,uint16_t length,uint8_t *data) {
    assert(status==0x68); received++; last_cmd=cmd; last_length=length; source=data_frame_source();
    if(length) memcpy(output,data,length);
}
int main(void) {
    uint8_t data[512], frame[522]; for(unsigned i=0;i<512;i++) data[i]=(uint8_t)i;
    on_data_frame_complete(callback);
    data_frame_tx_t *tx=data_frame_make(1100,0x68,512,data); assert(tx&&tx->length==522); memcpy(frame,tx->buffer,522);
    for(unsigned i=0;i<522;i++) data_frame_receive_from(DATA_FRAME_BLE,frame+i,1);
    data_frame_process(); assert(received==1&&source==DATA_FRAME_BLE&&last_length==512&&!memcmp(data,output,512));
    assert(!data_frame_make(1,0,513,data)); assert(!data_frame_make(1,0,1,NULL));
    frame[8]^=1; data_frame_receive(frame,522); data_frame_process(); assert(received==1); frame[8]^=1;
    frame[521]^=1; data_frame_receive(frame,522); data_frame_process(); assert(received==1); frame[521]^=1;
    uint8_t noise[]={0,0x11,0x11}; data_frame_receive(noise,sizeof(noise));
    data_frame_receive(frame,522); data_frame_process(); assert(received==2);
    /* Simultaneous fragmented traffic must not mix the two channels. */
    data_frame_receive_from(DATA_FRAME_USB,frame,80); data_frame_receive_from(DATA_FRAME_BLE,frame,77);
    data_frame_receive_from(DATA_FRAME_USB,frame+80,442); data_frame_receive_from(DATA_FRAME_BLE,frame+77,445);
    data_frame_process(); assert(received==4);
    for(unsigned i=0;i<10;i++) data_frame_receive(frame,522);
    for(unsigned i=0;i<10;i++) data_frame_process(); assert(received==7); /* Bounded queue, no overwrite. */
    data_frame_tick(100); data_frame_receive(frame,20); data_frame_tick(2201); data_frame_receive(frame,522);
    data_frame_process(); assert(received==8);
    tx=data_frame_make(1108,0x68,0,NULL); data_frame_receive(tx->buffer,tx->length); data_frame_process(); assert(received==9&&last_cmd==1108&&last_length==0);
    data_frame_receive(frame,40); data_frame_reset_channel(DATA_FRAME_USB);
    data_frame_receive(frame,522); data_frame_process(); assert(received==10);
    data_frame_receive(frame,522); data_frame_reset_channel(DATA_FRAME_USB);
    data_frame_process(); assert(received==10); /* A disconnected client's queued command cannot execute. */
    data_frame_receive(frame,522); data_frame_process(); assert(received==11);
    uint32_t random=1234;
    for(unsigned j=0;j<30000;j++){random=random*1664525+1013904223; uint8_t b=(uint8_t)(random>>24); data_frame_receive_from((data_frame_channel_t)(j%2),&b,1); data_frame_process();}
    puts("dataframe: maximum payload, fragments, checksums, channel isolation, saturation, timeout and noise passed");
}
