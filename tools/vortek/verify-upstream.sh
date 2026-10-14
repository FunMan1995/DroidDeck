#!/usr/bin/env bash
# Re-fetches the pinned upstream commits and diffs them against the vendored tree.
# Pristine import: commit "vortek: vendor client and renderer sources" (no diff expected
# except upstream files intentionally left out: logo.png, build.sh, .gitignore, and the
# unused winlator/ sources). DroidDeck changes since then are exported to patches/.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; . "$here/PINS"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
git clone -q "$VORTEK_CLIENT_REPO" "$tmp/client" && git -C "$tmp/client" checkout -q "$VORTEK_CLIENT_COMMIT"
git clone -q --filter=blob:none --sparse "$VORTEK_SERVER_REPO" "$tmp/server"
git -C "$tmp/server" sparse-checkout set "$VORTEK_SERVER_SUBDIR" "$VORTEK_COMPAT_SUBDIR"
git -C "$tmp/server" checkout -q "$VORTEK_SERVER_COMMIT"
rc=0
diff -r -x .git -x logo.png -x build.sh -x .gitignore "$tmp/client" "$here/client" || rc=1
diff -r "$tmp/server/$VORTEK_SERVER_SUBDIR" "$here/server/vortekrenderer" || rc=1
for f in $(cd "$here/server/winlator" && find . -type f); do
  diff "$tmp/server/$VORTEK_COMPAT_SUBDIR/$f" "$here/server/winlator/$f" || rc=1
done
exit $rc
