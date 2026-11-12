#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 1 ]]; then
    echo "Usage: $0 EYE_SIZE_REBUILD_OUTPUT_DIR" >&2
    exit 2
fi
project=$(cd "$(dirname "$0")" && pwd)
out=$(cd "$1" && pwd)
cxx=${HOST_CXX:-c++}
# Compile the runtime's own eye_size parser and size selection, not a copy of them.
awk '
    /^static .* (parseEyeSize|recommendedEyeSize|vulkanPreviewSize)\(/ { copy = 1 }
    copy { print }
    /^}/ { copy = 0 }
' "$out/src/runtime.cpp" > "$out/eye-size-fixture.inc"
"$cxx" -std=c++17 -I"$out" "$project/fixtures/eye-size.cpp" -o "$out/eye-size"
"$out/eye-size"
