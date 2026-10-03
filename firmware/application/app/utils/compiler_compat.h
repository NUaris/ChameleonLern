/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CHAMELEON_COMPILER_COMPAT_H
#define CHAMELEON_COMPILER_COMPAT_H
#include "nrf.h"
#if defined(__GNUC__)
/* Legacy application files use ARMCC spellings for CMSIS intrinsics. */
#define __nop __NOP
#define __rev __REV
#endif
#endif
