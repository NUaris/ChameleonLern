/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CHAMELEON_DFU_ENTRY_H
#define CHAMELEON_DFU_ENTRY_H
#include <stdint.h>
#include <stdbool.h>
uint16_t dfu_entry_request(void);
bool app_cmd_dfu_pending(void);
void app_cmd_process(void);
#endif
