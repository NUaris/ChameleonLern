#ifndef BLE_MAIN_H
#define BLE_MAIN_H
#include <stdint.h>
#include <stdbool.h>
void ble_slave_init(void);
void advertising_start(void);
bool ble_command_write(const void *data, uint16_t length);
void ble_command_process(void);
void ble_environment_process(bool enabled, uint16_t period_ms, uint16_t window_ms);
bool ble_environment_active(void);
#endif
