#ifndef TEST_RFID_MAIN_H
#define TEST_RFID_MAIN_H
typedef enum {DEVICE_MODE_NONE, DEVICE_MODE_READER, DEVICE_MODE_TAG} device_mode_t;
device_mode_t get_device_mode(void);
void tag_mode_enter(void);
void light_up_by_slot(void);
void apply_slot_change(uint8_t,uint8_t);
void set_slot_ligth_color(unsigned);

#define TAG_FIELD_LED_OFF() ((void)0)
#define ARRAY_SIZE(a) ((int)(sizeof(a)/sizeof((a)[0])))
#endif
