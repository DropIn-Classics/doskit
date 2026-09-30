/* main.c - {{NAME}}: a native compatibility implementation requiring an
 * installed copy of the original game.
 *
 *     {{SLUG}} [-game DIR | -gog FILE|FOLDER]
 *
 * DIR is the game's unpacked files: -game, else ${{ENV}}, else the first
 * folder `game` holding {{MARKER}} beside the program, in the current
 * directory or in the data folder (sys_find_game).  When there is none,
 * the installed GOG release's image is unpacked into the data folder's
 * `game`, or, installed as a folder, that folder copied there (cdimage.h;
 * -gog names the image or the folder instead of looking for it).
 *
 * Nothing is ported yet: the program shows where it found the game and
 * waits for Esc.
 */
#include <stdio.h>
#include <string.h>
#include "cdimage.h"
#include "platform.h"
#include "sys.h"
#include "textmode.h"

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

static uint8_t pixels[TM_WIDTH * TM_HEIGHT];
static uint32_t palette[256];

static void show(void)
{
    tm_render(pixels, palette);
    plat_present(pixels, TM_WIDTH, TM_HEIGHT, palette);
}

/* the game's files: found, or from the GOG release (its CD image
 * unpacked, or its installed folder copied); 1 if there */
static int get_game(const char *given, const char *gog, char *out, size_t n)
{
    char from[SYS_PATH], data[SYS_PATH], err[256];
    int folder, r;

    if (sys_find_game(given, "{{ENV}}", "{{MARKER}}", out, n))
        return 1;
    if (given)
        return 0;
    if (gog)
        snprintf(from, sizeof from, "%s", gog);
    else if (!gog_find(&release, from, sizeof from) &&
             !gog_find_folder(&release, from, sizeof from))
        return 0;
    folder = sys_is_dir(from);
    sys_data_dir(data, sizeof data);
    sys_join(out, n, data, "game");
    tm_clear(' ', TM_ATTR(TM_LIGHTGREY, TM_BLUE));
    tm_text(2, 2, folder ? "Copying the game's files from" : "Unpacking the game's files from",
            TM_ATTR(TM_WHITE, TM_BLUE));
    tm_text(2, 3, from, TM_ATTR(TM_YELLOW, TM_BLUE));
    show();
    if (folder)
        r = gog_copy(from, out, "{{MARKER}}", NULL, NULL, err, sizeof err);
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
    if (!plat_init("{{NAME}}"))
        return 1;
    if (!get_game(given, gog, game, sizeof game)) {
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
        int b, esc = 0;
        while ((b = plat_read_scancode()) >= 0)
            if (b == 0x01)
                esc = 1;
        if (esc)
            break;
        show();
        plat_sleep_ms(15);
    }
    plat_shutdown();
    return 0;
}
