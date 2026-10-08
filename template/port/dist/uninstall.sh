#!/bin/sh
# uninstall.sh - removes what {{SLUG}} copied and wrote (Linux and macOS;
# in the Mac package it is uninstall.command).  Started without anything
# it asks: first whether to remove the copied game files (the data
# folder's "game", and a "game" left beside the program by an old
# version), then whether to remove the saves and settings too (the whole
# data folder).  Anything but y or yes keeps them.  Without a terminal to
# ask on it only lists what it would ask about.  For scripts: --yes
# answers the first question with yes and the second with no, --yes --all
# both with yes.  The program's own folder is not touched: it was
# unpacked by hand and is deleted by hand.
set -eu
SLUG="{{SLUG}}"
NAME="{{NAME}}"
yes=0 all=0
for a in "$@"; do
    case $a in
        --yes) yes=1 ;;
        --all) all=1 ;;
        *) echo "usage: $0 [--yes [--all]]" >&2; exit 2 ;;
    esac
done
if [ $all = 1 ] && [ $yes = 0 ]; then
    echo "usage: $0 [--yes [--all]]" >&2; exit 2
fi
# the data folder as the program finds it (the kit's sys_data_dir)
if [ -n "${DK_DATA_DIR:-}" ]; then data=$DK_DATA_DIR
elif [ "$(uname)" = Darwin ]; then data="${HOME:?}/Library/Application Support/$NAME"
elif [ -n "${XDG_DATA_HOME:-}" ]; then data="$XDG_DATA_HOME/$SLUG"
else data="${HOME:?}/.local/share/$SLUG"
fi
# never a folder that is not the program's own
case $data in
    ""|/|"${HOME:-/}"|"${HOME:-/}/") echo "$0: the data folder is \"$data\": nothing done" >&2; exit 1 ;;
esac
here=$(cd "$(dirname "$0")" && pwd)
size() { du -sh "$1" 2>/dev/null | cut -f1 | tr -d " "; }
# ask QUESTION ANSWER-WITH---yes (0 yes, 1 no): 0 for yes
ask() {
    if [ $yes = 1 ]; then return "$2"; fi
    if [ ! -t 0 ]; then echo "$1 [y/N] (no terminal: not asked, kept)"; return 1; fi
    printf '%s [y/N] ' "$1"
    read -r answer || return 1
    case $answer in y|Y|yes|Yes|YES) return 0 ;; *) return 1 ;; esac
}
removed=0
for g in "$data/game" "$here/game"; do
    [ -d "$g" ] || continue
    if ask "Remove the copied game files in $g ($(size "$g"))?" 0; then
        rm -rf -- "$g"
        echo "removed $g"; removed=1
    else echo "kept $g"; fi
done
if [ -d "$data" ]; then
    if [ $all = 1 ]; then second=0; else second=1; fi
    if ask "Also remove the saves and settings in $data ($(size "$data"))?" $second; then
        rm -rf -- "$data"
        echo "removed $data"; removed=1
    else echo "kept $data"; fi
fi
[ $removed = 1 ] || echo "Nothing removed."
echo "The program's own folder is yours to delete: $here"
