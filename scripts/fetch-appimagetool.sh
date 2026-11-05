#!/usr/bin/env bash
# Verify the pinned release even when cached; install downloads only after checking them.
set -euo pipefail
OUT="$1"
VERSION=1.9.0
SHA=46fdd785094c7f6e545b61afcfb0f3d98d8eab243f644b4b17698c01d06083d1

if [[ -f "$OUT" ]]; then
    echo "$SHA  $OUT" | sha256sum -c -
else
    WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
    curl -fsSL -o "$WORK/appimagetool" "https://github.com/AppImage/appimagetool/releases/download/$VERSION/appimagetool-x86_64.AppImage"
    echo "$SHA  $WORK/appimagetool" | sha256sum -c -
    mkdir -p "$(dirname "$OUT")"
    install -m 755 "$WORK/appimagetool" "$OUT"
fi
chmod +x "$OUT"
