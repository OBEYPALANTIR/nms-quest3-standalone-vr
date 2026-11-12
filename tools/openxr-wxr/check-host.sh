#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 1 ]]; then
    echo "Usage: $0 REBUILD_OUTPUT_DIR" >&2
    exit 2
fi
project=$(cd "$(dirname "$0")" && pwd)
out=$(cd "$1" && pwd)
cxx=${HOST_CXX:-c++}
# Compile the runtime's actual binding ingestion and controller resolver with
# the host fixture's minimal XR types, without building the Windows renderer.
awk '
    /^struct (ActionBinding|DiagnosticRightPoseSpace) \{/ ||
    /^static .* (AddController|ResolveDiagnosticRightPoseRole|xrSuggestInteractionProfileBindings_runtime)\(/ {
        copy = 1
        if ($0 ~ /^static .*xrSuggestInteractionProfileBindings_runtime/) print "} // namespace rt"
    }
    copy || /^static .* g_(actionPaths|actionBindings|controllerGrips|diagnosticRightPoseSpaces|controllerPoses|controllerSpaces|interactionProfiles|profileOrder|activeProfile|diagnosticProfileAttached|controllerMutex|pathStrings|actionNames)( |;)/ { print }
    /^}/ { copy = 0 }
' "$out/src/runtime.cpp" > "$out/runtime-binding-fixture.inc"
# The text_priority_input checks need 0006, which only the text-priority-input profile applies.
defines=()
if grep -q SetTextPriorityInput "$out/src/WinXrApiUDP.h"; then defines=(-DTEXT_PRIORITY_INPUT); fi
for fixture in test tracking-fix-probe; do
    "$cxx" -std=c++17 -pthread ${defines[@]+"${defines[@]}"} -I"$project/fixtures" -I"$out/src" -I"$out" \
        "$project/fixtures/$fixture.cpp" "$out/src/WinXrApiUDP.cpp" -o "$out/$fixture"
    "$out/$fixture"
done
