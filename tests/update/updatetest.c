/* updatetest.c - the runtime's update.c, sys_data_migrate and sys_join
 * into its folder's own buffer, run by
 * selftest.py with DK_DATA_DIR set to an empty folder:
 *
 *     updatetest fetch file:///.../latest.json    first start
 *     updatetest kept file:///nowhere             the next, the same day
 *
 * fetch: versions compared, latest.json's fields read (escapes, a page
 * that is not https), nothing looked for before the player's yes, then
 * the file fetched (curl) and v1.3 made known over v1.2; a file and a
 * folder beside the program moved into the data folder.  kept: the yes
 * remembered, v1.3 known from the kept file without fetching.  Says
 * "update ok". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sys.h"
#include "update.h"

static int fails;

static void expect(int ok, const char *what)
{
    if (!ok) {
        printf("FAILED: %s\n", what);
        fails++;
    }
}

/* update_poll until it knows a release, for at most 20 seconds */
static int wait_poll(UpdateInfo *u)
{
    time_t end = time(NULL) + 20;
    while (time(NULL) < end)
        if (update_poll(u))
            return 1;
    return 0;
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");        /* the bytes as given, on Windows too */
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void compare_and_parse(void)
{
    UpdateInfo u;
#ifdef _WIN32
    const char *asset = ",\"packages\":{\"windows-x64\":{\"file\":\"testgame-windows-x64.zip\",\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}}";
#elif defined(__APPLE__)
    const char *asset = "";
#else
    const char *asset = ",\"packages\":{\"linux-x64\":{\"file\":\"testgame-linux-x64.tar.gz\",\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}}";
#endif
    char json[1024];

    expect(update_compare("v1.10", "v1.9") > 0, "v1.10 after v1.9");
    expect(update_compare("v1.2", "v1.2.0") == 0, "v1.2 = v1.2.0");
    expect(update_compare("v1.2", "v1.2.1") < 0, "v1.2 before v1.2.1");
    expect(update_compare("1.3", "v1.2") > 0, "1.3 after v1.2");
    expect(update_compare("v1.0", "v1.0-rc2") > 0, "v1.0 after v1.0-rc2");
    expect(update_compare("v1.0-rc1", "v1.0-rc2") < 0, "v1.0-rc1 before v1.0-rc2");
    expect(update_compare("v1.0-rc2", "v1.0") < 0, "v1.0-rc2 before v1.0");
    expect(update_compare("v1.0-rc2", "v1.0-rc2") == 0, "the same rc");
    expect(update_compare("v1.1-rc1", "v1.0") > 0, "v1.1-rc1 after v1.0");
    expect(update_parse("{\"version\": \"v2.0\", \"page\" : \"https://x/y\","
                        " \"notes\": \"a \\\"b\\\"\\nc \\u00e4\"}", &u), "parsed");
    expect(!strcmp(u.version, "v2.0") && !strcmp(u.page, "https://x/y"), "version, page");
    expect(!strcmp(u.notes, "a \"b\"\nc \xc3\xa4"), "notes decoded");
    expect(update_parse("{\"version\": \"v2.0\", \"page\": \"file:///etc\"}", &u) &&
           u.page[0] == 0, "a page not https:// dropped");
    snprintf(json, sizeof json,
             "{\"version\":\"v2.0\",\"page\":\"https://github.com/x/y/releases/tag/v2.0\"%s}",
             asset);
    expect(update_parse(json, &u), "latest release asset parsed");
#ifndef __APPLE__
    expect(u.package[0] && strlen(u.sha256) == 64, "platform package and digest");
    snprintf(json, sizeof json,
             "{\"version\":\"v2.0\",\"page\":\"https://x/y\",\"packages\":{"
#ifdef _WIN32
             "\"windows-x64\":{\"file\":\"..\\\\bad.zip\",\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}"
#else
             "\"linux-x64\":{\"file\":\"../bad.tar.gz\",\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}"
#endif
             "}}");
    expect(!update_parse(json, &u), "unsafe asset path rejected");
#endif
    expect(!update_parse("{\"page\": \"https://x\"}", &u), "no version, nothing");
    expect(!update_open("file:///etc/passwd"), "only https:// opened");
}

/* sys_join into the folder's own buffer, as sys_data_dir does (glibc's
 * snprintf would have left "/share" of "/home/me/.local") */
static void joins(void)
{
    char path[16], sep[2] = { 0 };

    snprintf(path, sizeof path, "/home/me");
    sys_join(path, sizeof path, path, "share");
    sep[0] = path[8];
    expect(!strncmp(path, "/home/me", 8) && strchr("/\\", sep[0]) && !strcmp(path + 9, "share"),
           "sys_join into its folder's buffer");
    sys_join(path, sizeof path, path, "toolong");
    expect(strlen(path) == sizeof path - 1 && !strncmp(path, "/home/me", 8), "cut short, ended");
    snprintf(path, sizeof path, "/x/");
    sys_join(path, sizeof path, path, "y");
    expect(!strcmp(path, "/x/y"), "no second separator");
}

static void migrate(void)
{
    char exe[SYS_PATH], data[SYS_PATH], path[SYS_PATH], sub[SYS_PATH];
    static const char *const names[] = { "old.cfg", "oldsave", "missing", NULL };
    size_t size;
    uint8_t *buf;

    sys_exe_dir(exe, sizeof exe);
    sys_data_dir(data, sizeof data);
    sys_join(path, sizeof path, exe, "old.cfg");
    write_file(path, "a = 1\n");
    sys_join(path, sizeof path, exe, "oldsave");
    sys_mkdir(path);
    sys_join(sub, sizeof sub, path, "SLOT1");
    write_file(sub, "saved\n");
    expect(sys_data_migrate(names) == 2, "two entries moved");
    sys_join(path, sizeof path, data, "oldsave");
    sys_join(sub, sizeof sub, path, "SLOT1");
    buf = sys_load(sub, &size);
    expect(buf && size == 6 && !memcmp(buf, "saved\n", 6), "the save in the data folder");
    free(buf);
    sys_join(path, sizeof path, exe, "old.cfg");
    expect(!sys_is_file(path), "the old one gone from beside the program");
    expect(sys_data_migrate(names) == 0, "nothing moved twice");
}

int main(int argc, char **argv)
{
    UpdateInfo u;

    if (argc != 3)
        return 2;
    sys_set_app("doskit test", "doskit-test");
    if (!strcmp(argv[1], "fetch")) {
        compare_and_parse();
        joins();
        migrate();
        expect(update_consent() == -1, "not asked yet");
        update_start("v1.2", argv[2]);
        expect(!update_poll(&u), "nothing before the yes");
        update_set_consent(1);
        update_start("v1.2", argv[2]);
        expect(wait_poll(&u), "v1.3 fetched");
        expect(!strcmp(u.version, "v1.3") && !strcmp(u.notes, "Faster.\nFixed."), "its fields");
        update_set_consent(0);
        expect(!update_poll(&u), "nothing shown after a no");
        update_set_consent(1);
    } else {
        expect(update_consent() == 1, "the yes remembered");
        update_start("v1.2", argv[2]);
        expect(update_poll(&u) && !strcmp(u.version, "v1.3"), "v1.3 from the kept file");
    }
    if (!fails)
        printf("update ok (%s)\n", argv[1]);
    return fails != 0;
}
