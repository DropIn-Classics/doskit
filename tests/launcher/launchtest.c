/* launchtest.c - the runtime's launcher.c and frame.c's keymap
 * (selftest step 4), headless on plat_null.c with keys by picture:
 * from the menu a page opened, a choice stepped, a key item given Right
 * Ctrl, Esc back to the menu, a second page opened and left, an action
 * picked; Esc in the menu quits; the settings saved and read back; then frame_wait
 * handing keys through a keymap (one key to another, an E0 key to a
 * plain one, a key dropped).  Prints "launcher ok", exit status 0, or
 * what went wrong.
 *
 *     launchtest FILE      (FILE: where the settings are written) */
#define _POSIX_C_SOURCE 200809L         /* setenv */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame.h"
#include "launcher.h"
#include "platform.h"
#include "vga.h"

static int vol = 1, key = 0x2A, other;
static const char *const levels[] = { "off", "low", "middle", "high", NULL };
static const char *const yesno[] = { "no", "yes", NULL };

static LauncherItem menu[] = {
    { LI_PAGE, "Sound", NULL, NULL, NULL, 1, NULL },
    { LI_PAGE, "More", NULL, NULL, NULL, 2, NULL },
    { LI_ACTION, "Start", NULL, NULL, NULL, 7, NULL },
};
static LauncherItem first[] = {
    { LI_CHOICE, "Volume", "volume", levels, &vol, 0, "how loud" },
    { LI_KEY, "Left flipper", "leftflipper", NULL, &key, 0, NULL },
};
static LauncherItem second[] = {
    { LI_HEAD, "Others", NULL, NULL, NULL, 0, NULL },
    { LI_CHOICE, "Other", "other", yesno, &other, 0, NULL },
};
static LauncherPage pages[] = {
    { "Test", menu, 3 },
    { "Sound", first, 2 },
    { "More", second, 2 },
};

static unsigned char got[16];
static int ngot;

static void keyboard(unsigned char b)
{
    if (ngot < (int)sizeof got)
        got[ngot++] = b;
}

static int fail(const char *what)
{
    printf("%s (volume %d, key %02X, other %d)\n", what, vol, key, other);
    return 1;
}

int main(int argc, char **argv)
{
    static const char keys[] =
        "1:1C 3:E0-4D 5:E0-50 7:1C 9:E0-1D 11:01 13:E0-50 15:1C 17:E0-4D 19:01 21:E0-50 23:1C "
        "30:01 "
        "40:E0-1D 41:E0-9D 42:39 43:B9 44:1E 45:9E";
    static unsigned char map[256];
    static const unsigned char want[] = { 0x2A, 0xAA, 0x1D, 0x9D };
    static const LauncherApp app = { "Test", "test", "v1.0" };
    int r, i;

    if (argc != 2)
        return fail("no file given");
#ifdef _WIN32
    _putenv_s("DK_KEYS", keys);
#else
    setenv("DK_KEYS", keys, 1);
#endif
    plat_init("launchtest");
    r = launcher_run(&app, NULL, pages, 3, NULL);
    if (r != 7 || vol != 2 || key != 0x9D || other != 1)
        return fail("launcher_run");
    if (launcher_run(&app, NULL, pages, 3, NULL) != LAUNCHER_QUIT)
        return fail("Esc in the menu did not quit");
    if (strcmp(launcher_key_name(0x9D), "Right Ctrl") || strcmp(launcher_key_name(0), "none"))
        return fail("launcher_key_name");
    if (launcher_save(argv[1], "test", pages, 3) != 0)
        return fail("launcher_save");
    vol = 0, key = 0, other = 0;
    if (launcher_load(argv[1], pages, 3) != 0 || vol != 2 || key != 0x9D || other != 1)
        return fail("launcher_load");

    /* the keymap: Right Ctrl to Left Shift, Space to Left Ctrl, A none */
    for (i = 0; i < 256; i++)
        map[i] = (unsigned char)i;
    map[0x9D] = 0x2A;
    map[0x39] = 0x1D;
    map[0x1E] = 0;
    frame_set_keymap(map);
    frame_set_keyboard(keyboard);
    vga_set_mode(0x13);
    while (frame_count() < 30)
        frame_wait();
    if (ngot != (int)sizeof want || memcmp(got, want, sizeof want)) {
        printf("keymap: %d bytes:", ngot);
        for (i = 0; i < ngot; i++)
            printf(" %02X", got[i]);
        printf("\n");
        return 1;
    }
    printf("launcher ok\n");
    return 0;
}
