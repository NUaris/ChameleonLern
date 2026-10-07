#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
flags=(-std=c11 -Wall -Wextra -Werror -pedantic -g -O1)
if [[ "${SANITIZE:-0}" == 1 ]]; then
    flags+=("-fsanitize=${SANITIZERS:-address,undefined}" -fno-omit-frame-pointer)
    if [[ "$(uname -s)" == Linux ]]; then flags+=(-fno-pie -no-pie); fi
fi
src=firmware/application/src
"${CC:-gcc}" "${flags[@]}" -I"$src/selection" tests/test_selection.c "$src/selection/selection_core.c" -o build/tests/selection
build/tests/selection
"${CC:-gcc}" "${flags[@]}" -Itests/stubs -I"$src/selection" -I"$src/bsp" -I"$src" tests/test_selection_runtime.c "$src/selection/selection.c" "$src/selection/selection_core.c" -o build/tests/selection_runtime
build/tests/selection_runtime
"${CC:-gcc}" "${flags[@]}" -Itests/stubs -I"$src/selection" tests/test_learning_platform.c "$src/selection/learning_platform.c" -o build/tests/learning_platform
build/tests/learning_platform
python3 -m unittest discover -s tests -p 'test_*.py' -v
