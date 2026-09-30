#!/bin/sh
# Builds build/{{SLUG}} (the window, on SDL2) and build/{{SLUG}}-headless
# (for tests and scripted runs, doskit/runtime/plat_null.c) with cc
# (clang or gcc), on macOS and Linux; build.bat is the same for Windows.
# Without SDL2 (doskit/runtime/sdl2-flags.sh) only the headless one.
set -e
cd "$(dirname "$0")"
CC=${CC:-cc}
RT=../doskit/runtime
CFLAGS="-O2 -Wall -Wextra -I$RT -Isrc"
# the release's version: $PORT_VERSION, else the tag of the commit built;
# none, and PORT_VERSION stays undefined
VERSION=${PORT_VERSION:-$(git describe --tags --exact-match 2>/dev/null || true)}
if [ -n "$VERSION" ]; then
    CFLAGS="$CFLAGS -DPORT_VERSION=\"$VERSION\""
fi
# where a release looks for newer ones (the workflow sets it; update.h)
if [ -n "$PORT_UPDATE_URL" ]; then
    CFLAGS="$CFLAGS -DPORT_UPDATE_URL=\"$PORT_UPDATE_URL\""
fi
GAME="src/main.c"
RUNTIME="$RT/sys.c $RT/cdimage.c $RT/inno.c $RT/textmode.c $RT/pad.c $RT/sha256.c $RT/rmem.c $RT/vga.c $RT/frame.c $RT/modplay.c $RT/audiofx.c $RT/fli.c $RT/shot.c $RT/update.c"

mkdir -p build
$CC $CFLAGS -o build/{{SLUG}}-headless $GAME $RUNTIME $RT/plat_null.c -lm
if ! sdl=$(sh $RT/sdl2-flags.sh); then
    echo "SDL2 not found: built build/{{SLUG}}-headless only." >&2
    exit 0
fi
# $sdl unquoted: it is a list of options
$CC $CFLAGS -o build/{{SLUG}} $GAME $RUNTIME $RT/plat_sdl.c $sdl -lm
