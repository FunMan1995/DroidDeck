#!/usr/bin/env bash
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if tree_status=$(git -C "${repo_root}" status --porcelain 2>/dev/null); then
    if [[ -n "${tree_status}" ]]; then
        export DROIDDECK_BUILD_TREE_STATE=dirty
    else
        export DROIDDECK_BUILD_TREE_STATE=clean
    fi
else
    export DROIDDECK_BUILD_TREE_STATE=unknown
fi
sdk_dir=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-"${HOME}/Library/Android/sdk"}}
java_dir=${JAVA_HOME:-"/opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home"}
image_name=${DROIDDECK_BUILD_IMAGE:-droiddeck-local-cross:24.04-v3}
build_variant=${DROIDDECK_BUILD_VARIANT:-release}
case "$build_variant" in
    debug) gradle_task=assembleDebug ;;
    release) gradle_task=assembleRelease ;;
    *) echo "DROIDDECK_BUILD_VARIANT must be debug or release" >&2; exit 1 ;;
esac

if [[ ! -x "${sdk_dir}/platform-tools/adb" ]]; then
    echo "Android SDK not found at ${sdk_dir}; set ANDROID_HOME or ANDROID_SDK_ROOT." >&2
    exit 1
fi
if [[ ! -x "${java_dir}/bin/java" ]]; then
    echo "Java 17 not found at ${java_dir}; set JAVA_HOME." >&2
    exit 1
fi
if ! command -v docker >/dev/null 2>&1; then
    echo "Docker is required to cross-compile the glibc ARM64 preload libraries." >&2
    exit 1
fi
for tool in curl tar zstd shasum unzip python3; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "${tool} is required to build the CI-equivalent APK." >&2
        exit 1
    fi
done
if [[ -f "${repo_root}/tools/gamescope/release.env" \
        || -f "${repo_root}/tools/droiddeck-esync/release.env" ]] && ! command -v gh >/dev/null 2>&1; then
    echo "GitHub CLI is required to download the pinned Gamescope and droiddeck-esync release assets." >&2
    exit 1
fi

# Pinned NDK; auto-detecting the newest entry under ndk/ can pick a non-NDK directory.
ndk_version=${DROIDDECK_NDK_VERSION:-27.3.13750724}
if [[ -z "${ndk_version}" || ! -d "${sdk_dir}/ndk/${ndk_version}" ]]; then
    echo "No Android NDK found under ${sdk_dir}/ndk; set DROIDDECK_NDK_VERSION." >&2
    exit 1
fi
export ANDROID_HOME="${sdk_dir}"
export ANDROID_SDK_ROOT="${sdk_dir}"
export JAVA_HOME="${java_dir}"
# apksigner and the other build-tools wrappers need java on PATH; JAVA_HOME alone is not enough.
export PATH="${JAVA_HOME}/bin:${PATH}"
export NDK="${sdk_dir}/ndk/${ndk_version}"

staging_dir=$(mktemp -d "${TMPDIR:-/tmp}/droiddeck-build.XXXXXX")
bundle_asset="${repo_root}/app/src/main/assets/pulseaudio.tzst"
linuxfs_dir="${repo_root}/app/src/main/assets/linuxfs"
cleanup() {
    local exit_code=$?
    trap - EXIT
    rm -rf -- "${staging_dir}" || exit_code=1
    exit "${exit_code}"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

python3 "${repo_root}/tools/docker_preflight.py" "${image_name}"

# Debian bullseye image pinned to the point release that matches the apt snapshot used below
# (snapshot.debian.org 20250601T000000Z); the floating tag ships newer libc6 than the snapshot has.
debian_snapshot_image=public.ecr.aws/docker/library/debian:bullseye-20250428@sha256:2a7f95bcf104c8410bf4d3b13c52f6e0e4334bb2edf8d80c7f9881e49447effe

# Ownership guard: a run killed during a root container step (kill skips the EXIT trap) leaves
# root-owned files that make the next rm -rf / Gradle step fail. Hand them back to this user first.
foreign=$(find "${repo_root}" -xdev \( -name .git -o -name .gradle \) -prune -o ! -uid "$(id -u)" -print -quit 2>/dev/null || true)
if [[ -n "${foreign}" ]]; then
    echo "build_local: files not owned by $(id -un) (first: ${foreign#"${repo_root}/"}); restoring ownership via Docker" >&2
    docker run --rm --platform linux/amd64 -v "${repo_root}:/src" "${debian_snapshot_image}" \
        find /src -xdev \( -name .git -o -name .gradle \) -prune -o ! -uid "$(id -u)" -exec chown -h "$(id -u):$(id -g)" {} +
    left=$(find "${repo_root}" -xdev \( -name .git -o -name .gradle \) -prune -o ! -uid "$(id -u)" -print -quit 2>/dev/null || true)
    if [[ -n "${left}" ]]; then
        echo "build_local: still not owned by $(id -un): ${left}" >&2
        exit 1
    fi
fi
rm -rf -- "${linuxfs_dir}"
mkdir -p "${linuxfs_dir}"

docker run --rm --platform linux/amd64 \
    --user "$(id -u):$(id -g)" \
    -v "${repo_root}:/src" -w /src "${image_name}" bash -lc '
        set -euo pipefail
        d=app/src/main/assets/linuxfs
        mkdir -p "$d"
        put() { mkdir -p "$(dirname "$2")" && cat "$1" > "$2"; }
        aarch64-linux-gnu-g++ -shared -fPIC -O2 -Wall -Wno-attributes -Wno-nonnull-compare \
            -pthread -std=c++17 -static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL \
            -o "$d/libfakeinput.so" app/src/main/cpp/fakeinput_steam.cpp -ldl
        aarch64-linux-gnu-strip --strip-unneeded "$d/libfakeinput.so"
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -pthread \
            -o "$d/libblsession.so" tools/linuxfs/preload/*.c -ldl
        aarch64-linux-gnu-strip --strip-unneeded "$d/libblsession.so"
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -pthread \
            -o "$d/libblfastpath.so" tools/proot/fastpath/fastpath.c -ldl
        aarch64-linux-gnu-strip --strip-unneeded "$d/libblfastpath.so"
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -nostdlib -ffreestanding -fno-stack-protector \
          -fno-tree-loop-distribute-patterns -Wl,-z,defs -o "$d/libblaudit.so" tools/proot/fastpath/audit.c
        aarch64-linux-gnu-strip --strip-unneeded "$d/libblaudit.so"
        aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -pthread \
            -o "$d/libssbs.so" tools/linuxfs/ssbs/ssbs_adapter.c -ldl
        aarch64-linux-gnu-strip --strip-unneeded "$d/libssbs.so"
        mkdir -p "$d/usr/local/bin"
        aarch64-linux-gnu-gcc -O2 -Wall -Wextra -o "$d/usr/local/bin/droiddeck-clipboard" tools/linuxfs/clipboard/clipboard.c -ldl
        aarch64-linux-gnu-strip --strip-unneeded "$d/usr/local/bin/droiddeck-clipboard"
        for script in tools/linuxfs/overlay/usr/local/bin/droiddeck-* tools/linuxfs/overlay/usr/local/bin/steam-compatibility; do
            put "$script" "$d/usr/local/bin/$(basename "$script")"
        done
        for f in tools/linuxfs/overlay/usr/bin/* tools/linuxfs/overlay/usr/bin/steamos-polkit-helpers/*; do
            [ -f "$f" ] && put "$f" "$d/${f#tools/linuxfs/overlay/}"
        done
        put tools/linuxfs/desktop/droiddeck-desktop "$d/usr/local/bin/droiddeck-desktop"
        put tools/linuxfs/desktop/droiddeck-gpu "$d/usr/local/bin/droiddeck-gpu"
        put tools/linuxfs/desktop/droiddeck-desktop-gpu "$d/usr/local/bin/droiddeck-desktop-gpu"
        put tools/linuxfs/desktop/kwin_wayland_wrapper "$d/usr/local/bin/kwin_wayland_wrapper"
        put tools/linuxfs/desktop/firefox-droiddeck.js \
            "$d/usr/lib/firefox/defaults/pref/droiddeck.js"

        READELF=aarch64-linux-gnu-readelf bash tools/linuxfs/check-preloads.sh "$d"
    '

docker run --rm --platform linux/amd64 \
    -v "${repo_root}:/src" -v /etc/ssl/certs:/etc/ssl/certs:ro -w /src "${debian_snapshot_image}" bash -c '
        set -euo pipefail
        # Pinned Debian snapshot (the old Debian archive mirror 404ed bullseye packages, gate d9613e1e).
        printf "deb https://snapshot.debian.org/archive/debian/20250601T000000Z bullseye main\n" > /etc/apt/sources.list
        rm -f /etc/apt/sources.list.d/*
        printf "Acquire::Check-Valid-Until \"false\";\n" > /etc/apt/apt.conf.d/99snapshot
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends gcc g++ gcc-multilib g++-multilib binutils >/dev/null
        tools/linuxfs/build-x86-preloads.sh app/src/main/assets/linuxfs
        chown -R '"$(id -u):$(id -g)"' app/src/main/assets/linuxfs
    '

DROIDDECK_BUILD_IMAGE="${image_name}" "${repo_root}/tools/vr/build.sh" "${linuxfs_dir}/usr/local/lib/droiddeck-vr"

github_repo=${DROIDDECK_GITHUB_REPOSITORY:-}
if [[ -z "${github_repo}" ]]; then
    origin_url=$(git -C "${repo_root}" remote get-url origin)
    case "${origin_url}" in
        https://github.com/*) github_repo=${origin_url#https://github.com/} ;;
        ssh://git@github.com/*) github_repo=${origin_url#ssh://git@github.com/} ;;
        git@github.com:*) github_repo=${origin_url#git@github.com:} ;;
        *)
            echo "Cannot determine the GitHub repository from origin: ${origin_url}" >&2
            echo "Set DROIDDECK_GITHUB_REPOSITORY=owner/repo." >&2
            exit 1
            ;;
    esac
    github_repo=${github_repo%.git}
fi

cache_dir=${DROIDDECK_BUILD_CACHE:-"${HOME}/.cache/droiddeck-build"}
mkdir -p "${cache_dir}"
cached() {
    local sha=$1 name=$2 out
    shift 2
    out="${cache_dir}/${sha}-${name}"
    if [[ -f "${out}" ]] && printf '%s  %s\n' "${sha}" "${out}" | shasum -a 256 -c - >/dev/null 2>&1; then
        echo "${out}"
        return 0
    fi
    rm -f "${out}.part"
    out="${out}.part" "$@" >&2
    printf '%s  %s\n' "${sha}" "${out}.part" | shasum -a 256 -c - >&2
    mv "${out}.part" "${out}"
    echo "${out}"
}

if [[ -f "${repo_root}/tools/gamescope/release.env" ]]; then
    . "${repo_root}/tools/gamescope/release.env"
    gamescope_archive=$(cached "${GAMESCOPE_SHA256}" gamescope.tzst \
        bash -c 'gh release download "$0" -R "$1" -p gamescope.tzst -O "$out"' "${GAMESCOPE_TAG}" "${GAMESCOPE_REPO:-${github_repo}}")
    zstd -dc "${gamescope_archive}" | tar -xf - -C "${linuxfs_dir}"
    test -f "${linuxfs_dir}/usr/local/bin/gamescope"
fi

. "${repo_root}/tools/linuxfs/uruntime.env"
uruntime_binary=$(cached "${URUNTIME_SHA256}" "${URUNTIME_ASSET}" \
    bash -c 'curl -fsSL --retry 3 -o "$out" "$0"' "https://github.com/VHSgunzo/uruntime/releases/download/${URUNTIME_VERSION}/${URUNTIME_ASSET}")
mkdir -p "${linuxfs_dir}/usr/local/lib/droiddeck" "${linuxfs_dir}/usr/local/share/licenses/uruntime"
install -m644 "${uruntime_binary}" "${linuxfs_dir}/usr/local/lib/droiddeck/uruntime"
install -m644 "${repo_root}/tools/linuxfs/licenses/uruntime-LICENSE" "${linuxfs_dir}/usr/local/share/licenses/uruntime/LICENSE"

sync_assets="${repo_root}/app/src/main/assets/droiddeck-esync"
if [[ -f "${repo_root}/tools/droiddeck-esync/release.env" ]]; then
    . "${repo_root}/tools/droiddeck-esync/release.env"
    sync_archive=$(cached "${SYNC_BUNDLE_SHA256}" "${SYNC_BUNDLE_ASSET}" \
        bash -c 'gh release download "$0" -R "$2" -p "$1" -O "$out"' "${SYNC_BUNDLE_TAG}" "${SYNC_BUNDLE_ASSET}" "${SYNC_BUNDLE_REPO}")
    rm -rf "${sync_assets}"
    mkdir -p "${sync_assets}"
    zstd -dc "${sync_archive}" | tar -xf - -C "${sync_assets}"
    test -f "${sync_assets}/index.json"
    test -f "${sync_assets}/index.json.sig"
    sync_index=$(mktemp)
    gh release download "${SYNC_BUNDLE_TAG}" -R "${SYNC_BUNDLE_REPO}" -p index.json -O "${sync_index}" --clobber
    revoked=$(python3 -c 'import json, sys; print(" ".join(p["id"] for p in json.load(open(sys.argv[1]))["packs"] if p.get("revoked") is True))' "${sync_index}")
    for id in ${revoked}; do
        if [[ -e "${sync_assets}/packs/${id}.tzst" ]]; then
            echo "${SYNC_BUNDLE_ASSET} carries revoked pack ${id}; it is left out of the APK" >&2
            rm -f "${sync_assets}/packs/${id}.tzst"
        fi
    done
    rm -f "${sync_index}"
elif [[ -d "${sync_assets}" ]]; then
    echo "No tools/droiddeck-esync/release.env: the APK bundles the droiddeck-esync packs already in ${sync_assets}." >&2
fi

msi_pkgs="${staging_dir}/msi-pkgs"
msi_dir="${linuxfs_dir}/usr/local/lib/droiddeck-msitools"
mkdir -p "${msi_pkgs}" "${msi_dir}"
while read -r package_sha256 package_url; do
    [[ -n "${package_url}" ]] || continue
    package_archive=$(cached "${package_sha256}" "$(basename "${package_url}")" \
        bash -c 'curl -fsSL --retry 3 -o "$out" "$0"' "${package_url}")
    tar -xJf "${package_archive}" -C "${msi_pkgs}"
done < <(grep -v '^#' "${repo_root}/tools/msitools/packages.txt")
for tool in msiinfo cabextract; do install -m644 "${msi_pkgs}/usr/bin/${tool}" "${msi_dir}/${tool}"; done
for tool in 7z 7z.so; do install -m644 "${msi_pkgs}/usr/lib/7zip/${tool}" "${msi_dir}/${tool}"; done
for library in libmsi-1.0.so.0 libgsf-1.so.114 libgcab-1.0.so.0; do
    cp -L "${msi_pkgs}/usr/lib/${library}" "${msi_dir}/${library}"
done
mkdir -p "${linuxfs_dir}/usr/local/share/licenses/droiddeck-msitools"
for license in NOTICE GPL-2 GPL-3 LGPL-2.1 7zip-License 7zip-unRarLicense; do
    install -m644 "${repo_root}/tools/msitools/licenses/${license}" \
        "${linuxfs_dir}/usr/local/share/licenses/droiddeck-msitools/${license}"
done

mango_dir="${linuxfs_dir}/usr/local/lib/mangoapp"
mango_pkgs="${staging_dir}/mango-pkgs"
mkdir -p "${mango_dir}" "${mango_pkgs}"
while read -r package_sha256 package_url; do
    [[ -n "${package_url}" ]] || continue
    package_archive=$(cached "${package_sha256}" "$(basename "${package_url}")" \
        bash -c 'curl -fsSL --retry 3 -o "$out" "$0"' "${package_url}")
    zstd -dc "${package_archive}" | tar -xf - -C "${mango_pkgs}"
done < <(grep -v '^#' "${repo_root}/tools/mangoapp/packages.txt")
install -m644 "${mango_pkgs}/usr/bin/mangoapp" "${mango_dir}/mangoapp"
for library in libfmt.so.10 libspdlog.so.1.13 libglfw.so.3 libtraceevent.so.1; do
    cp -L "${mango_pkgs}/usr/lib/${library}" "${mango_dir}/${library}"
done
docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" -v "${repo_root}:/src" -w /src "${image_name}" \
    aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wall -Wl,-soname,libtracefs.so.1 \
    -o app/src/main/assets/linuxfs/usr/local/lib/mangoapp/libtracefs.so.1 tools/mangoapp/libtracefs-shim.c
mkdir -p "${linuxfs_dir}/usr/local/bin"
install -m644 "${repo_root}/tools/mangoapp/mangoapp" "${linuxfs_dir}/usr/local/bin/mangoapp"

pa_source=${DROIDDECK_PA13_SOURCE_DIR:-"${staging_dir}/pulseaudio-13.0"}
if [[ -z "${DROIDDECK_PA13_SOURCE_DIR:-}" ]]; then
    # Official release tarball, the same file and sha256 as THIRD_PARTY_NOTICES.md.
    pa_tarball=$(cached 961b23ca1acfd28f2bc87414c27bb40e12436efcf2158d29721b1e89f3f28057 pulseaudio-13.0.tar.xz \
        bash -c 'curl -fsSL --retry 3 -o "$out" "$0"' https://freedesktop.org/software/pulseaudio/releases/pulseaudio-13.0.tar.xz)
    mkdir -p "${pa_source}"
    tar -xJf "${pa_tarball}" -C "${pa_source}" --strip-components=1
fi
if [[ ! -f "${pa_source}/src/pulse/version.h.in" ]]; then
    echo "PulseAudio 13.0 source not found at ${pa_source}; set DROIDDECK_PA13_SOURCE_DIR." >&2
    exit 1
fi

sink_output="${staging_dir}/sink-out"
"${repo_root}/tools/directaudio/fetch.sh" "${repo_root}" "${sink_output}"
"${repo_root}/tools/aaudio-sink/build.sh" "${pa_source}" "${sink_output}"
proot_out="${repo_root}/app/src/main/jniLibs/arm64-v8a"
proot_sources=$(cd "${repo_root}/tools/proot" && find . -type f ! -name '*.pyc' | LC_ALL=C sort | xargs shasum -a 256 | shasum -a 256 | cut -d' ' -f1)
proot_inputs=$(printf '%s\n' "${proot_sources}" "${ndk_version}" | shasum -a 256 | cut -d' ' -f1)
if [[ -f "${proot_out}/libproot.so" && -f "${proot_out}/libproot-loader.so" \
        && "$(cat "${proot_out}/.proot-inputs" 2>/dev/null)" = "${proot_inputs}" ]] \
        && (cd "${proot_out}" && shasum -a 256 -c .proot-outputs >/dev/null 2>&1); then
    echo "proot: sources unchanged, keeping ${proot_out}/libproot.so"
else
    "${repo_root}/tools/proot/build.sh" "${proot_out}"
    echo "${proot_inputs}" > "${proot_out}/.proot-inputs"
    (cd "${proot_out}" && shasum -a 256 libproot.so libproot-loader.so > .proot-outputs)
fi

bundle_dir="${staging_dir}/pulseaudio-bundle"
mkdir -p "${bundle_dir}"
zstd -dc "${bundle_asset}" | tar -xf - -C "${bundle_dir}"
if [[ -e "${bundle_dir}/modules/arm64/module-aaudio-sink.so" \
        || -e "${bundle_dir}/modules/arm64/module-directaudio-native-sink.so" ]]; then
    echo "The committed audio bundle already contains a built ARM64 sink." >&2
    exit 1
fi
install -m755 "${sink_output}/module-aaudio-sink.so" \
    "${bundle_dir}/modules/arm64/module-aaudio-sink.so"
install -m755 "${sink_output}/module-directaudio-native-sink.so" \
    "${bundle_dir}/modules/arm64/module-directaudio-native-sink.so"
tar -cf - -C "${bundle_dir}" . | zstd -19 -T0 -c > "${staging_dir}/pulseaudio.tzst"
mkdir -p "${repo_root}/app/build/prepared-assets"
mv "${staging_dir}/pulseaudio.tzst" "${repo_root}/app/build/prepared-assets/pulseaudio.tzst"

gradle_rust_arg=""
if [[ "${DROIDDECK_SKIP_RUST:-0}" == 1 ]]; then
    gradle_rust_arg="-PskipRust=true"
else
    export PATH="${HOME}/.cargo/bin:${PATH}"
    for tool in rustup cargo; do
        if ! command -v "${tool}" >/dev/null 2>&1; then
            echo "${tool} is required for the store engines (https://rustup.rs); set DROIDDECK_SKIP_RUST=1 to build without them." >&2
            exit 1
        fi
    done
    rustup target add --toolchain stable aarch64-linux-android
    if ! command -v cargo-ndk >/dev/null 2>&1; then
        cargo install cargo-ndk --locked
    fi
fi

cd "${repo_root}"
python3 tools/quest/build-driver.py "${NDK}"
bash tools/vortek/build-client.sh
bash tools/comet/build-local.sh
python3 tools/release/runtime_inputs.py record
# Kotlin compiles inside the Gradle JVM (one -Xmx cap); a separate Kotlin daemon OOMed at 1 GB in Lab.
./gradlew "${gradle_task}" --console=plain -Pkotlin.compiler.execution.strategy=in-process -PndkVersion="${ndk_version}" ${gradle_rust_arg}
python3 tools/release/check_session_assets.py "app/build/outputs/apk/${build_variant}/app-${build_variant}.apk"
python3 tools/release/runtime_inputs.py apk "app/build/outputs/apk/${build_variant}/app-${build_variant}.apk"

apk="${repo_root}/app/build/outputs/apk/${build_variant}/app-${build_variant}.apk"
audio_check="${staging_dir}/audio-check"
mkdir -p "${audio_check}"
unzip -p "${apk}" assets/pulseaudio.tzst | zstd -dc | tar -xf - -C "${audio_check}"
for audio_file in \
    pactl \
    modules/arm64/module-aaudio-sink.so \
    modules/arm64/module-directaudio-native-sink.so; do
    if [[ ! -f "${audio_check}/${audio_file}" ]]; then
        echo "APK audio bundle is missing ${audio_file}." >&2
        exit 1
    fi
done

readelf_path=$(find "${NDK}/toolchains/llvm/prebuilt" -name llvm-readelf | head -1)
[[ -x "${readelf_path}" ]] || { echo "NDK llvm-readelf is missing" >&2; exit 1; }
stores_check=()
[[ "${DROIDDECK_SKIP_RUST:-0}" != 1 ]] || stores_check+=(--without-stores)
python3 tools/release/check_android_libraries.py "${apk}" --readelf "${readelf_path}" "${stores_check[@]}"
git diff --exit-code -- app/src/main/rust/stores/Cargo.lock

# Prefer the pinned build-tools (37.0.0) for zipalign/apksigner; apksigner is not under $JAVA_HOME/bin.
build_tools_version=${BUILD_TOOLS_VERSION:-37.0.0}
build_tools="${sdk_dir}/build-tools/${build_tools_version}"
if [[ ! -x "${build_tools}/apksigner" ]]; then
    build_tools=$(find "${sdk_dir}/build-tools" -mindepth 1 -maxdepth 1 -type d -print | sort -V | tail -1)
fi
export PATH="${build_tools}:${PATH}"
echo "build-tools for zipalign/apksigner: ${build_tools}"
if [[ ! -x "${build_tools}/zipalign" || ! -x "${build_tools}/apksigner" ]]; then
    echo "Android build-tools with zipalign/apksigner are required under ${sdk_dir}/build-tools." >&2
    exit 1
fi

"${build_tools}/zipalign" -p -f 4 "${apk}" "${apk}.aligned"
"${build_tools}/apksigner" sign \
    --ks "${repo_root}/keystore/testkey.p12" --ks-type PKCS12 --ks-pass pass:android \
    --ks-key-alias testkey --key-pass pass:android \
    --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true \
    --out "${apk}" "${apk}.aligned"
rm -f "${apk}.aligned" "${apk}.idsig"

signature_output=$("${build_tools}/apksigner" verify --min-sdk-version 21 --verbose --print-certs "${apk}")
printf '%s\n' "${signature_output}"
if ! unzip -l "${apk}" | grep -E 'META-INF/.*\.(SF|RSA|DSA)$' >/dev/null; then
    echo "APK signature check failed: JAR signature files are missing." >&2
    exit 1
fi
for scheme in \
    'Verified using v1 scheme (JAR signing): true' \
    'Verified using v2 scheme (APK Signature Scheme v2): true' \
    'Verified using v3 scheme (APK Signature Scheme v3): true'; do
    grep -qF "${scheme}" <<<"${signature_output}" || {
        echo "APK signature check failed: ${scheme}" >&2
        exit 1
    }
done
grep -qi 'CN=Android, OU=Android, O=Android' <<<"${signature_output}" || {
    echo "APK signature check failed: the signer is not the AOSP testkey." >&2
    exit 1
}

signing_env=${DROIDDECK_SIGNING_ENV:-}
if [[ -z "${signing_env}" ]]; then
    for candidate in "${repo_root}/.signing.env" \
            "$(dirname "$(git -C "${repo_root}" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || echo .)")/.signing.env"; do
        [[ -f "${candidate}" ]] && { signing_env=${candidate}; break; }
    done
fi
if [[ -n "${signing_env}" ]]; then
    echo "Signing with DroidDeck's key (${signing_env})"
    (
        set -a
        cd "$(dirname "${signing_env}")"
        . "${signing_env}"
        set +a
        BUILD_TOOLS="${build_tools}" "${repo_root}/tools/release/sign-apk.sh" "${apk}" standard "${apk}.release"
    )
    mv "${apk}.release" "${apk}"
fi

printf 'APK: %s\n' "${apk}"
printf 'SHA-256: '
shasum -a 256 "${apk}" | awk '{print $1}'
