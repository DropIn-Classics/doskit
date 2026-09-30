/* cdimage.c - see cdimage.h */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
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

/* dir/name */
static int take_in(const char *dir, const char *name, const char *must_have, char *out, size_t n)
{
    char path[SYS_PATH];
    sys_join(path, sizeof path, dir, name);
    return take(path, must_have, out, n);
}

#ifdef _WIN32
/* GOG's installers keep a key per game under GOG.com\Games, named by
 * its product ID, with the folder in its value "path" (so on a Windows
 * installation of one game, the name written "PATH"; the registry
 * ignores case) */
static int from_registry(const char *games, const GogRelease *rel, const char *image,
                         const char *must_have, char *out, size_t n)
{
    char key[256], dir[MAX_PATH];
    DWORD size = sizeof dir;

    snprintf(key, sizeof key, "%s\\%s", games, rel->gog_id);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, key, "path", RRF_RT_REG_SZ, NULL, dir, &size) !=
        ERROR_SUCCESS)
        return 0;
    return take_in(dir, image, must_have, out, n);
}
#endif

int gog_find(const GogRelease *rel, char *out, size_t n)
{
    char dir[SYS_PATH], path[SYS_PATH];
    const char *image = rel->image && *rel->image ? rel->image : "game.gog";
    const char *must = rel->must_have && *rel->must_have ? rel->must_have : NULL;

    sys_exe_dir(dir, sizeof dir);
    if (take_in(dir, image, must, out, n) || take(image, must, out, n))
        return 1;
#ifdef _WIN32
    {
        char drive;
        const char *pf = getenv("ProgramFiles(x86)");

        if (rel->gog_id && *rel->gog_id &&
            (from_registry("SOFTWARE\\WOW6432Node\\GOG.com\\Games", rel, image, must, out, n) ||
             from_registry("SOFTWARE\\GOG.com\\Games", rel, image, must, out, n)))
            return 1;
        /* the installer's default folder, on any drive; GOG Galaxy's */
        for (drive = 'C'; drive <= 'Z'; drive++) {
            snprintf(path, sizeof path, "%c:\\GOG Games\\%s", drive, rel->folder);
            if (take_in(path, image, must, out, n))
                return 1;
        }
        snprintf(path, sizeof path, "%s\\GOG Galaxy\\Games\\%s",
                 pf ? pf : "C:\\Program Files (x86)", rel->folder);
        if (take_in(path, image, must, out, n))
            return 1;
    }
#else
    {
        /* Elsewhere (the Windows release under Wine, Heroic, Lutris):
         * guesses at the usual folders, not checked */
        static const char *const home_dirs[] = {
            "GOG Games", "Games/Heroic", ".wine/drive_c/GOG Games",
        };
        char home[SYS_PATH];
        size_t i;

        sys_home_dir(home, sizeof home);
        if (rel->mac_bundle && *rel->mac_bundle) {
            /* on a Mac the release is an application, the image inside */
            snprintf(path, sizeof path, "/Applications/%s", rel->mac_bundle);
            if (take(path, must, out, n))
                return 1;
            sys_join(dir, sizeof dir, home, "Applications");
            sys_join(path, sizeof path, dir, rel->mac_bundle);
            if (take(path, must, out, n))
                return 1;
        }
        for (i = 0; i < sizeof home_dirs / sizeof home_dirs[0]; i++) {
            sys_join(dir, sizeof dir, home, home_dirs[i]);
            sys_join(dir, sizeof dir, dir, rel->folder);
            if (take_in(dir, image, must, out, n))
                return 1;
        }
    }
#endif
    return 0;
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
