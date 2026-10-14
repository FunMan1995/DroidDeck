#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
image=${DROIDDECK_BUILD_IMAGE:-droiddeck-local-cross:24.04-v3}
docker run --rm --platform linux/amd64 -v "$repo:/src" \
  -v "${HOME}/.cache/droiddeck-comet-rust:/rust" -w /src \
  -e CARGO_HOME=/rust/cargo -e RUSTUP_HOME=/rust/rustup \
  -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" "$image" bash -lc '
    set -euo pipefail
    # Root is needed for apt-get; hand everything written to the bind mounts back to the host user.
    trap "chown -R \"\$HOST_UID:\$HOST_GID\" /src/app/src/main/assets/linuxfs/usr/local/lib/droiddeck-comet /src/app/build/comet /rust 2>/dev/null || true" EXIT
    apt-get update -qq
    apt-get install -y -qq curl git unzip ca-certificates pkg-config >/dev/null
    (cd / && git config --global --add safe.directory /src/app/build/comet/source)
    export PATH=/rust/cargo/bin:$PATH
    if ! command -v cargo >/dev/null; then
      curl -fsSL https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain 1.90.0
    fi
    export RUSTUP_TOOLCHAIN=1.90.0
    rustup target add aarch64-unknown-linux-gnu
    bash tools/comet/build.sh
  '
