/* SPDX-License-Identifier: GPL-3.0-only */
#include "learning_platform.h"
#include "tag_emulation.h"
#include "tag_persistence.h"
#include "rfid_main.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static bool field,record,enabled;static unsigned pauses,resumes,saves,bytes;static uint8_t current;
bool selection_field_active(void){return field;}
void tag_emulation_get_specific_types_by_slot(uint8_t slot,tag_slot_specific_type_t *t){t->tag_hf=slot==1?TAG_TYPE_MIFARE_1024:TAG_TYPE_UNDEFINED;t->tag_lf=TAG_TYPE_UNDEFINED;}
bool is_slot_enabled(uint8_t slot,tag_sense_type_t s){return slot==1&&s==TAG_SENSE_HF&&enabled;}
void get_fds_map_by_slot_sense_type_for_dump(uint8_t slot,tag_sense_type_t s,fds_slot_record_map_t *m){m->id=0x1100+slot;m->key=s;}
bool fds_is_exists(uint16_t id,uint16_t key){return record&&id==0x1101&&key==TAG_SENSE_HF;}
uint8_t tag_emulation_get_slot(void){return current;}
void tag_emulation_change_slot(uint8_t s,bool pause){assert(!field);(void)pause;current=s;}
void tag_emulation_sense_end(void){pauses++;}
void tag_emulation_sense_run(void){resumes++;}
void tag_emulation_save(void){saves++;}
device_mode_t get_device_mode(void){return DEVICE_MODE_TAG;}
void apply_slot_change(uint8_t a,uint8_t b){(void)a;(void)b;}
bool fds_read_sync(uint16_t id,uint16_t key,uint16_t *max,uint8_t *out){(void)id;(void)key;assert(*max==8);memset(out,1,4);*max=4;return true;}
bool fds_write_sync(uint16_t id,uint16_t key,uint16_t length,void *data){(void)id;(void)key;(void)data;bytes=length;return true;}
int main(void){
 assert(!learning_slot_available(8));assert(!learning_slot_available(1));enabled=true;assert(!learning_slot_available(1));record=true;assert(learning_slot_available(1));
 field=true;assert(!learning_change_slot(1,true));assert(!learning_tag_save());assert(!learning_select_empty_slot(7));assert(!pauses&&!saves);
 field=false;assert(learning_change_slot(1,true)&&current==1);assert(learning_select_empty_slot(7)&&current==7);assert(!learning_select_empty_slot(8));
 assert(learning_tag_save()&&pauses==1&&resumes==1&&saves==1);
 uint8_t data[8];uint16_t actual=0;assert(learning_model_read(0x4c10,1,8,data,&actual)&&actual==4);assert(learning_model_write(0x4c10,1,2,data)&&bytes==8);
 puts("official slot storage, field interlock and model byte/word adapter passed");
}
