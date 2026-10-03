#include <stdbool.h>
#include <stdint.h>
extern volatile bool g_is_tag_emulating;
bool tag_emulation_slot_available(uint8_t);
uint8_t tag_emulation_get_slot(void);
bool tag_emulation_change_slot(uint8_t,bool);
bool tag_emulation_save(void);
bool tag_emulation_idle_pause(void);
void tag_emulation_idle_resume(void);
