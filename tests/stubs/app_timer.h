#ifndef TEST_TIMER_H
#define TEST_TIMER_H
#include <stdint.h>
#define APP_TIMER_TICKS(ms) (ms)
uint32_t app_timer_cnt_get(void);
uint32_t app_timer_cnt_diff_compute(uint32_t,uint32_t);
#endif
