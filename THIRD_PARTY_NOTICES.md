# Third-party notices

DroidDeck is licensed under GPL-3.0. It bundles the third-party components below.

## DirectAudio
- Credit: DirectAudio by The412Banner (https://github.com/The412Banner/directaudio)
- Copyright: The412Banner and DirectAudio contributors (see upstream COPYING/NOTICE)
- License: LGPL-2.1-or-later, full text in `licenses/LGPL-2.1.txt`
- Source: https://github.com/The412Banner/directaudio, tag `directaudio-linux-v1.1.0`
- Bundled files: `jniLibs/arm64-v8a/libdirectaudiorelay.so`, `assets/directaudio/{linux-wine11,linux-wine11-systhread}/winedirectaudio.{so,drv}`, `module-directaudio-native-sink.so` (linuxfs)

## droiddeck-esync packs
- Source and patches: https://github.com/Droid-Deck/DroidDeck-Components, tag `droiddeck-esync-index`, asset `bundle-20261004-142603.tzst` (patch series under `droiddeck-esync/patches`)
- License: the DroidDeck patches are GPL-3.0. The packaged `ntdll.so` and `wineserver` binaries are derived from Wine/Proton and are LGPL-2.1-or-later, full text in `licenses/LGPL-2.1.txt`
- Patch series (pinned): https://github.com/Droid-Deck/DroidDeck-Components/tree/780ab619dec8d30e74744babcca3ad14089e43f8/droiddeck-esync/patches (`valve-11` and `cachyos-11`, series sha256 `1c298e3f…`)
- Upstream Proton source for each bundled pack:
  - `cachyos-cachyos-11.0-20260703-slr-…-r1`: https://github.com/CachyOS/proton-cachyos tag `cachyos-11.0-20260703-slr`, commit `ea0053a9fbc074947d17028da0025834cc97f83a`, wine `b5f2dc7b5906ef864f83df8fef94c9f539eaad2d`
  - `ge-GE-Proton11-7-…-r1`: https://github.com/GloriousEggroll/proton-ge-custom tag `GE-Proton11-7`, commit `c191f35dcebbeccfacd3b4c6f6eea026e588c1c2`, wine `46b29104e3741fe23bf5e2547196a253aab88c89` (after GE's `patches/protonprep-valve-staging.sh`)
  - `valve-experimental-11.0-20261001-arm64-…-r1`: https://github.com/ValveSoftware/Proton tag `experimental-11.0-20261001`, commit `70b7e109e8fc0783a509805f3d1bc090b55dac3f`, wine `6d211aabd1d325c9990a5fd7a81b9db37caae12d`
  - `valve-proton-11.0-2c-arm64-…-r1`: https://github.com/ValveSoftware/Proton tag `proton-11.0-2c`, commit `5b89db940e0ebe3a137a6009a3589232fe084c09`, wine `dc26e61847081a1b5cb0733dc30feba6ee575482`
- Bundled files: `assets/droiddeck-esync/`

## comet
- Credit: comet by imLinguin (https://github.com/imLinguin/comet)
- License: Apache-2.0, comet's LICENSE is shipped alongside the bundled files (no NOTICE file upstream)
- Source: https://github.com/imLinguin/comet, tag `v0.3.2`, commit `3d7488ee72f83b7e8462a70fd4c4638d6da71360` (built from source with `tools/comet/droiddeck.patch` applied)
- Stubs: `GalaxyCommunication.exe` and `update-permissions.exe` are built from `dummy-service/communication.c` and `dummy-service/permissions.c` at the same commit (currently taken from the v0.3.2 release asset `dummy-service.zip`)
- Sub-component:
  - gog_protocols: https://github.com/Yepoleb/gog_protocols, commit `923e8a0008603365d29002db611559190da5bf99`, license CC0-1.0 (`.proto` files compiled into comet)
