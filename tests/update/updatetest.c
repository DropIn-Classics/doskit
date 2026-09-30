/* updatetest.c - the runtime's update.c and sys_data_migrate, run by
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
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void compare_and_parse(void)
{
    UpdateInfo u;

    expect(update_compare("v1.10", "v1.9") > 0, "v1.10 after v1.9");
    expect(update_compare("v1.2", "v1.2.0") == 0, "v1.2 = v1.2.0");
    expect(update_compare("v1.2", "v1.2.1") < 0, "v1.2 before v1.2.1");
    expect(update_compare("1.3", "v1.2") > 0, "1.3 after v1.2");
    expect(update_parse("{\"version\": \"v2.0\", \"page\" : \"https://x/y\","
                        " \"notes\": \"a \\\"b\\\"\\nc \\u00e4\"}", &u), "parsed");
    expect(!strcmp(u.version, "v2.0") && !strcmp(u.page, "https://x/y"), "version, page");
    expect(!strcmp(u.notes, "a \"b\"\nc \xc3\xa4"), "notes decoded");
    expect(update_parse("{\"version\": \"v2.0\", \"page\": \"file:///etc\"}", &u) &&
           u.page[0] == 0, "a page not https:// dropped");
    expect(!update_parse("{\"page\": \"https://x\"}", &u), "no version, nothing");
    expect(!update_open("file:///etc/passwd"), "only https:// opened");
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
