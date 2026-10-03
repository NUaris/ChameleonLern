#ifndef USB_MAIN_H
#define USB_MAIN_H

#include <stdint.h>
#include <stdbool.h>

void usb_cdc_init(void);
bool usb_cdc_write(const void *p_buf, uint16_t length);
void usb_cdc_process(void);

#endif
