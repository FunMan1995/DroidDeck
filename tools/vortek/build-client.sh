#!/usr/bin/env bash
# Builds the Vortek guest client (glibc, aarch64) from tools/vortek/client and packs
# app/build/prepared-assets/graphics_driver/vortek-client.tzst with SOURCE.txt.
# Does not use upstream build.sh (it bakes the Winlator rootfs rpath) or vortek-2.1.tzst.
# Default: the droiddeck-local-cross image (same as build_local.sh). VORTEK_HOST_CC=1 uses a
# host aarch64-linux-gnu-gcc instead (X11 headers are only needed as headers).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; repo="$(cd "$here/../.." && pwd)"; . "$here/PINS"
image=${DROIDDECK_BUILD_IMAGE:-droiddeck-local-cross:24.04-v3}
out="$repo/app/build/vortek-client"; asset="$repo/app/build/prepared-assets/graphics_driver/vortek-client.tzst"
rm -rf "$out"; mkdir -p "$out/pkg" "$(dirname "$asset")"
# Khronos headers (VK_HEADER_VERSION 302): the renderer's copy plus the xlib/ICD/layer headers
# from KhronosGroup/Vulkan-Headers v1.3.302 (client-droiddeck/khronos), Apache-2.0.
hdrs="$out/include"; mkdir -p "$hdrs"
cp -r "$here/server/vortekrenderer/include/vulkan" "$here/server/vortekrenderer/include/vk_video" "$hdrs/"
cp "$here"/client-droiddeck/khronos/vulkan/*.h "$hdrs/vulkan/"
srcs="src/main.c src/vulkan_calls.c src/vk_object.c src/vk_object_pool.c src/arrays.c src/descriptor_update_template.c src/ring_buffer.c"
flags="-shared -fPIC -O2 -Wall -Wno-discarded-qualifiers -DVK_USE_PLATFORM_XLIB_KHR -Iinclude -idirafter $hdrs -Wl,-soname,libvulkan_vortek.so -lpthread"
if [[ "${VORTEK_HOST_CC:-0}" == 1 ]]; then
    (cd "$here/client" && aarch64-linux-gnu-gcc $flags -idirafter /usr/include $srcs -o "$out/pkg/libvulkan_vortek.so")
else
    docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" -v "$repo:/src" -w /src/tools/vortek/client "$image" \
        aarch64-linux-gnu-gcc ${flags//$repo//src} $srcs -o /src/app/build/vortek-client/pkg/libvulkan_vortek.so
fi
cp "$here/client-droiddeck/vortek_icd.aarch64.json" "$out/pkg/"
cp "$here/LICENSE" "$out/pkg/LICENSE"
shopt -s nullglob; series=("$here"/patches/*.patch); shopt -u nullglob
patches_sha=$(cat /dev/null "${series[@]}" | sha256sum | cut -d' ' -f1)
cat > "$out/pkg/SOURCE.txt" <<SRC
Vortek client (libvulkan_vortek.so), LGPL-2.1
Source: $VORTEK_CLIENT_REPO @ $VORTEK_CLIENT_COMMIT
Modified by DroidDeck: tools/vortek/patches (series sha256 $patches_sha)
Built by tools/vortek/build-client.sh; renderer: $VORTEK_SERVER_REPO @ $VORTEK_SERVER_COMMIT ($VORTEK_SERVER_SUBDIR)
SRC
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$out/pkg" -cf - . | zstd -q -19 -f -o "$asset"
echo "vortek client -> $asset"
