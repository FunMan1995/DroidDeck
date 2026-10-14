# Vortek forwarding driver (built from source)

Vortek forwards Vulkan calls from a glibc guest (client, `libvulkan_vortek.so`) over an
AF_UNIX socket plus shared-memory ring buffers to an Android-side renderer
(server, `libvortekrenderer.so`) that calls the system Vulkan driver.
DroidDeck uses it as an opt-in forwarder on Mali (brief 003). Prototype only.

- `client/`: brunodev85/vortek @ `b1730c5def9b575672e671aee11d79ae7adc63d1`
- `server/vortekrenderer/`: brunodev85/winlator-app @ `3981d86efa4f333b2a34a7da8b6521476cd8c8b9`, `app/src/main/cpp/vortekrenderer`
- `server/winlator/`: the subset of `app/src/main/cpp/winlator` (same commit) the renderer needs
- License: LGPL-2.1, `LICENSE` here and `licenses/LGPL-2.1.txt`. Upstream files carry no
  per-file headers; none were removed. DroidDeck changes are listed in `patches/`.
- The prebuilt `vortek-2.1.tzst` from winlator-app is NOT used.

`verify-upstream.sh` re-fetches the pins and diffs them against this tree.

## Build
- Renderer: `server/droiddeck/CMakeLists.txt`, pulled in by `app/src/main/cpp/CMakeLists.txt`
  (app `externalNativeBuild`, NDK, arm64-v8a). No adrenotools; system `libvulkan.so` via dlopen.
  `VORTEK_NO_XWINDOW` (default ON) replaces the Java-window swapchain; with `VORTEK_PRESENT_SHM` (default ON)
  that is presentation option (b), offscreen + memfd + client-side X11 SHM (brief 003b). Host harness:
  `host-test/build.sh` then `host-test/runs.sh a..f` (lavapipe + Pixel 9a profile, not komodo).
- Client: `build-client.sh` (called by `tools/build_local.sh`) cross-builds `libvulkan_vortek.so`
  for glibc aarch64 in `droiddeck-local-cross:24.04-v3` and packs
  `app/build/prepared-assets/graphics_driver/vortek-client.tzst` (lib, ICD json, LICENSE, SOURCE.txt).
  `VORTEK_HOST_CC=1` uses a host `aarch64-linux-gnu-gcc` instead.
- `export-patches.sh` regenerates `patches/` from git history after editing vendored files.
