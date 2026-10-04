/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CHAMELEON_STORAGE_IDS_H
#define CHAMELEON_STORAGE_IDS_H

/* Private records; leave current and historical official records untouched.
 * Official file IDs: 0x1000, 0x1001, 0x1066..0x1069,
 * 0x1100..0x1107, 0x1200..0x1207. Peer Manager reserves >=0xC000.
 * Shared FDS allocation is 22 pages of 2048 words on S140 7.2.0.
 */
#define CL_CONFIG_FILE 0x4C00
#define CL_CONFIG_KEY 1
#define CL_TAG_HF_FILE 0x4C01
#define CL_TAG_LF_FILE 0x4C02
#define CL_TAG_KEY_BASE 1
#define CL_MODEL_FILE 0x4C10
#define CL_MODEL_KEY 1
#endif
