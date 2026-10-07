/* SPDX-License-Identifier: GPL-3.0-only */
#include "selection.h"
#include "learning_platform.h"
#include "rfid_main.h"
#include "app_status.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now;
static uint8_t current;
static unsigned switches,saves;
static bool scan,flash_fail,raw_field;
static device_mode_t device_mode=DEVICE_MODE_TAG;
static bool host_connected=true;
static uint8_t flash[SEL_MODEL_BYTES];
bool g_is_tag_emulating;
uint32_t bsp_monotonic_ms(void){return now;}
uint32_t app_timer_cnt_get(void){return now;}
uint32_t app_timer_cnt_diff_compute(uint32_t a,uint32_t b){return a-b;}
bool learning_slot_available(uint8_t slot){return slot<7;}
bool learning_hf_field_present(void){return raw_field&&device_mode==DEVICE_MODE_TAG;}
void learning_resume_tag_mode(void){device_mode=DEVICE_MODE_TAG;}
bool learning_reader_abandoned(void){return device_mode==DEVICE_MODE_READER&&!host_connected;}
uint8_t tag_emulation_get_slot(void){return current;}
bool learning_change_slot(uint8_t slot,bool pause){(void)pause;assert(!selection_field_active());current=slot;switches++;return true;}
bool learning_select_empty_slot(uint8_t slot){if(slot>=8||selection_field_active())return false;current=slot;return true;}
bool learning_tag_save(void){return !selection_field_active();}
bool learning_idle_pause(void){return !selection_field_active();}
void learning_idle_resume(void){}
void learning_refresh_slot(void){}
device_mode_t get_device_mode(void){return device_mode;}
void ble_environment_process(bool enabled,uint16_t period,uint16_t window){(void)period;(void)window;scan=enabled;}
bool ble_environment_active(void){return scan;}
void ble_environment_pause(void){scan=false;}
bool learning_model_read(uint16_t id,uint16_t key,uint16_t length,uint8_t *out,uint16_t *actual){(void)id;(void)key;if(!saves)return false;assert(length==sizeof(flash));memcpy(out,flash,length);*actual=length;return true;}
bool learning_model_write(uint16_t id,uint16_t key,uint16_t words,void *data){(void)id;(void)key;assert(!selection_field_active());assert(words*4u==sizeof(flash));if(flash_fail)return false;memcpy(flash,data,sizeof(flash));saves++;return true;}
static uint8_t out[512];static uint16_t size;
static uint16_t command(unsigned cmd,const uint8_t *data,unsigned length){return selection_command(cmd,data,(uint16_t)length,out,&size);}
static void mode(uint8_t m){assert(command(1108,NULL,0)==STATUS_SUCCESS);uint8_t config[12];memcpy(config,out,12);config[0]=m;assert(command(1101,config,12)==STATUS_SUCCESS);}
static void status(void){assert(command(1100,NULL,0)==STATUS_SUCCESS&&size==30);}
static void tick(uint32_t amount,uint32_t beacon){now+=amount;if(beacon)selection_ble_report(beacon,-55);selection_process();}
static void tap(void){selection_field_event(2,true);selection_reader_command(4,3);selection_reader_command(5,7);selection_field_event(2,false);tick(350,123);}
int main(void){
 selection_init();status();assert(out[1]==SEL_AUTO);
 tick(1000,123);assert(scan);
 uint8_t train[2]={2,0};assert(command(1103,train,2)==STATUS_SUCCESS);assert(command(1103,train,2)==STATUS_SUCCESS);
 tick(1000,123);assert(current==2&&switches==1); // no manual button: learned match selects a card
 assert(selection_manual_slot(1));tick(350,123);assert(current==1);
 tick(65000,123);status();assert(current==1&&out[6]==SEL_REASON_MANUAL); // no timer can override manual choice
 selection_field_event(2,true);assert(!scan);selection_reader_command(4,3);selection_field_event(1,true);
 tick(10000,456);assert(current==1&&!scan); // whole transaction stays on selected card
 selection_field_event(2,false);tick(1000,456);assert(current==1&&g_is_tag_emulating);
 selection_field_event(1,false);tick(349,456);assert(current==1);tick(1,456);assert(!g_is_tag_emulating);
 assert(selection_save());sel_model_t decoded;assert(sel_model_decode(&decoded,flash,sizeof(flash)));
 bool found=false;for(unsigned i=0;i<SEL_SAMPLES;i++)if(decoded.samples[i].slot==1&&decoded.samples[i].context.reader_count)found=true;
 assert(found); // the completed manual interaction labels the selected card
 // A new A/B press during a field queues safely and is not mistaken for the old card's interaction.
 selection_field_event(2,true);assert(selection_manual_slot(3));tick(1000,123);assert(current==1);
 selection_field_event(2,false);tick(350,123);assert(current==3);tick(35000,123);assert(current==3);tap();
 // Manual latch was consumed by that tap; a different trained environment can now switch.
 train[0]=4;tick(21000,999);assert(command(1103,train,2)==STATUS_SUCCESS);assert(command(1103,train,2)==STATUS_SUCCESS);
 tick(1000,999);assert(current==4);
 // Automatic switching is never a training label, and off still disables learning/automatic operation.
 assert(selection_save());assert(sel_model_decode(&decoded,flash,sizeof(flash)));unsigned count=sel_sample_count(&decoded);
 tick(1000,999);assert(selection_save());assert(sel_model_decode(&decoded,flash,sizeof(flash)));assert(sel_sample_count(&decoded)==count);
 mode(SEL_OFF);assert(selection_manual_slot(5));tick(350,999);assert(current==5);tap();tick(60000,999);assert(current==5);
 mode(SEL_OBSERVE);flash_fail=true;assert(!selection_save());status();assert(out[13]&&out[15]);flash_fail=false;assert(selection_save());
 assert(!selection_management_slot(8));selection_field_event(2,true);assert(!selection_management_slot(7));selection_field_event(2,false);assert(selection_management_slot(7));
 // Disabling tag hardware during an active exchange need not emit FIELD_LOST.
 // Clearing the aborted session must release the field interlock without training it.
 assert(selection_manual_slot(2));tick(350,123);selection_field_event(2,true);selection_reader_command(4,3);selection_field_event(1,true);
 assert(g_is_tag_emulating);device_mode=DEVICE_MODE_READER;assert(selection_save()==false);
 selection_emulation_stopped();assert(!selection_field_active()&&!g_is_tag_emulating);
 assert(selection_save());assert(sel_model_decode(&decoded,flash,sizeof(flash)));count=sel_sample_count(&decoded);
 tick(350,123);assert(selection_save());assert(sel_model_decode(&decoded,flash,sizeof(flash)));assert(sel_sample_count(&decoded)==count);
 // Host slot management must not turn a GUI reader into an emulator.
 device_mode=DEVICE_MODE_READER;assert(selection_management_slot(2));tick(350,123);assert(device_mode==DEVICE_MODE_READER&&current==2);
 host_connected=false;tick(1000,123);assert(device_mode==DEVICE_MODE_TAG);host_connected=true;device_mode=DEVICE_MODE_READER;
 // Physical A/B requests do restore emulation even if GUI left reader mode active.
 assert(selection_manual_slot(3));tick(350,123);assert(device_mode==DEVICE_MODE_TAG&&current==3);
 // A hardware field can precede its queued IRQ; neither flash nor slot changes may begin then.
 assert(selection_manual_slot(4));raw_field=true;tick(1000,999);assert(current==3&&!scan);
 assert(!selection_prepare_sleep(false));raw_field=false;tick(350,999);assert(current==4);
 // AUTO/OBSERVE no longer block upstream idle sleep. Dirty samples are saved,
 // passive scans stop, and a manual choice survives a System OFF wake.
 mode(SEL_AUTO);tick(1000,999);assert(scan);assert(selection_prepare_sleep(false));assert(!scan);
 selection_restore_manual_hold(true);tick(1000,999);status();assert(current==4&&out[6]==SEL_REASON_MANUAL);
 assert(selection_prepare_sleep(false));selection_restore_manual_hold(false); // cold reset discards the retained latch
 tick(1000,999);status();assert(out[6]!=SEL_REASON_MANUAL);
 mode(SEL_OBSERVE);flash_fail=true;assert(!selection_prepare_sleep(false));assert(!scan);
 assert(selection_prepare_sleep(true)); // low battery cannot be held awake by a failed model write
 flash_fail=false;assert(selection_save());
 selection_field_event(2,true);assert(!selection_prepare_sleep(false));assert(selection_prepare_sleep(true));selection_field_event(2,false);
 puts("manual latch, reader-to-tag A/B, raw-field interlock, System OFF retention, idle sleep, low battery and learning passed");
}
