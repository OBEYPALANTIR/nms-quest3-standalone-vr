#!/usr/bin/env bash
# Builds the INPUT_ROOT that rebuild.sh needs from upstream sources, then checks every
# file against inputs.sha256. Usage: fetch-inputs.sh NEW_INPUT_ROOT
#
# Sources (each *_URL / *_REF can be overridden, e.g. to point at a local mirror):
#   src/ (4 files)          WinlatorXR/OpenXR-WXR @ 5f7a5c455050e320578539f9009333a336599659
#                           CONFIRMED: the commit, src tree and blobs are recorded in README.md.
#   OpenXR-SDK/include      KhronosGroup/OpenXR-SDK tag release-1.1.43
#                           UNCONFIRMED URL/tag: inferred from XR_CURRENT_API_VERSION 1.1.43 in
#                           the recorded headers; no local record names the repo or tag.
#   original-include/       loader_interfaces.h and xr_linear.h, taken from src/common/ of the
#                           same OpenXR-SDK checkout. UNCONFIRMED path and revision. The pinned
#                           loader_interfaces.h hash equals include/openxr/openxr_loader_negotiation.h
#                           in the recorded headers (confirmed); the src/common/ path is not.
#   Vulkan-Headers/include  KhronosGroup/Vulkan-Headers tag v1.3.296
#                           UNCONFIRMED URL/tag: inferred from VK_HEADER_VERSION 296.
#   libvulkan-1.a           not downloaded: generated from vulkan-1.def with MinGW dlltool.
#                           Verified offline: it links the same pinned DLL as the recorded library.
# The sha256 check is what decides; a wrong unconfirmed tag or path fails it.
set -euo pipefail

if [[ $# != 1 ]]; then
    echo "Usage: $0 NEW_INPUT_ROOT" >&2
    exit 2
fi
: "${OPENXR_WXR_URL:=https://github.com/WinlatorXR/OpenXR-WXR}"
: "${OPENXR_WXR_REF:=5f7a5c455050e320578539f9009333a336599659}"
: "${OPENXR_SDK_URL:=https://github.com/KhronosGroup/OpenXR-SDK}"
: "${OPENXR_SDK_REF:=refs/tags/release-1.1.43}"
: "${VULKAN_HEADERS_URL:=https://github.com/KhronosGroup/Vulkan-Headers}"
: "${VULKAN_HEADERS_REF:=refs/tags/v1.3.296}"
: "${DLLTOOL:=x86_64-w64-mingw32-dlltool}"
# Output of the dlltool command below; deterministic for GNU binutils 2.47.
generated_lib=4a60d8da513d1b1d296cc51bbcb0fe96634eb178a04ee8552d765f10a69ccf8f

project=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$(dirname "$1")"
mkdir "$1"
root=$(cd "$1" && pwd)
work="$root/.checkouts"
mkdir "$work"

checkout() { # NAME URL REF
    git init -q "$work/$1"
    git -C "$work/$1" fetch -q --depth 1 "$2" "$3"
    git -C "$work/$1" checkout -q FETCH_HEAD
}
checkout wxr "$OPENXR_WXR_URL" "$OPENXR_WXR_REF"
checkout xr "$OPENXR_SDK_URL" "$OPENXR_SDK_REF"
checkout vk "$VULKAN_HEADERS_URL" "$VULKAN_HEADERS_REF"

# Copy exactly the files the manifest pins.
while read -r _ path; do
    case $path in
        src/*) from="$work/wxr/$path" ;;
        OpenXR-SDK/*) from="$work/xr/${path#OpenXR-SDK/}" ;;
        Vulkan-Headers/*) from="$work/vk/${path#Vulkan-Headers/}" ;;
        original-include/*) from="$work/xr/src/common/${path#original-include/}" ;;
        libvulkan-1.a) continue ;;
        *) echo "No source rule for $path" >&2; exit 1 ;;
    esac
    mkdir -p "$root/$(dirname "$path")"
    cp "$from" "$root/$path"
done < "$project/inputs.sha256"

(cd "$root" && "$DLLTOOL" --deterministic-libraries -d "$project/vulkan-1.def" \
    -l libvulkan-1.a -t _tmp_oxrwxr_build_libvulkan_1_a)

if command -v sha256sum >/dev/null; then
    sha=(sha256sum)
else
    sha=(shasum -a 256)
fi
(cd "$root" && grep -v ' libvulkan-1\.a$' "$project/inputs.sha256" | "${sha[@]}" -c --quiet -)
(cd "$root" && printf '%s  libvulkan-1.a\n' "$generated_lib" | "${sha[@]}" -c -)
rm -rf "$work"
echo "Input root ready: $root"
