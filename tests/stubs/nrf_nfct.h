#include <stdint.h>
#include <stdbool.h>
extern bool raw_hf_field;
#define NRF_NFCT_FIELD_STATE_PRESENT_MASK 1
static inline uint8_t nrf_nfct_field_status_get(void){return raw_hf_field;}
