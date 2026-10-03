/* SPDX-License-Identifier: GPL-3.0-only */
#include "tag_emulation.h"
#include "nfc_mf1.h"
#include "tag_persistence.h"
#include "fds_util.h"
#include "rfid_main.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static struct {uint16_t id,key,length;bool used;uint8_t data[4500];} records[20];
static nfc_tag_mf1_information_t *hf;
static uint8_t *lf;
static bool field,flash_fail;
bool raw_hf_field;
static unsigned writes;
bool selection_field_active(void){return field;}
device_mode_t get_device_mode(void){return DEVICE_MODE_TAG;}
void nfc_tag_14a_sense_switch(bool enabled){(void)enabled;}
void lf_tag_125khz_sense_switch(bool enabled){(void)enabled;}
static int find(uint16_t id,uint16_t key){for(int i=0;i<20;i++)if(records[i].used&&records[i].id==id&&records[i].key==key)return i;return -1;}
bool fds_exists(uint16_t id,uint16_t key){return find(id,key)>=0;}
bool fds_read_sync_size(uint16_t id,uint16_t key,uint16_t maximum,uint8_t *out,uint16_t *length){int i=find(id,key);if(i<0||records[i].length>maximum)return false;*length=records[i].length;memcpy(out,records[i].data,*length);return true;}
bool fds_write_sync(uint16_t id,uint16_t key,uint16_t words,void *data){writes++;if(flash_fail)return false;int i=find(id,key);if(i<0){for(i=0;i<20;i++)if(!records[i].used)break;}assert(i<20&&words*4u<=4500);records[i].id=id;records[i].key=key;records[i].length=words*4;records[i].used=true;memcpy(records[i].data,data,words*4);return true;}
int fds_delete_sync(uint16_t id,uint16_t key){int i=find(id,key);if(i<0)return 0;if(flash_fail)return -1;records[i].used=false;return 1;}
uint16_t nfc_tag_mf1_block_count(tag_specific_type_t type){return type==TAG_TYPE_MIFARE_1024?64:0;}
int get_information_size_by_tag_type(tag_specific_type_t type,bool align){(void)align;return type==TAG_TYPE_MIFARE_1024?((int)offsetof(nfc_tag_mf1_information_t,memory)+1024+3)&~3:0;}
int nfc_tag_mf1_data_loadcb(tag_specific_type_t type,tag_data_buffer_t *buffer){hf=buffer?(nfc_tag_mf1_information_t*)buffer->buffer:NULL;return buffer?get_information_size_by_tag_type(type,true):0;}
int nfc_tag_mf1_data_savecb(tag_specific_type_t type,tag_data_buffer_t *buffer){(void)buffer;return get_information_size_by_tag_type(type,true);}
int lf_tag_em410x_data_loadcb(tag_specific_type_t type,tag_data_buffer_t *buffer){(void)type;lf=buffer?buffer->buffer:NULL;return buffer?5:0;}
int lf_tag_em410x_data_savecb(tag_specific_type_t type,tag_data_buffer_t *buffer){(void)type;(void)buffer;return 5;}
bool nfc_tag_mf1_data_factory(uint8_t slot,tag_specific_type_t type){nfc_tag_mf1_information_t info={0};info.res_coll.size=4;info.memory[0][0]=slot+10;fds_slot_record_map_t map;get_fds_map_by_slot_sense_type(slot,TAG_SENSE_HF,&map);return fds_write_sync(map.id,map.key,get_information_size_by_tag_type(type,true)/4,&info);}
bool lf_tag_em410x_data_factory(uint8_t slot,tag_specific_type_t type){(void)type;uint32_t data[2]={0x04030201,5};fds_slot_record_map_t map;get_fds_map_by_slot_sense_type(slot,TAG_SENSE_LF,&map);return fds_write_sync(map.id,map.key,2,data);}
int main(void){
 tag_emulation_init();assert(!hf&&!lf);assert(!tag_emulation_slot_available(0));
 assert(tag_emulation_factory_data(0,TAG_TYPE_MIFARE_1024));assert(hf&&hf->memory[0][0]==10);
 assert(tag_emulation_factory_data(0,TAG_TYPE_EM410X));assert(lf&&lf[0]==1);assert(tag_emulation_save());
 tag_emulation_set_slot(1);tag_emulation_load_data();assert(!hf&&!lf); /* Missing record must clear previous identity. */
 tag_emulation_set_slot(0);tag_emulation_load_data();assert(hf&&lf);
 assert(tag_emulation_factory_data(1,TAG_TYPE_MIFARE_1024));assert(hf->memory[0][0]==10); /* Creating an inactive slot must not load it. */
 assert(tag_emulation_change_slot(1,true)&&hf->memory[0][0]==11&&!lf);
 assert(tag_emulation_change_slot(0,true));assert(!tag_emulation_change_slot(8,true));
 field=true;assert(!tag_emulation_change_slot(1,true));assert(!tag_emulation_save());field=false;
 raw_hf_field=true;assert(!tag_emulation_change_slot(1,true));raw_hf_field=false;
 fds_slot_record_map_t map;get_fds_map_by_slot_sense_type(1,TAG_SENSE_HF,&map);int record=find(map.id,map.key);records[record].length=4;
 assert(!tag_emulation_change_slot(1,true));assert(tag_emulation_get_slot()==0&&hf&&lf&&hf->memory[0][0]==10);
 uint8_t block[16]={88};assert(!tag_emulation_set_mf1_blocks(64,1,block));assert(tag_emulation_set_mf1_blocks(63,1,block));
 flash_fail=true;unsigned before=writes;assert(!tag_emulation_save());assert(writes>before);
 flash_fail=false;before=writes;assert(tag_emulation_save());assert(writes>before); /* Failed save cannot advance CRC. */
 tag_emulation_delete_data(255,TAG_SENSE_HF); /* Invalid delete cannot index config. */
 for(uint8_t i=0;i<8;i++)set_tag_emulation_slot_enable(i,false);
 assert(find_next_tag_emulation_slot(0)==0&&find_prev_tag_emulation_slot(0)==0);
 assert(find_next_tag_emulation_slot(255)==0);
 puts("tags: empty-slot clearing, inactive-slot setup, field guard, invalid-record rollback, block bounds and failed-save retry passed");
}
