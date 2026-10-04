/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef BOARD_CHAMELEON_ULTRA_H
#define BOARD_CHAMELEON_ULTRA_H

/* Production Ultra HW v1, upstream hw_connect.c at
 * 6d92a9ff1a56f93efbcaca10f547eee0a3dd6791. Not the 2022 prototype or Lite.
 * DFU hw_version is the device type (0), not this board revision (1).
 */
#include "nrf_gpio.h"
#define CHAMELEON_DEVICE_MODEL 0u
#define CHAMELEON_BOARD_REVISION 1u
#define LED_FIELD NRF_GPIO_PIN_MAP(1, 1)
#define LED_R NRF_GPIO_PIN_MAP(0, 24)
#define LED_G NRF_GPIO_PIN_MAP(0, 22)
#define LED_B NRF_GPIO_PIN_MAP(1, 0)
#define LED_1 NRF_GPIO_PIN_MAP(0, 20)
#define LED_2 NRF_GPIO_PIN_MAP(0, 17)
#define LED_3 NRF_GPIO_PIN_MAP(0, 15)
#define LED_4 NRF_GPIO_PIN_MAP(0, 13)
#define LED_5 NRF_GPIO_PIN_MAP(0, 12)
#define LED_6 NRF_GPIO_PIN_MAP(1, 9)
#define LED_7 NRF_GPIO_PIN_MAP(0, 8)
#define LED_8 NRF_GPIO_PIN_MAP(0, 6)
#define RGB_LIST_NUM 8
#define RGB_CTRL_NUM 3
#define LF_ANT_DRIVER NRF_GPIO_PIN_MAP(0, 31)
#define LF_OA_OUT NRF_GPIO_PIN_MAP(0, 29)
#define LF_MOD NRF_GPIO_PIN_MAP(1, 13)
#define LF_RSSI_PIN NRF_GPIO_PIN_MAP(0, 2)
#define LF_RSSI NRF_LPCOMP_INPUT_0
#define HF_SPI_SELECT NRF_GPIO_PIN_MAP(1, 6)
#define HF_SPI_MISO NRF_GPIO_PIN_MAP(0, 11)
#define HF_SPI_MOSI NRF_GPIO_PIN_MAP(1, 7)
#define HF_SPI_SCK NRF_GPIO_PIN_MAP(1, 4)
#define HF_ANT_SEL NRF_GPIO_PIN_MAP(1, 10)
#define BUTTON_1 NRF_GPIO_PIN_MAP(1, 2)
#define BUTTON_2 NRF_GPIO_PIN_MAP(0, 26)
/* This historical application uses BAT_SENSE only as a GPIO, not an ADC input. */
#define BAT_SENSE NRF_GPIO_PIN_MAP(0, 4)
#define READER_POWER NRF_GPIO_PIN_MAP(1, 15)
#endif
