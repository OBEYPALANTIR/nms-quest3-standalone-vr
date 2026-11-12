#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 && $# != 3 ]]; then
    echo "Usage: $0 INPUT_ROOT NEW_OUTPUT_DIR [0319|eye-size|text-priority-eye-size|text-priority-input]" >&2
    exit 2
fi
# Default: 0001-0003. 0319: 0001 alone, the deployed .0319 build. eye-size: 0001 and 0004 on that base.
# text-priority-eye-size: 0001-0003 and 0005 (0004 rebased), with the default profile's epoch and image base.
# text-priority-input: text-priority-eye-size plus 0006 (conf key text_priority_input), same epoch and image base.
case ${3:-} in
    "") patches=(0001-stereo-ipd 0002-tracking-reference-spaces 0003-text-priority-right-poses)
        manifest=patched.sha256 epoch=1794195961 base=0x23a620000 dll=openxr_wxr_64.right-pose-diag.dll
        expected=0723529c639aea56025a0c32c53c1d54901bd3bd5a8ee76cd725233931917693 ;;
    0319) patches=(0001-stereo-ipd)
        manifest=patched.0319.sha256 epoch=1794125998 base=0x3998f0000 dll=openxr_wxr_64.0319.dll
        expected=e521bb64a65d50bb9a5a7583bf51a504ccad3bc21c946a4650c3be46cc109021 ;;
    eye-size) patches=(0001-stereo-ipd 0004-eye-size)
        manifest=patched.eye-size.sha256 epoch=1794280941 base=0x3998f0000 dll=openxr_wxr_64.eyesize.dll
        expected=07d2f082da45845be64edb5e843c27d1669cf38dd810a1aee332f49485cc3083 ;;
    text-priority-eye-size) patches=(0001-stereo-ipd 0002-tracking-reference-spaces 0003-text-priority-right-poses 0005-eye-size-on-text-priority)
        manifest=patched.text-priority-eye-size.sha256 epoch=1794195961 base=0x23a620000 dll=openxr_wxr_64.textprio-eyesize.dll
        expected=9decc18e4cdf52e3089f9731af66fbff88c3db4edab157894e70cb959a528d46 ;;
    text-priority-input) patches=(0001-stereo-ipd 0002-tracking-reference-spaces 0003-text-priority-right-poses 0005-eye-size-on-text-priority 0006-text-priority-input)
        manifest=patched.text-priority-input.sha256 epoch=1794195961 base=0x23a620000 dll=openxr_wxr_64.textprio-input.dll
        expected=cb299dbf848c64e29b051e8faf072340987fb44387b07bb47b6b4e0f2b239f65 ;;
    *) echo "Unknown profile $3" >&2; exit 2 ;;
esac
project=$(cd "$(dirname "$0")" && pwd)
input=$(cd "$1" && pwd)
out=$2
cxx=${CXX:-x86_64-w64-mingw32-g++}
cxx=$(command -v "$cxx")
if [[ $("$cxx" -dumpmachine) != x86_64-w64-mingw32 || $("$cxx" -dumpfullversion) != 16.2.0 ]]; then
    echo "Expected x86_64-w64-mingw32 GCC 16.2.0" >&2
    exit 2
fi
"$cxx" --version
if command -v sha256sum >/dev/null; then
    sha=(sha256sum)
else
    sha=(shasum -a 256)
fi
(cd "$input" && grep -v ' libvulkan-1\.a$' "$project/inputs.sha256" | "${sha[@]}" -c -)
# libvulkan-1.a: the recorded library (inputs.sha256) or the deterministic one fetch-inputs.sh
# generates from vulkan-1.def; both link the same pinned DLL.
lib=$("${sha[@]}" "$input/libvulkan-1.a")
lib=${lib%% *}
if ! grep -q "^$lib  libvulkan-1\.a$" "$project/inputs.sha256" \
    && [[ $lib != 4a60d8da513d1b1d296cc51bbcb0fe96634eb178a04ee8552d765f10a69ccf8f ]]; then
    echo "libvulkan-1.a: unpinned SHA-256 $lib" >&2
    exit 1
fi

# Require a fresh output directory and compile from relative source paths.
mkdir "$out"
cd "$out"
mkdir src
for file in runtime.cpp WinXrApiUDP.cpp WinXrApiUDP.h wxr_bridge.h; do
    cp "$input/src/$file" "src/$file"
done
for patch in "${patches[@]}"; do
    git apply --whitespace=error "$project/$patch.patch"
done
"${sha[@]}" -c "$project/$manifest"

export SOURCE_DATE_EPOCH=$epoch
for source in runtime WinXrApiUDP; do
    "$cxx" -std=c++17 -O2 -fpermissive \
        -I"$input/original-include" -I"$input/OpenXR-SDK/include" \
        -I"$input/OpenXR-SDK/include/openxr" -I"$input/Vulkan-Headers/include" \
        -c "src/$source.cpp" -o "$source.o"
done
"$cxx" -std=c++17 -O2 -s -fpermissive -shared -static-libgcc -static-libstdc++ \
    runtime.o WinXrApiUDP.o -o "$dll" \
    -Wl,--image-base,$base -Wl,--output-def,openxr_wxr.def \
    -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic \
    -lvulkan-1 -lws2_32 -ld3d11 -ld3d12 -ldxgi -ld3dcompiler \
    -lopengl32 -lgdi32 -luser32 -L"$input"
printf '%s  %s\n' "$expected" "$dll" | "${sha[@]}" -c -
