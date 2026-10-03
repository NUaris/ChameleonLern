/* SPDX-License-Identifier: GPL-3.0-only */
#include "fds_util.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void (*handler)(const fds_evt_t *);
static uint32_t clock_ms, value[2]={123,456};
static unsigned closes, opens, writes, garbage;
static bool exists=true, no_space, fail_completion;
static fds_evt_t pending;
uint32_t app_timer_cnt_get(void){return clock_ms;}
uint32_t app_timer_cnt_diff_compute(uint32_t a,uint32_t b){return a-b;}
ret_code_t fds_register(void (*cb)(const fds_evt_t*)){handler=cb;return 0;}
ret_code_t fds_init(void){pending=(fds_evt_t){.id=FDS_EVT_INIT};return 0;}
int sd_app_evt_wait(void){clock_ms++;handler(&pending);return 0;}
ret_code_t fds_record_find(uint16_t id,uint16_t key,fds_record_desc_t *desc,fds_find_token_t *token){(void)id;(void)key;(void)desc;(void)token;return exists?0:NRF_ERROR_NOT_FOUND;}
ret_code_t fds_record_open(fds_record_desc_t *desc,fds_flash_record_t *record){(void)desc;static const fds_header_t header={2};*record=(fds_flash_record_t){&header,value};opens++;return 0;}
ret_code_t fds_record_close(fds_record_desc_t *desc){(void)desc;closes++;return 0;}
ret_code_t fds_record_write(fds_record_desc_t *desc,fds_record_t *record){(void)desc;writes++;if(no_space){no_space=false;return FDS_ERR_NO_SPACE_IN_FLASH;}pending=(fds_evt_t){.id=FDS_EVT_WRITE,.result=fail_completion?NRF_ERROR_BUSY:0,.write={record->file_id,record->key}};exists=true;return 0;}
ret_code_t fds_record_update(fds_record_desc_t *desc,fds_record_t *record){ret_code_t result=fds_record_write(desc,record);pending.id=FDS_EVT_UPDATE;return result;}
ret_code_t fds_record_delete(fds_record_desc_t *desc){(void)desc;pending=(fds_evt_t){.id=FDS_EVT_DEL_RECORD,.del={42,7}};exists=false;return 0;}
ret_code_t fds_gc(void){garbage++;pending=(fds_evt_t){.id=FDS_EVT_GC};return 0;}
int main(void){
 fds_util_init(); uint32_t out[2]={0};uint16_t size;
 assert(fds_read_sync_size(42,7,8,(uint8_t*)out,&size)&&size==8&&!memcmp(value,out,8));
 assert(!fds_read_sync_size(42,7,4,(uint8_t*)out,&size)&&size==0); assert(opens==2&&closes==2);
 assert(!fds_write_sync(42,7,2,(uint8_t*)out+1));
 assert(fds_write_sync(42,7,2,value)); assert(writes==1);
 fail_completion=true;assert(!fds_write_sync(42,7,2,value));fail_completion=false;
 no_space=true;assert(fds_write_sync(42,7,2,value));assert(garbage==1&&writes==4);
 assert(fds_delete_sync(42,7)==1);assert(!fds_exists(42,7));
 assert(fds_write_sync(42,7,2,value));assert(fds_exists(42,7));
 puts("fds: open/close balance, alignment, update completion, failed flash event, GC retry and delete passed");
}
