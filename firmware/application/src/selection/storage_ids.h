/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CHAMELEON_STORAGE_IDS_H
#define CHAMELEON_STORAGE_IDS_H
/* Official cards/config/names use 0x1000/0x1001/0x1100..0x1107/0x1200..0x1207.
 * Preserve alpha.2/alpha.3 CLRN model; legacy private card files 0x4C00..0x4C03
 * remain untouched and require host export/import to the official format. */
#define CL_MODEL_FILE 0x4C10
#define CL_MODEL_KEY 1
#endif
