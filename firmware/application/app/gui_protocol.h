/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CHAMELEON_GUI_PROTOCOL_H
#define CHAMELEON_GUI_PROTOCOL_H
#include <stdint.h>
#include <stdbool.h>

/* Management subset of the current official wire format, not an RF feature claim. */
#define CHAMELEON_PROTOCOL_MAJOR 2
#define CHAMELEON_PROTOCOL_MINOR 0
#define CHAMELEON_PROJECT_VERSION "ChameleonLern-v0.1.0-alpha.3"
#define GUI_COMMANDS(X) \
    X(1004) X(1005) X(1006) X(1007) X(1008) X(1009) \
    X(1018) X(1019) X(1021) X(1023) X(1024) X(1038) \
    X(4000) X(4001) X(4008) X(4009) X(4010) X(4011) \
    X(4012) X(4014) X(4015) X(4016) X(4017) X(4018) \
    X(5000) X(5001)

uint16_t gui_protocol_command(uint16_t command, const uint8_t *data,
                              uint16_t length, uint8_t output[512], uint16_t *size);
bool gui_protocol_select_slot(uint8_t slot);
void gui_protocol_init(void);
#endif
