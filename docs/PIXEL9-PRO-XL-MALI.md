# Pixel 9 Pro XL (Mali-G715) notes

This fork tracks [wallentx/DroidDeck](https://github.com/wallentx/DroidDeck) Preview builds for non-Adreno GPUs. Stock DroidDeck is Adreno-only; Pixel / Tensor Mali needs this Preview path.

## Device under test

- Google Pixel 9 Pro XL (`komodo`), Tensor G4, Mali-G715
- GrapheneOS build `2026091000` (Android 17)
- Working baseline: **Preview 12** (`0.6.9-wallentx.12`)

## Install

1. Install the APK from wallentx Releases: [Preview 12](https://github.com/wallentx/DroidDeck/releases/tag/main-c7d5e9ea1e2738c603aec29589bd92dfe4b228da) (`DroidDeck-0.6.9-wallentx.12.apk`).
2. Install the Linux runtime in-app.
3. In **Setup → Steam interface**, leave **GPU drawing** off (Software Steam UI). Mali Venus often loses `steamwebhelper` when GPU drawing is on (`dri3` missing).
4. Optional: import [PanVK-Mali-kbase-Pair.zip](https://github.com/The412Banner/droiddeck-nonadreno-drivers/releases/download/panvk-mali-kbase/PanVK-Mali-kbase-Pair.zip) under Setup → Drivers. Auto prefers `nad:panvk-mali-kbase`; without the pair it falls back to **Mali (Venus)** + System Vulkan.

## Fixes applied on-device (Nov 2026)

| Issue | Symptom | Fix |
|-------|---------|-----|
| Empty `/etc/machine-id` in linuxfs | dbus “Invalid machine ID”, Steam IPC stalls | Write a real 32-hex machine-id (+ `/var/lib/dbus/machine-id`) |
| Preview 11 | Missing fullscreen / component fixes | Upgrade to Preview 12 |
| Steam GPU drawing on Mali | `steamwebhelper` connects then disconnects; black UI | `steamGpuDrawing=false` / Software Steam UI |
| LMKD empty/cached kills | `[TOO MANY EMPTY PROCS]` in device report | Raise `max_phantom_processes` / `max_cached_processes` |

## Expected graphics path (without PanVK import)

```
route: Mali (Venus) → phone Mali HAL
display: System Vulkan
runtime ICD: Venus (virtio) over vtest
auto would pick: nad:panvk-mali-kbase (once the pair is imported)
```

Venus on Pixel G715 can reach Steam Big Picture and light titles; treat game support as experimental.

## Share logs

Setup → **Share logs** (or the session drawer). Attach the zip in Discord `#bug-reports` / wallentx threads, and say you are on Pixel 9 Pro XL + Preview 12 + Venus or PanVK.
