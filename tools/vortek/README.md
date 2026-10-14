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
