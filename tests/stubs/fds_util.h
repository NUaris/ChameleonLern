#include <stdint.h>
#include <stdbool.h>
bool fds_is_exists(uint16_t,uint16_t);
bool fds_read_sync(uint16_t,uint16_t,uint16_t*,uint8_t*);
bool fds_write_sync(uint16_t,uint16_t,uint16_t,void*);
