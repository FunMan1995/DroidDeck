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

## gamescope
- Version: 3.16.29, with DroidDeck patches (see `tools/gamescope/PATCHES.md`)
- License: BSD-2-Clause, full text in `licenses/gamescope-BSD-2-Clause.txt`
- Source: https://github.com/ValveSoftware/gamescope/archive/refs/tags/3.16.29.tar.gz (tag `3.16.29`); patched build: https://github.com/Droid-Deck/DroidDeck-Components/releases/tag/gamescope-3.16.29-p10 (asset `gamescope.tzst`, sha256 `e8e2433a…`)
- Bundled files: `assets/linuxfs/usr/local/bin/gamescope`

## uruntime
- Version: v0.8.1 (asset `uruntime-appimage-lite-aarch64`, sha256 `8485ae20…`)
- License: MIT, full text in `licenses/uruntime-MIT.txt`
- Source: https://github.com/VHSgunzo/uruntime, tag `v0.8.1`
- Bundled files: `assets/linuxfs/usr/local/lib/droiddeck/uruntime`

## MangoHud (mangoapp) and its libraries
Taken unchanged from SteamOS aarch64 packages (`tools/mangoapp/packages.txt`); bundled under `assets/linuxfs/usr/local/lib/mangoapp/`.
- MangoHud: package `mangohud-20260902.1-1` (https://holo-packages.steamos.cloud/archlinux-deckard-hotfixes/release/0.4.x/mangohud-20260902.1-1-aarch64.pkg.tar.zst), license MIT, upstream https://github.com/flightlessmango/MangoHud (exact upstream tag/commit for this SteamOS build not yet pinned)
- fmt 10.2.0: license MIT, source https://github.com/fmtlib/fmt/archive/refs/tags/10.2.0.tar.gz
- spdlog 1.13.0: license MIT, source https://github.com/gabime/spdlog/archive/refs/tags/v1.13.0.tar.gz
- glfw 3.4: license Zlib, source https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
- libtraceevent 1.8.2: license LGPL-2.1-only (library; GPL-2.0-only for tools not shipped), full texts in `licenses/LGPL-2.1.txt` and `licenses/GPL-2.0.txt`, source https://git.kernel.org/pub/scm/libs/libtrace/libtraceevent.git/snapshot/libtraceevent-1.8.2.tar.gz

## PRoot and talloc
Built from source by `tools/proot/build.sh` with the patches in `tools/proot/patches`; bundled as `jniLibs/arm64-v8a/libproot.so` and `libproot-loader.so`.
- PRoot (Termux fork): commit `4dba3afbf3a63af89b4d9c1a59bf2bda10f4d10f`, license GPL-2.0-or-later, full text in `licenses/GPL-2.0.txt`, source https://github.com/termux/proot/archive/4dba3afbf3a63af89b4d9c1a59bf2bda10f4d10f.tar.gz (sha256 `de94f648…`)
- talloc 2.4.3: license LGPL-3.0-or-later, full text in `licenses/LGPL-3.0.txt`, source https://www.samba.org/ftp/talloc/talloc-2.4.3.tar.gz (sha256 `dc46c40b…`)

## msitools set and 7-Zip
Arch Linux ARM aarch64 builds, unchanged (`tools/msitools/packages.txt`, mirrored at https://github.com/Droid-Deck/DroidDeck/releases/tag/msitools-0.106-r1); bundled under `assets/linuxfs/usr/local/lib/droiddeck-msitools/`. PKGBUILD revisions are the Arch packaging tags (Arch Linux ARM builds these unmodified).
- msitools 0.106 (`msiinfo`, `libmsi-1.0.so.0`): GPL-2.0-or-later / LGPL-2.1-or-later; source https://download.gnome.org/sources/msitools/0.106/msitools-0.106.tar.xz; PKGBUILD `0.106-3` https://gitlab.archlinux.org/archlinux/packaging/packages/msitools/-/raw/0.106-3/PKGBUILD
- libgsf 1.14.60 (`libgsf-1.so.114`): LGPL-2.1-only; source https://download.gnome.org/sources/libgsf/1.14/libgsf-1.14.60.tar.xz; PKGBUILD `1.14.60-1` https://gitlab.archlinux.org/archlinux/packaging/packages/libgsf/-/raw/1.14.60-1/PKGBUILD
- gcab 1.6 (`libgcab-1.0.so.0`): LGPL-2.1-or-later; source https://download.gnome.org/sources/gcab/1.6/gcab-1.6.tar.xz; PKGBUILD `1.6-2` https://gitlab.archlinux.org/archlinux/packaging/packages/gcab/-/raw/1.6-2/PKGBUILD
- cabextract 1.11: GPL-3.0-or-later (per the tarball's COPYING); source https://www.cabextract.org.uk/cabextract-1.11.tar.gz; PKGBUILD `1.11-3` https://gitlab.archlinux.org/archlinux/packaging/packages/cabextract/-/raw/1.11-3/PKGBUILD
- 7-Zip 26.04 (`7z`, `7z.so`): LGPL-2.1-or-later AND LicenseRef-UnRAR AND BSD-3-Clause AND BSD-2-Clause, texts in `licenses/7zip-License.txt` and `licenses/7zip-unRarLicense.txt`; source https://www.7-zip.org/a/7z2604-src.tar.xz; PKGBUILD `26.04-1` https://gitlab.archlinux.org/archlinux/packaging/packages/7zip/-/raw/26.04-1/PKGBUILD
- License texts: `licenses/GPL-2.0.txt`, `licenses/GPL-3.0.txt`, `licenses/LGPL-2.1.txt`

## PulseAudio
- Version: 13.0, built from source by `tools/build_local.sh`
- License: LGPL-2.1-or-later, full text in `licenses/LGPL-2.1.txt`
- Source: https://freedesktop.org/software/pulseaudio/releases/pulseaudio-13.0.tar.xz (release tarball for tag `v13.0`, sha256 `961b23ca1acfd28f2bc87414c27bb40e12436efcf2158d29721b1e89f3f28057`); downloaded and sha256-checked by `tools/build_local.sh`
- Bundled files: `assets/pulseaudio.tzst`

## libsndfile and libltdl (PulseAudio dependencies)
Prebuilt Android arm64 libraries linked by the PulseAudio core; bundled as `jniLibs/arm64-v8a/libsndfile.so` and `jniLibs/arm64-v8a/libltdl.so`.
- libsndfile 1.0.31 (version from the binary's `sf_version_string`; sha256 of the bundled `.so` `ab999a10…`): license LGPL-2.1-or-later, full text in `licenses/LGPL-2.1.txt`; source https://github.com/libsndfile/libsndfile/releases/download/1.0.31/libsndfile-1.0.31.tar.bz2 (tag `1.0.31`)
- libltdl from GNU Libtool 2.4.6 (sha256 of the bundled `.so` `14d616028b38a4b778576477c675c1ce34618582b84ccc5c3fd2492e6518196f`): Copyright (C) 1998-2000, 2004-2008, 2011-2015 Free Software Foundation, Inc.; license LGPL-2.1-or-later, full text in `licenses/LGPL-2.1.txt`; unmodified, dynamically linked; source https://ftp.gnu.org/gnu/libtool/libtool-2.4.6.tar.gz (sha256 `e3bd4d5d3d025a36c21dd6af7ea818a2afcd4dfc1ea5a17b39d7854bcd0c06e3`); built for Android arm64 (NDK r27c, API 26) by https://github.com/The412Banner/Bannerlator `native/pulseaudio-android/build-stack.sh` @ `4239cd9799`
- libtracefs: not bundled. `assets/linuxfs/usr/local/lib/mangoapp/libtracefs.so.1` is a DroidDeck no-op shim built from `tools/mangoapp/libtracefs-shim.c` (GPL-3.0, part of DroidDeck), contains no libtracefs code and only satisfies mangoapp's link-time symbols

## Mesa Turnip Vulkan driver packs
Bundled under `assets/graphics_driver/`; each pack carries a `SOURCE.txt`. Drivers are unmodified Mesa Turnip (freedreno Vulkan) builds.
- License: MIT (Mesa, https://docs.mesa3d.org/license.html), full text in `licenses/Mesa-MIT.txt`. The Quest adapter `libquest_turnip.so` is DroidDeck code (`tools/quest`, GPL-3.0-or-later)
- `adrenotools-turnip-sdk36` (`vulkan.ad08XX.so`, sha256 `2552b159…`): Mesa 26.0.0-devel (`git-ff39aa7716`), a downstream commit that is not in upstream mesa/mesa; origin fork unknown, redistributed from Bannerlator: https://github.com/The412Banner/Bannerlator/blob/main/app/src/main/assets/graphics_driver/adrenotools-turnip-sdk36.tzst
- `adrenotools-turnip25.1.0` (`vulkan.ad07xx.so`, sha256 `af0d6bb3…`): Mesa 25.0.0-devel, upstream commit https://gitlab.freedesktop.org/mesa/mesa/-/commit/365cd04375baeb8c607c1a49e85e9a364e193a08 (the pack is named 25.1.0, the build reports 25.0.0-devel); obtained byte-identical from https://downloads.gamenative.app/graphics_driver/adrenotools-turnip25.1.0.tzst, original builder unknown
- `adrenotools-turnip25.1.0-quest1`: the same `vulkan.ad07xx.so` as `turnip25.1.0`, plus the Quest adapter; built by `tools/quest/build-driver.py`
- `adrenotools-turnip26.3.0-a6xx` and `adrenotools-turnip26.3.0-quest2` (`libvulkan_freedreno.so`): Banners-Turnip `v26.3.0-20261012-r2`, https://github.com/The412Banner/Banners-Turnip/releases/download/v26.3.0-20261012-r2/Turnip-v26.3.0-20261012-r2.zip (sha256 `bb1b2c51…`), Mesa commit https://gitlab.freedesktop.org/mesa/mesa/-/commit/a0ff50acf0e1d152f54949490fa41558d2e0f7a4; quest2 adds the Quest adapter; both built by `tools/quest/build-driver.py`
