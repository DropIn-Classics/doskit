/* dialogtest.c - launcher.c's dialog about the game's files (selftest
 * step 4), headless on plat_null.c with keys by picture: the copy
 * offered and taken (Enter), declined by "Quit" (Down, Enter) and by
 * Esc; the screens "not found" and "could not be copied" left by Enter;
 * the bar while copying drawn.  Prints "dialog ok", exit status 0, or
 * what went wrong.
 *
 *     dialogtest [SHOTS]   (SHOTS: DK_SHOTS' value, to look at the screens) */
#define _POSIX_C_SOURCE 200809L         /* setenv */
#include <stdio.h>
#include <stdlib.h>
#include "launcher.h"
#include "platform.h"

static void env(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static int fail(const char *what)
{
    printf("%s\n", what);
    return 1;
}

int main(int argc, char **argv)
{
    /* a screen answered at picture n takes the pictures up to n + 1 */
    static const char keys[] = "2:1C 5:E0-50 7:1C 10:01 13:1C 16:1C";
    static const LauncherApp app = { "A Game With A Name", "agame", "v1.2" };
    static const char from[] = "/home/player/GOG Games/A Game With A Name/data/game.gog";
    static const char to[] =
        "/home/player/a/very/long/path/that/does/not/fit/the/window/.local/share/agame/game";
    LauncherCopy copy = { &app, LAUNCHER_CD, 0, 0 };

    env("DK_KEYS", keys);
    if (argc > 1)
        env("DK_SHOTS", argv[1]);
    plat_init("dialogtest");
    if (launcher_offer_copy(&app, LAUNCHER_GAME, from, to) != 1)
        return fail("Enter did not take the copy");
    if (launcher_offer_copy(&app, LAUNCHER_GAME_AND_CD, from, to) != 0)
        return fail("Down, Enter did not quit");
    if (launcher_offer_copy(&app, LAUNCHER_CD, from, to) != 0)
        return fail("Esc did not decline");
    launcher_no_game(&app, NULL);
    launcher_copy_failed(&app, from, "cannot write GAME/GAME.EXE");
    if (launcher_copy_progress(&copy, "MUSIC/Track02.ogg", 5, 10) != 0 || copy.closed)
        return fail("the bar stopped the copy");
    printf("dialog ok\n");
    return 0;
}
