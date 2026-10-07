#ifndef TEST_TAG_H
#define TEST_TAG_H
#include <stdbool.h>
#include <stdint.h>
#define TAG_MAX_SLOT_NUM 8
typedef enum {TAG_SENSE_NO,TAG_SENSE_LF,TAG_SENSE_HF} tag_sense_type_t;
typedef enum {TAG_TYPE_UNDEFINED=0,TAG_TYPE_EM410X=100,TAG_TYPE_MIFARE_1024=1001} tag_specific_type_t;
typedef struct {tag_specific_type_t tag_hf,tag_lf;} tag_slot_specific_type_t;
extern bool g_is_tag_emulating;
uint8_t tag_emulation_get_slot(void);
void tag_emulation_get_specific_types_by_slot(uint8_t,tag_slot_specific_type_t*);
bool is_slot_enabled(uint8_t,tag_sense_type_t);
void tag_emulation_change_slot(uint8_t,bool);
void tag_emulation_save(void);
void tag_emulation_sense_end(void);
void tag_emulation_sense_run(void);
#endif
