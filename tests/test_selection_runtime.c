/* SPDX-License-Identifier: GPL-3.0-only */
#include "selection.h"
#include "fds_util.h"
#include "rfid_main.h"
#include "app_status.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now;
static uint8_t current;
static unsigned switches,saves;
static bool scan,flash_fail;
static uint8_t flash[SEL_MODEL_BYTES];
volatile bool g_is_tag_emulating;
uint32_t bsp_monotonic_ms(void){return now;}
uint32_t app_timer_cnt_get(void){return now;}
uint32_t app_timer_cnt_diff_compute(uint32_t a,uint32_t b){return a-b;}
bool tag_emulation_slot_available(uint8_t slot){return slot<7;}
uint8_t tag_emulation_get_slot(void){return current;}
bool tag_emulation_change_slot(uint8_t slot,bool pause){(void)pause;assert(!selection_field_active());current=slot;switches++;return true;}
bool tag_emulation_save(void){return !selection_field_active();}
bool tag_emulation_idle_pause(void){return !selection_field_active();}
void tag_emulation_idle_resume(void){}
device_mode_t get_device_mode(void){return DEVICE_MODE_TAG;}
void light_up_by_slot(void){}
void set_slot_ligth_color(unsigned color){(void)color;}
void ble_environment_process(bool enabled,uint16_t period,uint16_t window){(void)period;(void)window;scan=enabled;}
bool ble_environment_active(void){return scan;}
bool fds_read_sync_size(uint16_t id,uint16_t key,uint16_t length,uint8_t *out,uint16_t *actual){(void)id;(void)key;if(!saves)return false;assert(length==sizeof(flash));memcpy(out,flash,length);*actual=length;return true;}
bool fds_write_sync(uint16_t id,uint16_t key,uint16_t words,void *data){(void)id;(void)key;assert(!selection_field_active());assert(words*4u==sizeof(flash));if(flash_fail)return false;memcpy(flash,data,sizeof(flash));saves++;return true;}
static uint8_t output[512];static uint16_t size;
static uint16_t command(unsigned cmd,const uint8_t *data,unsigned length){return selection_command(cmd,data,(uint16_t)length,output,&size);}
static void mode(uint8_t m){assert(command(1108,NULL,0)==STATUS_DEVICE_SUCCESS);uint8_t config[12];memcpy(config,output,12);config[0]=m;assert(command(1101,config,12)==STATUS_DEVICE_SUCCESS);}
static void status(void){assert(command(1100,NULL,0)==STATUS_DEVICE_SUCCESS&&size==30);}
int main(void){
 selection_init();assert(!selection_background_enabled());status();assert(output[1]==SEL_OFF);
 mode(SEL_OBSERVE);now=1000;selection_ble_report(123,-55);selection_process();assert(scan);
 uint8_t train[2]={2,0};assert(command(1103,train,2)==STATUS_DEVICE_SUCCESS);assert(command(1103,train,2)==STATUS_DEVICE_SUCCESS);
 assert(command(1105,NULL,0)==STATUS_DEVICE_SUCCESS);assert(output[0]==2&&output[3]==SEL_REASON_READY);
 assert(selection_save()&&saves==1);mode(SEL_AUTO);now=32000;selection_ble_report(123,-55);selection_process();assert(current==2&&switches==1);
 now+=1000;selection_process();status();assert(output[6]==SEL_REASON_COOLDOWN);
 mode(SEL_OFF);selection_field_event(2,true);selection_field_event(1,true);assert(g_is_tag_emulating);
 assert(selection_manual_slot(1));selection_process();assert(current==2&&!scan);
 selection_field_event(2,false);now+=500;selection_process();assert(current==2);assert(!selection_save());
 selection_field_event(1,false);assert(!g_is_tag_emulating);selection_process();assert(current==2);
 now+=350;selection_process();assert(current==1);
 mode(SEL_AUTO);now+=1000;selection_ble_report(123,-55);selection_process();status();assert(current==1&&output[6]==SEL_REASON_MANUAL);
 now+=30000;selection_ble_report(123,-55);selection_process();assert(current==2);
 now+=21000;selection_process();assert(command(1105,NULL,0)==STATUS_DEVICE_SUCCESS&&output[3]==SEL_REASON_NO_CONTEXT);
 train[1]=1;assert(command(1103,train,2)==STATUS_NO_CONTEXT);assert(!selection_manual_slot(7));
 mode(SEL_OBSERVE);flash_fail=true;assert(!selection_save());status();assert(output[13]&&output[15]);
 flash_fail=false;assert(selection_save());status();assert(!output[13]&&!output[15]);
 sel_model_t restored;assert(sel_model_decode(&restored,flash,sizeof(flash))&&sel_sample_count(&restored)==1);
 puts("selection runtime: observe/auto, field interlock, queued manual selection, holds, stale context and flash retry passed");
}
