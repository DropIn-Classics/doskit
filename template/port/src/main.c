/* main.c - {{NAME}}: a native compatibility implementation requiring an
 * installed copy of the original game.
 *
 *     {{SLUG}} [-game DIR | -gog FILE|FOLDER|SETUP.exe]
 *
 * DIR is the game's unpacked files: -game, else ${{ENV}}, else the first
 * folder `game` holding {{MARKER}} beside the program, in the current
 * directory or in the data folder (sys_find_game).  When there is none,
 * the installed GOG release's image is unpacked into the data folder's
 * `game`, or, installed as a folder, that folder copied there (cdimage.h);
 * not installed, GOG's Windows installer (setup_*.exe) lying about is
 * unpacked instead (inno.h).  -gog names the image, the folder or the
 * installer instead of looking for it.  Found by itself, the release is
 * copied only when the player agrees (Y or Enter; N or Esc not), asked in
 * the window; the headless build asks only in a run scripted with keys
 * (DK_KEYS, plat_null.c) and copies without asking otherwise.
 *
 * A release build (PORT_VERSION and PORT_UPDATE_URL defined) asks once
 * whether it may look for newer releases and shows one it found
 * (update.h, docs/RELEASE.md point 7).
 *
 * Nothing is ported yet: the program shows where it found the game and
 * waits for Esc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cdimage.h"
#include "inno.h"
#include "platform.h"
#include "sys.h"
#include "textmode.h"
#include "update.h"

#ifndef PORT_VERSION
#define PORT_VERSION ""
#endif
#ifndef PORT_UPDATE_URL
#define PORT_UPDATE_URL ""
#endif

static const GogRelease release = {
    /* GOG's folder name */
    "{{GOG_FOLDER}}",
    /* the CD image's path in that folder ("": game.gog) */
    "{{GOG_IMAGE}}",
    /* the Mac release's image, its path in /Applications ("": none) */
    "{{MAC_BUNDLE}}",
    /* GOG's product ID, the number in goggame-ID.info ("": not known) */
    "{{GOG_ID}}",
    /* on the CD: images of other games are passed over */
    "{{MARKER}}",
};

/* what earlier versions wrote beside the program (sys_data_migrate) */
static const char *const old_files[] = { "game", NULL };

static uint8_t pixels[TM_WIDTH * TM_HEIGHT];
static uint32_t palette[256];

/* the lines about newer releases, from row y: the question, once, or
 * the setting and a release found; key the scancode read (-1 none) */
static void updates(int y, int key)
{
    const uint8_t attr = TM_ATTR(TM_LIGHTGREY, TM_BLUE), hi = TM_ATTR(TM_YELLOW, TM_BLUE);
    char line[80];
    UpdateInfo u;
    int consent = update_consent();

    if (!*PORT_VERSION || !*PORT_UPDATE_URL)
        return;
    if (consent < 0) {
        if (key == 0x15 || key == 0x31)                 /* Y, N */
            update_set_consent(key == 0x15);
        tm_text(3, y, "Look for new versions of this port on GitHub, once a day?  Y / N", hi);
        return;
    }
    if (key == 0x3C)                                    /* F2 */
        update_set_consent(!consent);
    update_start(PORT_VERSION, PORT_UPDATE_URL);
    snprintf(line, sizeof line, "F2: look for new versions: %s", update_consent() ? "on" : "off");
    tm_text(3, y, line, attr);
    if (update_poll(&u)) {
        if (key == 0x16)                                /* U */
            update_open(u.page);
        snprintf(line, sizeof line, "%s is out (this is %s).  U opens its page.", u.version,
                 PORT_VERSION);
        tm_text(3, y + 1, line, hi);
        snprintf(line, sizeof line, "%.74s", u.notes);
        line[strcspn(line, "\n")] = 0;                /* the notes' first line */
        tm_text(3, y + 2, line, attr);
    }
}

static void show(void)
{
    tm_render(pixels, palette);
    plat_present(pixels, TM_WIDTH, TM_HEIGHT, palette);
}

/* 1 if the player declined the copy */
static int declined;

/* asks whether `what` may be made from `from` into `to`; 1 yes.  Not
 * asked (1) when -gog named the release, nor headless unless keys are
 * scripted. */
static int offer(const char *what, const char *from, const char *to, int named)
{
    const uint8_t attr = TM_ATTR(TM_LIGHTGREY, TM_BLUE), hi = TM_ATTR(TM_YELLOW, TM_BLUE);

    if (named || (!plat_has_window() && !getenv("DK_KEYS")))
        return 1;
    tm_clear(' ', attr);
    tm_text(2, 2, "{{NAME}} was found (the GOG release):", TM_ATTR(TM_WHITE, TM_BLUE));
    tm_text(2, 3, from, hi);
    tm_text(2, 5, what, TM_ATTR(TM_WHITE, TM_BLUE));
    tm_text(2, 6, to, hi);
    tm_text(2, 8, "Do that now?  Y / N", hi);
    while (plat_pump()) {
        int b;

        while ((b = plat_read_scancode()) >= 0) {
            if (b == 0x15 || b == 0x1C)                 /* Y, Enter */
                return 1;
            if (b == 0x31 || b == 0x01)                 /* N, Esc */
                return 0;
        }
        show();
        plat_sleep_ms(15);
    }
    return 0;
}

/* the game's files: found, or from the GOG release (its CD image
 * unpacked, its installed folder copied, or its Windows installer
 * unpacked); 1 if there */
static int get_game(const char *given, const char *gog, char *out, size_t n)
{
    char from[SYS_PATH], data[SYS_PATH], err[256];
    int folder, setup, r;

    if (sys_find_game(given, "{{ENV}}", "{{MARKER}}", out, n))
        return 1;
    if (given)
        return 0;
    if (gog)
        snprintf(from, sizeof from, "%s", gog);
    else if (!gog_find(&release, from, sizeof from) &&
             !gog_find_folder(&release, from, sizeof from) &&
             !inno_find(&release, from, sizeof from))
        return 0;
    folder = sys_is_dir(from);
    setup = !folder && inno_is_setup(from);
    sys_data_dir(data, sizeof data);
    sys_join(out, n, data, "game");
    if (!offer(folder ? "Its game files can be copied into the data folder's"
                      : "Its game files can be unpacked into the data folder's",
               from, out, gog != NULL)) {
        declined = 1;
        return 0;
    }
    tm_clear(' ', TM_ATTR(TM_LIGHTGREY, TM_BLUE));
    tm_text(2, 2, folder ? "Copying the game's files from" : "Unpacking the game's files from",
            TM_ATTR(TM_WHITE, TM_BLUE));
    tm_text(2, 3, from, TM_ATTR(TM_YELLOW, TM_BLUE));
    show();
    if (folder)
        r = gog_copy(from, out, "{{MARKER}}", NULL, NULL, err, sizeof err);
    else if (setup)
        r = inno_unpack(from, out, "{{MARKER}}", NULL, NULL, err, sizeof err);
    else
        r = cd_unpack(from, out, "{{MARKER}}", NULL, NULL, err, sizeof err);
    if (r != 0) {
        plat_message(err);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    const char *given = NULL, *gog = NULL;
    char game[SYS_PATH];
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-game") && i + 1 < argc)
            given = argv[++i];
        else if (!strcmp(argv[i], "-gog") && i + 1 < argc)
            gog = argv[++i];
        else {
            fprintf(stderr, "usage: {{SLUG}} [-game DIR | -gog FILE|FOLDER]\n");
            return 2;
        }
    }
    sys_set_app("{{NAME}}", "{{SLUG}}");
    sys_data_migrate(old_files);
    if (!plat_init("{{NAME}}"))
        return 1;
    if (!get_game(given, gog, game, sizeof game)) {
        if (declined)
            plat_message("The game's files were not copied, so nothing can be played. "
                         "Start {{SLUG}} again to be asked again, or use -game with "
                         "the folder of the game's files.");
        else
            plat_message("The game's files were not found. This program needs an installed "
                         "copy of {{NAME}} (the GOG release), or -game with its folder.");
        plat_shutdown();
        return 1;
    }
    tm_clear(' ', TM_ATTR(TM_LIGHTGREY, TM_BLUE));
    tm_frame(1, 1, 78, 5, TM_ATTR(TM_WHITE, TM_BLUE));
    tm_text(3, 2, "{{NAME}}", TM_ATTR(TM_YELLOW, TM_BLUE));
    tm_text(3, 3, "The game's files:", TM_ATTR(TM_LIGHTGREY, TM_BLUE));
    tm_text(3, 4, game, TM_ATTR(TM_WHITE, TM_BLUE));
    tm_text(3, 8, "Nothing is ported yet.  Esc ends the program.", TM_ATTR(TM_LIGHTGREY, TM_BLUE));
    while (plat_pump()) {
        int b, key = -1;
        while ((b = plat_read_scancode()) >= 0)
            if (!(b & 0x80))
                key = b;
        if (key == 0x01)
            break;
        tm_fill(0, 10, TM_WIDTH, 3, ' ', TM_ATTR(TM_LIGHTGREY, TM_BLUE));
        updates(10, key);
        show();
        plat_sleep_ms(15);
    }
    plat_shutdown();
    return 0;
}
