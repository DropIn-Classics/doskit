#!/bin/sh
# Prints the compiler and linker options for SDL2 (plat_sdl.c), or nothing
# and exit status 1 when SDL2 is not found.  Looked for as SDL2.framework
# in ~/Library/Frameworks or /Library/Frameworks on a Mac, else through
# sdl2-config or pkg-config.  With SDL2_STATIC=1 sdl2-config's static
# library is linked in (the macOS release: nothing beside the program).
# On Linux the program also looks for SDL2 in its own directory (the
# rpath), as the Linux release carries SDL2 there (docs/RELEASE.md).
#
#     sdl=$(sh runtime/sdl2-flags.sh) && cc ... runtime/plat_sdl.c $sdl
sdl=
if [ -n "$SDL2_STATIC" ]; then
    sdl="$(sdl2-config --cflags --static-libs)" || exit 1
    echo "$sdl"
    exit 0
fi
for fw in "$HOME/Library/Frameworks" /Library/Frameworks; do
    if [ "$(uname)" = Darwin ] && [ -d "$fw/SDL2.framework" ]; then
        sdl="-I$fw/SDL2.framework/Headers -F$fw -framework SDL2 -Wl,-rpath,$fw"
        break
    fi
done
if [ -z "$sdl" ] && command -v sdl2-config >/dev/null 2>&1; then
    sdl="$(sdl2-config --cflags --libs)"
elif [ -z "$sdl" ] && pkg-config --exists sdl2 2>/dev/null; then
    sdl="$(pkg-config --cflags --libs sdl2)"
fi
if [ -n "$sdl" ] && [ "$(uname)" != Darwin ]; then
    sdl="$sdl -Wl,-rpath,\$ORIGIN"
fi
[ -n "$sdl" ] || exit 1
echo "$sdl"
