#!/usr/bin/env bash
# Builds the x86_64 host harness (brief 003 §4.2) from a Vortek source tree (default: this one):
#   $out/vortek-host-server, $out/libvulkan_vortek.so, $out/vortek_icd.x86_64.json, $out/feature_test
# usage: build.sh [out_dir] [vortek_tree]   (x86_64 Linux, gcc, host libvulkan.so.1; no device)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="$(mkdir -p "${1:-$here/out}" && cd "${1:-$here/out}" && pwd)"
v="$(cd "${2:-$here/..}" && pwd)"
[[ "$(uname -m)" == x86_64 ]] || { echo "host harness is x86_64 only" >&2; exit 1; }
jdk=${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac || command -v java)")")")}
glslc=${GLSLC:-$(ls "${ANDROID_HOME:-$HOME/Android/Sdk}"/ndk/*/shader-tools/linux-x86_64/glslc 2>/dev/null | tail -1)}

# Khronos headers, the same set build-client.sh assembles.
hdrs="$out/include"; rm -rf "$hdrs"; mkdir -p "$hdrs"
cp -r "$v/server/vortekrenderer/include/vulkan" "$v/server/vortekrenderer/include/vk_video" "$hdrs/"
cp "$v"/client-droiddeck/khronos/vulkan/*.h "$hdrs/vulkan/"
x11="$here/../client-droiddeck/x11"

# Server: the renderer sources plus host stubs, VORTEK_HOST_TEST (fake JNI, no AHardwareBuffer).
srv=("$v"/server/vortekrenderer/src/{main,vk_object,request_handler,vulkan_helper,vk_context,resource_memory,texture_decoder,shader_inspector,async_pipeline_creator,timeline_semaphore}.c
     "$v"/server/winlator/src/{arrays,ring_buffer,sysvshared_memory}.c)
[[ -f "$v/server/droiddeck/droiddeck_shims.c" ]] && srv+=("$v/server/droiddeck/droiddeck_shims.c")
defs=(-DVORTEK_HOST_TEST -DVK_USE_PLATFORM_ANDROID_KHR -D__ANDROID_API__=26 ${HOST_TEST_DEFS:-})
if [[ -f "$v/server/droiddeck/shm_swapchain.c" ]]; then srv+=("$v/server/droiddeck/shm_swapchain.c"); defs+=(-DDD_PRESENT_SHM)
else srv+=("$v/server/droiddeck/xwindow_swapchain_stub.c"); fi
gcc -std=gnu11 -O1 -g -w -Wno-error=incompatible-pointer-types -Wno-error=int-conversion "${defs[@]}" -I"$here/include" -I"$v/server/droiddeck" -I"$v/server/vortekrenderer/include" \
    -I"$v/server/winlator/include" -I"$jdk/include" -I"$jdk/include/linux" \
    "${srv[@]}" "$here/host_server.c" -o "$out/vortek-host-server" -ldl -lpthread -lm

# Client: the guest ICD built for x86_64.
cli=(src/main.c src/vulkan_calls.c src/vk_object.c src/vk_object_pool.c src/arrays.c src/descriptor_update_template.c src/ring_buffer.c)
for f in src/droiddeck_shims.c src/dd_present.c; do [[ -f "$v/client/$f" ]] && cli+=("$f"); done
(cd "$v/client" && gcc -shared -fPIC -O1 -g -w -DVK_USE_PLATFORM_XLIB_KHR -Iinclude -idirafter "$hdrs" -idirafter "$x11" \
    "${cli[@]}" -o "$out/libvulkan_vortek.so" -lpthread)
printf '{"file_format_version":"1.0.0","ICD":{"api_version":"1.3.0","library_path":"%s/libvulkan_vortek.so"}}\n' "$out" > "$out/vortek_icd.x86_64.json"

# Feature test (§3 C tests) with SPIR-V compiled from tri.vert/tri.frag by the NDK's glslc.
for s in vert frag; do
    { printf 'static const uint32_t %s_spv[] = {\n' "$s"; "$glslc" -mfmt=num -fshader-stage=$s "$here/tri.$s" -o -; printf '};\n'; } > "$out/tri.$s.inc"
done
gcc -O1 -g -I"$out" -I"$hdrs" "$here/feature_test.c" -o "$out/feature_test" -l:libvulkan.so.1
echo "host harness built in $out (sources: $v)"
