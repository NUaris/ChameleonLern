/* SPDX-License-Identifier: GPL-3.0-only */
#include "nrf.h"
extern void __libc_init_array(void);
extern int main(void);
void _init(void) {}
void _fini(void) {}
/* The SDK assembly copies .data and clears .bss before calling this entry. */
void application_start(void) {
    __libc_init_array();
    (void)main();
    for (;;) __WFE();
}
