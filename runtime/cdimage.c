/* cdimage.c - see cdimage.h */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cdimage.h"
#include "sys.h"

/* ---- finding the image */

static int has_file(const char *image, const char *must_have);

/* `path` if it is there and, with `must_have`, holds that file */
static int take(const char *path, const char *must_have, char *out, size_t n)
{
    if (!sys_is_file(path) || (must_have && !has_file(path, must_have)))
        return 0;
    snprintf(out, n, "%s", path);
    return 1;
}

/* dir/name, each part of name ("CD/GAME.INS") in any case: on Linux
 * GAME.GOG is not game.gog */
static int take_in(const char *dir, const char *name, const char *must_have, char *out, size_t n)
{
    char path[SYS_PATH], next[SYS_PATH], part[SYS_PATH];

    snprintf(path, sizeof path, "%s", dir);
    while (*name) {
        size_t len = strcspn(name, "/\\");

        snprintf(part, sizeof part, "%.*s", (int)len, name);
        if (!sys_find(path, part, next, sizeof next))
            return 0;
        memcpy(path, next, sizeof path);
        name += len;
        while (*name == '/' || *name == '\\')
            name++;
    }
    return take(path, must_have, out, n);
}

#ifndef _WIN32
/* the search through a machine's folders: fn(dir, ctx) for each folder
 * the game may be in; `dir` the folder being listed */
typedef struct {
    const GogRelease *rel;
    int (*fn)(const char *dir, void *ctx);
    void *ctx;
    const char *dir;
    int found;
} Walk;

/* An install folder: the game in it, or in its folder data (where GOG's
 * Linux installer put a DOSBox game's image, as seen with one release).
 * `any`: a folder that may be of another game (named in a list, found by
 * looking around), taken only if it is named as the release's folder or
 * the release names a must_have that tells its game from others. */
static int install(Walk *w, const char *dir, int any)
{
    char path[SYS_PATH], data[SYS_PATH];
    size_t len;

    snprintf(path, sizeof path, "%s", dir);
    for (len = strlen(path); len > 1 && path[len - 1] == '/'; len--)
        path[len - 1] = 0;
    if (any && !(w->rel->must_have && *w->rel->must_have)) {
        const char *base = strrchr(path, '/');
        if (strcmp(base ? base + 1 : path, w->rel->folder))
            return 0;
    }
    sys_join(data, sizeof data, path, "data");
    return w->fn(path, w->ctx) || w->fn(data, w->ctx);
}

/* the folders the Windows installer and GOG Galaxy choose, inside the
 * Wine prefix `prefix` (Wine, Lutris, Bottles) */
static int in_prefix(Walk *w, const char *prefix)
{
    static const char *const dirs[] = {
        "drive_c/GOG Games",
        "drive_c/Program Files (x86)/GOG Galaxy/Games",
        "drive_c/Program Files/GOG Galaxy/Games",
    };
    char dir[SYS_PATH], path[SYS_PATH];
    size_t i;

    for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        sys_join(dir, sizeof dir, prefix, dirs[i]);
        sys_join(path, sizeof path, dir, w->rel->folder);
        if (install(w, path, 0))
            return 1;
    }
    return 0;
}

/* each folder in a folder of games or of Wine prefixes: the game's own
 * folder (Lutris without Wine), a prefix, or one holding the game's
 * folder */
static void scan_entry(void *ctx, const char *name, int is_dir)
{
    Walk *w = (Walk *)ctx;
    char dir[SYS_PATH], sub[SYS_PATH];

    if (w->found || !is_dir)
        return;
    sys_join(dir, sizeof dir, w->dir, name);
    sys_join(sub, sizeof sub, dir, w->rel->folder);
    w->found = install(w, dir, 1) || in_prefix(w, dir) || install(w, sub, 0);
}

/* a text file, ending in a 0, NULL if it cannot be read */
static char *load_text(const char *path)
{
    size_t size;
    char *data = (char *)sys_load(path, &size), *text;

    if (!data || (text = (char *)realloc(data, size + 1)) == NULL) {
        free(data);
        return NULL;
    }
    text[size] = 0;
    return text;
}

/* Heroic's list of the GOG games it installed, a JSON file: each
 * "install_path" in it */
static int from_heroic(Walk *w, const char *json)
{
    static const char key[] = "\"install_path\"";
    char *data = load_text(json), *p, dir[SYS_PATH];
    int found = 0;

    if (!data)
        return 0;
    for (p = strstr(data, key); p && !found; p = strstr(p, key)) {
        size_t len = 0;

        p += sizeof key - 1;
        while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if (*p++ != '"')
            continue;
        while (*p && *p != '"' && len + 1 < sizeof dir) {
            if (*p == '\\' && p[1])
                p++;
            dir[len++] = *p++;
        }
        dir[len] = 0;
        found = install(w, dir, 1);
    }
    free(data);
    return found;
}

/* A menu entry of GOG's Linux installer, gog_com-NAME_1.desktop: the
 * game's folder, wherever the player installed it, in its Path= (as
 * seen with one release) or as the folder of the start.sh its Exec=
 * starts */
static void menu_entry(void *ctx, const char *name, int is_dir)
{
    Walk *w = (Walk *)ctx;
    size_t len = strlen(name);
    char path[SYS_PATH], dir[SYS_PATH], *data, *line, *end;

    if (w->found || is_dir || strncmp(name, "gog_com-", 8) || len < 16 ||
        strcmp(name + len - 8, ".desktop"))
        return;
    sys_join(path, sizeof path, w->dir, name);
    if ((data = load_text(path)) == NULL)
        return;
    for (line = data; line && !w->found; line = end ? end + 1 : NULL) {
        char *from, *to;

        end = strchr(line, '\n');
        if (end)
            *end = 0;
        if (!strncmp(line, "Path=", 5)) {
            from = line + 5;
            to = from + strcspn(from, "\r");
        } else if (!strncmp(line, "Exec=", 5) && (to = strstr(line, "/start.sh")) != NULL) {
            for (from = to; from > line + 5 && from[-1] != '"' && from[-1] != '\''; from--)
                ;
        } else {
            continue;
        }
        if (*from == '"' && to > from && to[-1] == '"') {
            from++;
            to--;
        }
        snprintf(dir, sizeof dir, "%.*s", (int)(to - from), from);
        w->found = install(w, dir, 1);
    }
    free(data);
}
#endif

/* calls fn for each folder the game may be installed in, until it
 * returns 1; 1 then */
static int each_install_dir(const GogRelease *rel, int (*fn)(const char *dir, void *ctx),
                            void *ctx)
{
    char path[SYS_PATH];
#ifdef _WIN32
    /* GOG's installers keep a key per game under GOG.com\Games, named by
     * its product ID, with the folder in its value "path" (so on a
     * Windows installation of one game, the name written "PATH"; the
     * registry ignores case) */
    static const char *const keys[] = {
        "SOFTWARE\\WOW6432Node\\GOG.com\\Games", "SOFTWARE\\GOG.com\\Games",
    };
    char key[256], dir[MAX_PATH];
    const char *pf = getenv("ProgramFiles(x86)");
    char drive;
    size_t i;

    for (i = 0; rel->gog_id && *rel->gog_id && i < sizeof keys / sizeof keys[0]; i++) {
        DWORD size = sizeof dir;
        snprintf(key, sizeof key, "%s\\%s", keys[i], rel->gog_id);
        if (RegGetValueA(HKEY_LOCAL_MACHINE, key, "path", RRF_RT_REG_SZ, NULL, dir, &size) ==
                ERROR_SUCCESS && fn(dir, ctx))
            return 1;
    }
    /* the installer's default folder, on any drive; GOG Galaxy's */
    for (drive = 'C'; drive <= 'Z'; drive++) {
        snprintf(path, sizeof path, "%c:\\GOG Games\\%s", drive, rel->folder);
        if (fn(path, ctx))
            return 1;
    }
    snprintf(path, sizeof path, "%s\\GOG Galaxy\\Games\\%s",
             pf ? pf : "C:\\Program Files (x86)", rel->folder);
    return fn(path, ctx);
#else
    /* On a Mac the release is an application.  On Linux GOG's Linux
     * release where its installer (the .sh) put it: its menu entry names
     * the folder, by default ~/GOG Games/FOLDER (as seen with one
     * release; /opt/GOG Games as root).  Or the Windows release: in
     * Heroic's list of installed games (as installed and as a Flatpak),
     * where Heroic, Minigalaxy and Lutris put games by default, in the
     * Wine prefixes of Wine, Lutris and Bottles.  These folders are the
     * programs' defaults as documented, not checked on a machine. */
    static const char *const menus[] = {
        ".local/share/applications", "Desktop", "/usr/share/applications",
    };
    static const char *const heroic[] = {
        ".config/heroic/gog_store/installed.json",
        ".var/app/com.heroicgameslauncher.hgl/config/heroic/gog_store/installed.json",
    };
    static const char *const game_dirs[] = {
        "GOG Games", "/opt/GOG Games", "Games/Heroic",
    };
    static const char *const scan_dirs[] = {
        "Games", "Games/Heroic", "Games/Heroic/Prefixes",
        ".local/share/bottles/bottles",
        ".var/app/com.usebottles.bottles/data/bottles/bottles",
        ".local/share/wineprefixes",
    };
    const char *wineprefix = getenv("WINEPREFIX");
    char home[SYS_PATH], dir[SYS_PATH];
    Walk w;
    size_t i;

    /* a path too long for the buffer is passed over, not cut short */
    if (snprintf(path, sizeof path, "/Applications/%s.app", rel->folder) < (int)sizeof path
        && fn(path, ctx))
        return 1;
    sys_home_dir(home, sizeof home);
    sys_join(dir, sizeof dir, home, "Applications");
    if (snprintf(path, sizeof path, "%s/%s.app", dir, rel->folder) < (int)sizeof path
        && fn(path, ctx))
        return 1;
    w.rel = rel;
    w.fn = fn;
    w.ctx = ctx;
    w.found = 0;
    for (i = 0; i < sizeof menus / sizeof menus[0]; i++) {
        sys_join(dir, sizeof dir, menus[i][0] == '/' ? "" : home, menus[i]);
        w.dir = dir;
        sys_list_dir(dir, menu_entry, &w);
        if (w.found)
            return 1;
    }
    for (i = 0; i < sizeof heroic / sizeof heroic[0]; i++) {
        sys_join(path, sizeof path, home, heroic[i]);
        if (from_heroic(&w, path))
            return 1;
    }
    for (i = 0; i < sizeof game_dirs / sizeof game_dirs[0]; i++) {
        sys_join(dir, sizeof dir, game_dirs[i][0] == '/' ? "" : home, game_dirs[i]);
        sys_join(path, sizeof path, dir, rel->folder);
        if (install(&w, path, 0))
            return 1;
    }
    if (wineprefix && *wineprefix && in_prefix(&w, wineprefix))
        return 1;
    sys_join(dir, sizeof dir, home, ".wine");
    if (in_prefix(&w, dir))
        return 1;
    for (i = 0; i < sizeof scan_dirs / sizeof scan_dirs[0]; i++) {
        sys_join(dir, sizeof dir, home, scan_dirs[i]);
        w.dir = dir;
        sys_list_dir(dir, scan_entry, &w);
        if (w.found)
            return 1;
    }
    return 0;
#endif
}

typedef struct {
    const char *image, *must_have;
    char *out;
    size_t n;
} Search;

static int image_in(const char *dir, void *ctx)
{
    Search *s = (Search *)ctx;
    return take_in(dir, s->image, s->must_have, s->out, s->n);
}

static int folder_with(const char *dir, void *ctx)
{
    Search *s = (Search *)ctx;
    if (!sys_has_marker(dir, s->must_have))
        return 0;
    snprintf(s->out, s->n, "%s", dir);
    return 1;
}

int gog_find(const GogRelease *rel, char *out, size_t n)
{
    char dir[SYS_PATH];
    Search s;

    s.image = rel->image && *rel->image ? rel->image : "game.gog";
    s.must_have = rel->must_have && *rel->must_have ? rel->must_have : NULL;
    s.out = out;
    s.n = n;
    sys_exe_dir(dir, sizeof dir);
    if (take_in(dir, s.image, s.must_have, out, n) || take(s.image, s.must_have, out, n))
        return 1;
    /* where a Mac app keeps its files: its own folder is in the bundle */
    sys_data_dir(dir, sizeof dir);
    if (take_in(dir, s.image, s.must_have, out, n))
        return 1;
#ifndef _WIN32
    if (rel->mac_bundle && *rel->mac_bundle) {
        /* on a Mac the release is an application, the image inside */
        char home[SYS_PATH], path[SYS_PATH];

        snprintf(path, sizeof path, "/Applications/%s", rel->mac_bundle);
        if (take(path, s.must_have, out, n))
            return 1;
        sys_home_dir(home, sizeof home);
        sys_join(dir, sizeof dir, home, "Applications");
        sys_join(path, sizeof path, dir, rel->mac_bundle);
        if (take(path, s.must_have, out, n))
            return 1;
    }
#endif
    return each_install_dir(rel, image_in, &s);
}

int gog_find_folder(const GogRelease *rel, char *out, size_t n)
{
    Search s;

    if (!rel->must_have || !*rel->must_have)
        return 0;
    s.image = NULL;
    s.must_have = rel->must_have;
    s.out = out;
    s.n = n;
    return each_install_dir(rel, folder_with, &s);
}

/* ---- the image */

#define RAW 2352
#define DATA 2048
#define MAX_DEPTH 8

typedef struct {
    FILE *f;
    long raw;                   /* sector size in the image: 2048 or 2352 */
    long total, done;           /* bytes */
    int writing, has_program;
    const char *must_have;
    int (*progress)(void *ctx, const char *file, long done, long total);
    void *ctx;
    char *err;
    size_t n;
} Unpack;

/* sector `lba`'s 2048 bytes of data */
static int sector(Unpack *u, uint32_t lba, uint8_t *out)
{
    uint8_t raw[RAW];

    if (fseek(u->f, (long)lba * u->raw, SEEK_SET) != 0 ||
        fread(raw, 1, (size_t)u->raw, u->f) != (size_t)u->raw)
        return -1;
    if (u->raw == DATA)
        memcpy(out, raw, DATA);
    else if (raw[15] == 1)
        memcpy(out, raw + 16, DATA);
    else if (raw[15] == 2)
        memcpy(out, raw + 24, DATA);    /* after the 8-byte subheader */
    else
        return -1;
    return 0;
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int fail(Unpack *u, const char *what)
{
    snprintf(u->err, u->n, "%s", what);
    return -1;
}

/* a name the CD may have: letters, digits and the few signs DOS allows */
static int plain_name(const char *s)
{
    if (!*s || !strcmp(s, ".") || !strcmp(s, ".."))
        return 0;
    for (; *s; s++)
        if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') ||
              strchr("_-.~!#$%&'()@^{}", *s)))
            return 0;
    return 1;
}

/* a file of `size` bytes from sector `lba` into `path` */
static int copy_file(Unpack *u, uint32_t lba, uint32_t size, const char *path)
{
    uint8_t buf[DATA];
    FILE *out = fopen(path, "wb");
    uint32_t left = size;

    if (!out)
        return fail(u, "A file could not be written.");
    while (left) {
        uint32_t k = left < DATA ? left : DATA;
        if (sector(u, lba++, buf) || fwrite(buf, 1, k, out) != k) {
            fclose(out);
            return fail(u, "The image could not be read to its end, or a file not written.");
        }
        left -= k;
    }
    return fclose(out) == 0 ? 0 : fail(u, "A file could not be written.");
}

/* each entry of the directory at `lba` (`size` bytes); `dir` the folder
 * it goes to (writing), `rel` its path on the CD */
static int walk(Unpack *u, uint32_t lba, uint32_t size, const char *dir, const char *rel, int depth)
{
    uint8_t *data;
    uint32_t i, s, sectors = (size + DATA - 1) / DATA;
    int r = 0;

    if (depth > MAX_DEPTH || size > 0x100000)
        return fail(u, "The image's folders are not as a CD's are.");
    data = (uint8_t *)malloc((size_t)sectors * DATA);
    if (!data)
        return fail(u, "No memory.");
    for (s = 0; s < sectors; s++)
        if (sector(u, lba + s, data + s * DATA)) {
            free(data);
            return fail(u, "The image cannot be read.");
        }
    for (i = 0; r == 0 && i < size;) {
        uint8_t len = data[i];
        char name[64], path[SYS_PATH], sub[256];
        int is_dir;
        uint32_t elba, esize;

        if (len == 0) {                 /* records never cross a sector */
            i = (i / DATA + 1) * DATA;
            continue;
        }
        if (len < 34 || i + len > size || 33u + data[i + 32] > len) {
            r = fail(u, "The image's folders are not as a CD's are.");
            break;
        }
        snprintf(name, sizeof name, "%.*s", data[i + 32], (const char *)data + i + 33);
        elba = le32(data + i + 2);
        esize = le32(data + i + 10);
        is_dir = (data[i + 25] & 2) != 0;
        i += len;
        if (name[0] == 0 || name[0] == 1)
            continue;                   /* the folder itself, its parent */
        name[strcspn(name, ";")] = 0;
        if (!plain_name(name)) {
            r = fail(u, "The image has a file name no DOS CD has.");
            break;
        }
        snprintf(sub, sizeof sub, "%s%s%s", rel, *rel ? "/" : "", name);
        if (u->writing)
            sys_join(path, sizeof path, dir, name);
        if (is_dir) {
            if (u->writing && sys_mkdir(path) != 0)
                r = fail(u, "A folder could not be made.");
            else
                r = walk(u, elba, esize, path, sub, depth + 1);
        } else if (u->writing) {
            r = copy_file(u, elba, esize, path);
            u->done += (long)esize;
            if (r == 0 && u->progress && u->progress(u->ctx, sub, u->done, u->total))
                r = fail(u, "Stopped.");
        } else {
            u->total += (long)esize;
            if (u->must_have && !sys_stricmp(sub, u->must_have))
                u->has_program = 1;
        }
    }
    free(data);
    return r;
}

/* the folder and everything in it (only ever our own `dir`.part) */
static void remove_entry(void *ctx, const char *name, int is_dir);

static void remove_tree(const char *dir)
{
    sys_list_dir(dir, remove_entry, (void *)dir);
    sys_rmdir(dir);
}

static void remove_entry(void *ctx, const char *name, int is_dir)
{
    char path[SYS_PATH];
    sys_join(path, sizeof path, (const char *)ctx, name);
    if (is_dir)
        remove_tree(path);
    else
        remove(path);
}

/* `image` opened into u->f, its sector size found and its primary
 * volume descriptor (sector 16: the root directory's record at 156) read
 * into `pvd`; 0, else -1 with the reason in u->err and nothing open */
static int open_image(Unpack *u, const char *image, uint8_t *pvd)
{
    static const uint8_t sync[12] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    uint8_t head[12];

    u->f = fopen(image, "rb");
    if (!u->f)
        return fail(u, "The image cannot be opened.");
    u->raw = fread(head, 1, 12, u->f) == 12 && !memcmp(head, sync, 12) ? RAW : DATA;
    if (sector(u, 16, pvd) || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
        fclose(u->f);
        u->f = NULL;
        return fail(u, "The image holds no CD file system.");
    }
    return 0;
}

/* 1 if `image` is a CD image holding `must_have` */
static int has_file(const char *image, const char *must_have)
{
    Unpack u;
    uint8_t pvd[DATA];
    char err[128];

    memset(&u, 0, sizeof u);
    u.err = err;
    u.n = sizeof err;
    u.must_have = must_have;
    if (open_image(&u, image, pvd) != 0)
        return 0;
    if (walk(&u, le32(pvd + 156 + 2), le32(pvd + 156 + 10), NULL, "", 0) != 0)
        u.has_program = 0;
    fclose(u.f);
    return u.has_program;
}

int cd_unpack(const char *image, const char *dir, const char *must_have,
              int (*progress)(void *ctx, const char *file, long done, long total), void *ctx,
              char *err, size_t n)
{
    Unpack u;
    uint8_t pvd[DATA];
    char part[SYS_PATH];
    int r;

    memset(&u, 0, sizeof u);
    u.progress = progress;
    u.ctx = ctx;
    u.err = err;
    u.n = n;
    if (sys_is_dir(dir) || sys_is_file(dir))
        return fail(&u, "The folder for the game's files is there already.");
    u.must_have = must_have && *must_have ? must_have : NULL;
    if (open_image(&u, image, pvd) != 0)
        return -1;
    r = walk(&u, le32(pvd + 156 + 2), le32(pvd + 156 + 10), NULL, "", 0);
    if (r == 0 && u.must_have && !u.has_program)
        r = fail(&u, "The image is not the game's CD (a file it must have is missing).");
    if (r == 0) {
        snprintf(part, sizeof part, "%s.part", dir);
        if (sys_is_dir(part))
            remove_tree(part);          /* left by an unpacking that was stopped */
        u.writing = 1;
        if (sys_mkdir(part) != 0)
            r = fail(&u, "The folder for the game's files cannot be made.");
        else
            r = walk(&u, le32(pvd + 156 + 2), le32(pvd + 156 + 10), part, "", 0);
        if (r == 0 && sys_rename(part, dir) != 0)
            r = fail(&u, "The unpacked files could not be moved to their folder.");
        if (r != 0)
            remove_tree(part);
    }
    fclose(u.f);
    return r;
}

/* ---- an installed folder */

typedef struct {
    const char *src, *dst;      /* the folders being walked */
    long total, done;           /* bytes */
    int writing, depth, r;
    int (*progress)(void *ctx, const char *file, long done, long total);
    void *ctx;
    char *err;
    size_t n;
} Copy;

static void copy_fail(Copy *c, const char *what)
{
    if (c->r == 0)
        snprintf(c->err, c->n, "%s", what);
    c->r = -1;
}

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    long k = -1;

    if (f && fseek(f, 0, SEEK_END) == 0)
        k = ftell(f);
    if (f)
        fclose(f);
    return k;
}

static void copy_bytes(Copy *c, const char *from, const char *to)
{
    static uint8_t buf[65536];
    FILE *in = fopen(from, "rb"), *out = in ? fopen(to, "wb") : NULL;
    size_t k;

    if (!in || !out) {
        copy_fail(c, in ? "A file could not be written." : "A file of the installation cannot be read.");
    } else {
        while ((k = fread(buf, 1, sizeof buf, in)) > 0) {
            if (fwrite(buf, 1, k, out) != k) {
                copy_fail(c, "A file could not be written.");
                break;
            }
            c->done += (long)k;
        }
        if (ferror(in))
            copy_fail(c, "A file of the installation cannot be read.");
    }
    if (in)
        fclose(in);
    if (out && fclose(out) != 0)
        copy_fail(c, "A file could not be written.");
}

static void copy_entry(void *ctx, const char *name, int is_dir);

/* each entry of `src` counted, or copied into `dst` */
static void copy_dir(Copy *c, const char *src, const char *dst)
{
    const char *old_src = c->src, *old_dst = c->dst;

    if (c->depth >= 32) {                       /* links to folders may loop */
        copy_fail(c, "The installation's folders are nested too deep.");
        return;
    }
    c->src = src;
    c->dst = dst;
    c->depth++;
    if (sys_list_dir(src, copy_entry, c) != 0)
        copy_fail(c, "A folder of the installation cannot be read.");
    c->depth--;
    c->src = old_src;
    c->dst = old_dst;
}

static void copy_entry(void *ctx, const char *name, int is_dir)
{
    Copy *c = (Copy *)ctx;
    char from[SYS_PATH], to[SYS_PATH];

    if (c->r)
        return;
    sys_join(from, sizeof from, c->src, name);
    if (c->writing)
        sys_join(to, sizeof to, c->dst, name);
    if (is_dir) {
        if (c->writing && sys_mkdir(to) != 0)
            copy_fail(c, "A folder could not be made.");
        else
            copy_dir(c, from, to);
    } else if (!c->writing) {
        long k = file_size(from);
        if (k > 0)
            c->total += k;
    } else {
        copy_bytes(c, from, to);
        if (c->r == 0 && c->progress && c->progress(c->ctx, name, c->done, c->total))
            copy_fail(c, "Stopped.");
    }
}

int gog_copy(const char *folder, const char *dir, const char *must_have,
             int (*progress)(void *ctx, const char *file, long done, long total), void *ctx,
             char *err, size_t n)
{
    Copy c;
    char part[SYS_PATH];

    memset(&c, 0, sizeof c);
    c.progress = progress;
    c.ctx = ctx;
    c.err = err;
    c.n = n;
    if (sys_is_dir(dir) || sys_is_file(dir)) {
        copy_fail(&c, "The folder for the game's files is there already.");
        return -1;
    }
    if (!sys_is_dir(folder)) {
        copy_fail(&c, "The installed game's folder is not there.");
        return -1;
    }
    if (must_have && *must_have && !sys_has_marker(folder, must_have)) {
        copy_fail(&c, "The folder is not the game's installation (a file it must have is missing).");
        return -1;
    }
    copy_dir(&c, folder, NULL);
    if (c.r)
        return -1;
    snprintf(part, sizeof part, "%s.part", dir);
    if (sys_is_dir(part))
        remove_tree(part);                      /* left by a copy that was stopped */
    c.writing = 1;
    if (sys_mkdir(part) != 0)
        copy_fail(&c, "The folder for the game's files cannot be made.");
    else
        copy_dir(&c, folder, part);
    if (c.r == 0 && sys_rename(part, dir) != 0)
        copy_fail(&c, "The copied files could not be moved to their folder.");
    if (c.r != 0)
        remove_tree(part);
    return c.r;
}

/* ---- a cue sheet's disc */

/* `name` (a cue sheet's FILE, '\\' or '/' between its parts) under `dir`,
 * each part found whatever its case; 1 if there */
static int cue_find(const char *dir, const char *name, char *out, size_t n)
{
    char at[SYS_PATH], part[260];
    const char *p = name;

    snprintf(at, sizeof at, "%s", dir);
    for (;;) {
        size_t len = 0;

        while (*p == '\\' || *p == '/')
            p++;
        while (p[len] && p[len] != '\\' && p[len] != '/' && len < sizeof part - 1) {
            part[len] = p[len];
            len++;
        }
        part[len] = 0;
        p += len;
        if (!len)
            break;
        if (!sys_find(at, part, out, n))
            return 0;
        snprintf(at, sizeof at, "%s", out);
    }
    snprintf(out, n, "%s", at);
    return 1;
}

/* the next FILE line's name of the sheet `f` into name; 0 at the end */
static int cue_next_file(FILE *f, char *name, size_t n)
{
    char line[1024];

    while (fgets(line, sizeof line, f)) {
        char *p = line, *e;

        while (*p == ' ' || *p == '\t')
            p++;
        if (toupper((unsigned char)p[0]) != 'F' || toupper((unsigned char)p[1]) != 'I' ||
            toupper((unsigned char)p[2]) != 'L' || toupper((unsigned char)p[3]) != 'E' ||
            (p[4] != ' ' && p[4] != '\t'))
            continue;
        p += 4;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '"') {
            e = strchr(++p, '"');
            if (!e)
                continue;
        } else {
            e = p;
            while (*e && *e != ' ' && *e != '\t' && *e != '\r' && *e != '\n')
                e++;
        }
        snprintf(name, n, "%.*s", (int)(e - p), p);
        return 1;
    }
    return 0;
}

/* `name`'s folders made under `root` and the file's path there */
static int cue_target(const char *root, const char *name, char *out, size_t n)
{
    char at[SYS_PATH], part[260];
    const char *p = name;

    snprintf(at, sizeof at, "%s", root);
    for (;;) {
        size_t len = 0;

        while (*p == '\\' || *p == '/')
            p++;
        while (p[len] && p[len] != '\\' && p[len] != '/' && len < sizeof part - 1) {
            part[len] = p[len];
            len++;
        }
        part[len] = 0;
        p += len;
        if (!len)
            return 0;
        sys_join(out, n, at, part);
        while (*p == '\\' || *p == '/')
            p++;
        if (!*p)
            return 1;
        if (!sys_is_dir(out) && sys_mkdir(out) != 0)
            return 0;
        snprintf(at, sizeof at, "%s", out);
    }
}

int cd_copy_disc(const char *cue, const char *dir,
                 int (*progress)(void *ctx, const char *file, long done, long total), void *ctx,
                 char *err, size_t n)
{
    Copy c;
    char src_dir[SYS_PATH], part[SYS_PATH], name[512], from[SYS_PATH], to[SYS_PATH];
    const char *base;
    FILE *f;
    int pass;

    memset(&c, 0, sizeof c);
    c.progress = progress;
    c.ctx = ctx;
    c.err = err;
    c.n = n;
    if (sys_is_dir(dir) || sys_is_file(dir)) {
        copy_fail(&c, "The folder for the CD's files is there already.");
        return -1;
    }
    if (!sys_is_file(cue) || !sys_parent(cue, src_dir, sizeof src_dir)) {
        copy_fail(&c, "The CD's cue sheet is not there.");
        return -1;
    }
    base = cue + strlen(src_dir);
    while (*base == '/' || *base == '\\')
        base++;
    snprintf(part, sizeof part, "%s.part", dir);
    for (pass = 0; pass < 2 && c.r == 0; pass++) {
        c.writing = pass;
        if (pass == 1) {
            if (sys_is_dir(part))
                remove_tree(part);
            if (sys_mkdir(part) != 0) {
                copy_fail(&c, "The folder for the CD's files cannot be made.");
                break;
            }
            sys_join(to, sizeof to, part, base);
            copy_bytes(&c, cue, to);
        } else {
            c.total += file_size(cue);
        }
        f = fopen(cue, "r");
        if (!f) {
            copy_fail(&c, "The CD's cue sheet cannot be read.");
            break;
        }
        while (c.r == 0 && cue_next_file(f, name, sizeof name)) {
            if (!cue_find(src_dir, name, from, sizeof from)) {
                copy_fail(&c, "A file the CD's cue sheet names is missing.");
                break;
            }
            if (pass == 0) {
                c.total += file_size(from);
                continue;
            }
            if (!cue_target(part, name, to, sizeof to)) {
                copy_fail(&c, "A folder for the CD's files could not be made.");
                break;
            }
            copy_bytes(&c, from, to);
            if (c.r == 0 && c.progress && c.progress(c.ctx, name, c.done, c.total))
                copy_fail(&c, "Stopped.");
        }
        fclose(f);
    }
    if (c.r == 0 && sys_rename(part, dir) != 0)
        copy_fail(&c, "The copied files could not be moved to their folder.");
    if (c.r != 0 && sys_is_dir(part))
        remove_tree(part);
    return c.r;
}
