/* innotest.c - runtime/inno.c on an installer, for the selftest.
 *
 *     innotest SETUP DIR [MUST_HAVE]
 *
 * Unpacks SETUP into DIR (inno_unpack) and says "inno_unpack ok, N files"
 * (N the progress calls), else "inno_unpack: " and the reason, exit 1.
 * With SETUP alone: "setup" or "not a setup" (inno_is_setup). */
#include <stdio.h>
#include "inno.h"

static int count(void *ctx, const char *file, long done, long total)
{
    (void)file;
    if (done > total)
        return 1;
    ++*(int *)ctx;
    return 0;
}

int main(int argc, char **argv)
{
    char err[256];
    int files = 0;

    if (argc == 2) {
        puts(inno_is_setup(argv[1]) ? "setup" : "not a setup");
        return 0;
    }
    if (argc < 3)
        return 2;
    if (inno_unpack(argv[1], argv[2], argc > 3 ? argv[3] : NULL, count, &files, err, sizeof err) != 0) {
        printf("inno_unpack: %s\n", err);
        return 1;
    }
    printf("inno_unpack ok, %d files\n", files);
    return 0;
}
