#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
flags=(-std=c11 -Wall -Wextra -Werror -pedantic -g -O1)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie); fi
"${CC:-gcc}" "${flags[@]}" -Ifirmware/application/app/selection tests/test_selection.c firmware/application/app/selection/selection_core.c -o build/tests/selection
build/tests/selection
"${CC:-gcc}" "${flags[@]}" -Wno-misleading-indentation -Ifirmware/application/app/utils -Ifirmware/application/app/rfid tests/test_dataframe.c firmware/application/app/utils/dataframe.c firmware/application/app/rfid/hex_utils.c -o build/tests/dataframe
build/tests/dataframe
"${CC:-gcc}" "${flags[@]}" -Wno-pedantic -Itests/stubs -Ifirmware/application/app/utils tests/test_fds.c firmware/application/app/utils/fds_util.c -o build/tests/fds
build/tests/fds
"${CC:-gcc}" "${flags[@]}" -Itests/stubs -Ifirmware/application/app/selection -Ifirmware/application/app/utils -Ifirmware/application/app tests/test_selection_runtime.c firmware/application/app/selection/selection.c firmware/application/app/selection/selection_core.c -o build/tests/selection_runtime
build/tests/selection_runtime
"${CC:-gcc}" "${flags[@]}" -fshort-enums -Wno-pedantic -Wno-unused-variable -include tests/stubs/app_error.h -Ifirmware/application/app/rfid/nfctag -Itests/stubs -Ifirmware/application/app/rfid/nfctag/hf -Ifirmware/application/app/rfid/nfctag/lf -Ifirmware/application/app/rfid -Ifirmware/application/app/selection -Ifirmware/application/app/utils tests/test_tags.c firmware/application/app/rfid/nfctag/tag_emulation.c firmware/application/app/rfid/nfctag/tag_persistence.c firmware/application/app/rfid/crc_utils.c -o build/tests/tags
build/tests/tags
python3 -m unittest discover -s tests -p 'test_*.py' -v
