/* gogfind.c - the runtime's gog_find, run by selftest.py with HOME set to
 * a folder laid out as a player's installation:
 *
 *     gogfind FOLDER [MUST_HAVE]
 *
 * Prints the image found and exits 0, or prints "not found" and exits 1. */
#include <stdio.h>
#include "cdimage.h"

int main(int argc, char **argv)
{
    GogRelease rel = {0};
    char out[1024];

    if (argc < 2)
        return 2;
    rel.folder = argv[1];
    rel.must_have = argc > 2 ? argv[2] : NULL;
    if (!gog_find(&rel, out, sizeof out)) {
        puts("not found");
        return 1;
    }
    puts(out);
    return 0;
}
