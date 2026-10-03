/* update.c - see update.h */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sys.h"
#include "sha256.h"
#include "update.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#endif
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

#define DAY (24 * 60 * 60)
#define MAX_JSON 65536

static char current[32];
static int started, have;
static UpdateInfo found;

/* ---- update.cfg: "consent = N", "checked = T" */

static int cfg_consent = -2;            /* -2: not read yet */
static long long cfg_checked;

static void data_path(char *out, size_t n, const char *name)
{
    char data[SYS_PATH];
    sys_data_dir(data, sizeof data);
    sys_join(out, n, data, name);
}

static void cfg_read(void)
{
    char path[SYS_PATH], line[128], name[32];
    long long v;
    FILE *f;

    if (cfg_consent != -2)
        return;
    cfg_consent = -1;
    data_path(path, sizeof path, "update.cfg");
    f = fopen(path, "r");
    while (f && fgets(line, sizeof line, f))
        if (sscanf(line, " %31[a-z] = %lld", name, &v) == 2) {
            if (!strcmp(name, "consent") && (v == 0 || v == 1))
                cfg_consent = (int)v;
            else if (!strcmp(name, "checked"))
                cfg_checked = v;
        }
    if (f)
        fclose(f);
}

static void cfg_write(void)
{
    char path[SYS_PATH];
    FILE *f;

    data_path(path, sizeof path, "update.cfg");
    f = fopen(path, "w");
    if (!f)
        return;
    fprintf(f, "consent = %d\nchecked = %lld\n", cfg_consent, cfg_checked);
    fclose(f);
}

int update_consent(void)
{
    cfg_read();
    return cfg_consent;
}

void update_set_consent(int yes)
{
    cfg_read();
    cfg_consent = yes ? 1 : 0;
    cfg_write();
    if (!yes)
        have = 0;
}

/* ---- latest.json */

/* the string value of "key" in json, decoded (\uXXXX as UTF-8); 1 if there */
static int json_string(const char *json, const char *key, char *out, size_t n)
{
    char pat[40];
    const char *p = json;
    size_t k = 0;

    snprintf(pat, sizeof pat, "\"%s\"", key);
    while ((p = strstr(p, pat)) != NULL) {
        p += strlen(pat);
        while (isspace((unsigned char)*p))
            p++;
        if (*p != ':')
            continue;
        p++;
        while (isspace((unsigned char)*p))
            p++;
        if (*p != '"')
            continue;
        p++;
        while (*p && *p != '"') {
            char c = *p++;
            unsigned u;
            if (c == '\\' && *p) {
                c = *p++;
                switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'u':
                    if (sscanf(p, "%4x", &u) != 1 || strlen(p) < 4)
                        return 0;
                    p += 4;
                    if (u >= 0xD800 && u < 0xE000)
                        u = '?';                /* a surrogate pair's half: not kept */
                    if (u < 0x80) {
                        c = (char)u;
                        break;
                    }
                    if (u < 0x800) {
                        if (k + 2 < n) {
                            out[k++] = (char)(0xC0 | u >> 6);
                            out[k++] = (char)(0x80 | (u & 0x3F));
                        }
                    } else if (k + 3 < n) {
                        out[k++] = (char)(0xE0 | u >> 12);
                        out[k++] = (char)(0x80 | (u >> 6 & 0x3F));
                        out[k++] = (char)(0x80 | (u & 0x3F));
                    }
                    continue;
                default: break;                 /* \" \\ \/ as they are */
                }
            }
            if (k + 1 < n)
                out[k++] = c;
        }
        if (n)
            out[k] = 0;
        return *p == '"';
    }
    return 0;
}

/* Copy a flat JSON object value (used for the selected platform's asset). */
static int json_object(const char *json, const char *key, char *out, size_t n)
{
    char pat[40];
    const char *p = json;
    size_t len;
    int depth = 0, quoted = 0, escaped = 0;

    snprintf(pat, sizeof pat, "\"%s\"", key);
    while ((p = strstr(p, pat)) != NULL) {
        p += strlen(pat);
        while (isspace((unsigned char)*p)) p++;
        if (*p++ != ':') continue;
        while (isspace((unsigned char)*p)) p++;
        if (*p != '{') continue;
        {
            const char *start = p;
            do {
                char c = *p++;
                if (!c) return 0;
                if (quoted) {
                    if (escaped) escaped = 0;
                    else if (c == '\\') escaped = 1;
                    else if (c == '"') quoted = 0;
                } else if (c == '"') quoted = 1;
                else if (c == '{') depth++;
                else if (c == '}') depth--;
            } while (depth);
            len = (size_t)(p - start);
            if (len >= n) return 0;
            memcpy(out, start, len);
            out[len] = 0;
            return 1;
        }
    }
    return 0;
}

static int ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && !strcmp(s + n - m, suffix);
}

int update_parse(const char *json, UpdateInfo *info)
{
    char hash[256];
    size_t i;

    memset(info, 0, sizeof *info);
    if (!json_string(json, "version", info->version, sizeof info->version) ||
        !(info->version[0] == 'v' || isdigit((unsigned char)info->version[0])))
        return 0;
    if (!json_string(json, "page", info->page, sizeof info->page) ||
        strncmp(info->page, "https://", 8) != 0)
        info->page[0] = 0;
    json_string(json, "notes", info->notes, sizeof info->notes);
#if defined(_WIN32)
    if (json_object(json, "windows-x64", hash, sizeof hash)) {
        const char *suffix = "-windows-x64.zip";
#elif defined(__APPLE__)
    if (0) {
#else
    if (json_object(json, "linux-x64", hash, sizeof hash)) {
        const char *suffix = "-linux-x64.tar.gz";
#endif
        if (!json_string(hash, "file", info->package, sizeof info->package) ||
            !json_string(hash, "sha256", info->sha256, sizeof info->sha256))
            return 0;
        if (!info->package[0] || strstr(info->package, "..") ||
            strchr(info->package, '/') || strchr(info->package, '\\'))
            return 0;
        for (i = 0; info->package[i]; i++)
            if (!((info->package[i] >= 'a' && info->package[i] <= 'z') ||
                  (info->package[i] >= 'A' && info->package[i] <= 'Z') ||
                  (info->package[i] >= '0' && info->package[i] <= '9') ||
                  info->package[i] == '.' || info->package[i] == '_' ||
                  info->package[i] == '-'))
                return 0;
#if !defined(__APPLE__)
        if (!ends_with(info->package, suffix))
            return 0;
#endif
        if (strlen(info->sha256) != 64)
            return 0;
        for (i = 0; i < 64; i++)
            if (!((info->sha256[i] >= '0' && info->sha256[i] <= '9') ||
                  (info->sha256[i] >= 'a' && info->sha256[i] <= 'f') ||
                  (info->sha256[i] >= 'A' && info->sha256[i] <= 'F')))
                return 0;
    }
    return 1;
}

int update_compare(const char *a, const char *b)
{
    if (*a == 'v' || *a == 'V')
        a++;
    if (*b == 'v' || *b == 'V')
        b++;
    while (*a || *b) {
        long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
        if (x != y)
            return x < y ? -1 : 1;
        while (*a && !isdigit((unsigned char)*a))
            a++;
        while (*b && !isdigit((unsigned char)*b))
            b++;
    }
    return 0;
}

/* the kept latest.json read; `have` if it names a newer release */
static void read_kept(void)
{
    char path[SYS_PATH], *json;
    size_t size;
    uint8_t *buf;

    data_path(path, sizeof path, "latest.json");
    buf = sys_load(path, &size);
    if (!buf || size > MAX_JSON) {
        free(buf);
        return;
    }
    json = (char *)malloc(size + 1);
    if (json) {
        memcpy(json, buf, size);
        json[size] = 0;
        have = update_parse(json, &found) && update_compare(found.version, current) > 0;
        free(json);
    }
    free(buf);
}

/* ---- the fetch: into latest.part, in the background */

static int fetching;
static char part[SYS_PATH];

#ifdef _WIN32

static volatile LONG fetch_result;      /* 0 running, 1 done, 2 failed */
static wchar_t fetch_url[1024];
static char fetch_file[SYS_PATH];       /* the file of a file:// address, else "" */

/* file:// for the kit's test (as curl takes it elsewhere): the file
 * copied, no more than MAX_JSON bytes of it */
static int fetch_local(void)
{
    char buf[4096];
    size_t got, total = 0;
    FILE *in = fopen(fetch_file, "rb"), *f;
    int ok = 1;

    if (!in)
        return 0;
    f = fopen(part, "wb");
    if (!f) {
        fclose(in);
        return 0;
    }
    while (ok && (got = fread(buf, 1, sizeof buf, in)) > 0) {
        total += got;
        ok = total <= MAX_JSON && fwrite(buf, 1, got, f) == got;
    }
    fclose(in);
    if (fclose(f) != 0)
        ok = 0;
    return ok;
}

static DWORD WINAPI fetch_thread(LPVOID arg)
{
    URL_COMPONENTS uc;
    wchar_t host[256], path[1024];
    HINTERNET s = NULL, c = NULL, r = NULL;
    DWORD code = 0, len = sizeof code, got, total = 0;
    char buf[4096];
    FILE *f = NULL;
    int ok = 0;

    (void)arg;
    if (fetch_file[0]) {
        InterlockedExchange(&fetch_result, fetch_local() ? 1 : 2);
        return 0;
    }
    memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.lpszHostName = host;
    uc.dwHostNameLength = sizeof host / sizeof host[0];
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = sizeof path / sizeof path[0];
    if (!WinHttpCrackUrl(fetch_url, 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS)
        goto out;
    s = WinHttpOpen(L"doskit", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s)
        goto out;
    WinHttpSetTimeouts(s, 10000, 10000, 10000, 10000);
    c = WinHttpConnect(s, host, uc.nPort, 0);
    if (c)      /* GitHub's redirect to its file server is followed by WinHTTP */
        r = WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!r || !WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA,
                                  0, 0, 0) || !WinHttpReceiveResponse(r, NULL))
        goto out;
    if (!WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX)
        || code != 200)
        goto out;
    f = fopen(part, "wb");
    if (!f)
        goto out;
    ok = 1;
    while (ok && WinHttpReadData(r, buf, sizeof buf, &got) && got > 0) {
        total += got;
        ok = total <= MAX_JSON && fwrite(buf, 1, got, f) == got;
    }
out:
    if (f && fclose(f) != 0)
        ok = 0;
    if (r)
        WinHttpCloseHandle(r);
    if (c)
        WinHttpCloseHandle(c);
    if (s)
        WinHttpCloseHandle(s);
    InterlockedExchange(&fetch_result, ok ? 1 : 2);
    return 0;
}

static int fetch_begin(const char *url)
{
    HANDLE t;

    fetch_file[0] = 0;
    if (strncmp(url, "file://", 7) == 0) {
        const char *p = url + 7;
        if (p[0] == '/' && p[1] && p[2] == ':')     /* file:///C:/... */
            p++;
        snprintf(fetch_file, sizeof fetch_file, "%s", p);
    } else if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, fetch_url,
                                    sizeof fetch_url / sizeof fetch_url[0]))
        return 0;
    fetch_result = 0;
    t = CreateThread(NULL, 0, fetch_thread, NULL, 0, NULL);
    if (!t)
        return 0;
    CloseHandle(t);
    return 1;
}

/* -1 running, 0 failed, 1 done */
static int fetch_end(void)
{
    LONG r = InterlockedCompareExchange(&fetch_result, 0, 0);
    return r == 0 ? -1 : r == 1;
}

int update_open(const char *page)
{
    if (strncmp(page, "https://", 8) != 0)
        return 0;
    return (INT_PTR)ShellExecuteA(NULL, "open", page, NULL, NULL, SW_SHOWNORMAL) > 32;
}

#else

static pid_t fetch_pid, open_pid;

/* prog with args, its output and input /dev/null; 0 if it cannot be started */
static pid_t spawn(char *const argv[])
{
    posix_spawn_file_actions_t fa;
    pid_t pid;
    int r;

    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    r = posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    return r == 0 ? pid : 0;
}

static int fetch_begin(const char *url)
{
    /* file:// for the kit's test; a redirect only to https */
    char *argv[] = { "curl", "-fsSL", "--max-time", "20", "--max-filesize", "65536",
                     "--proto", "=https,file", "--proto-redir", "=https",
                     "-o", part, (char *)url, NULL };
    fetch_pid = spawn(argv);
    return fetch_pid != 0;
}

static int fetch_end(void)
{
    int st;
    pid_t r = waitpid(fetch_pid, &st, WNOHANG);
    if (r == 0)
        return -1;
    return r == fetch_pid && WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

int update_open(const char *page)
{
#ifdef __APPLE__
    char *argv[] = { "open", (char *)page, NULL };
#else
    char *argv[] = { "xdg-open", (char *)page, NULL };
#endif
    if (strncmp(page, "https://", 8) != 0)
        return 0;
    if (open_pid)
        waitpid(open_pid, NULL, WNOHANG);
    open_pid = spawn(argv);
    return open_pid != 0;
}

#endif

void update_start(const char *version, const char *url)
{
    long long now = (long long)time(NULL);
    char kept[SYS_PATH];

    if (started || !version || !*version || !url || !*url || update_consent() != 1)
        return;
    started = 1;
    snprintf(current, sizeof current, "%s", version);
    data_path(kept, sizeof kept, "latest.json");
    if (sys_is_file(kept) && now >= cfg_checked && now - cfg_checked < DAY) {
        read_kept();
        return;
    }
    data_path(part, sizeof part, "latest.part");
    remove(part);
    fetching = fetch_begin(url);
}

int update_poll(UpdateInfo *info)
{
    if (fetching) {
        int r = fetch_end();
        if (r >= 0) {
            fetching = 0;
            if (r) {
                char kept[SYS_PATH];
                data_path(kept, sizeof kept, "latest.json");
                remove(kept);                   /* Windows renames only onto nothing */
                if (sys_rename(part, kept) == 0) {
                    cfg_checked = (long long)time(NULL);
                    cfg_write();
                    read_kept();
                }
            } else {
                remove(part);
            }
        }
    }
    if (have && update_consent() == 1) {
        *info = found;
        return 1;
    }
    return 0;
}

/* ---- a package the player chose to install */

#define MAX_PACKAGE (512u * 1024u * 1024u)

static int download_package(const char *url, const char *path)
{
#ifdef _WIN32
    URL_COMPONENTS uc;
    wchar_t wurl[2048], host[256], request_path[1536];
    HINTERNET s = NULL, c = NULL, r = NULL;
    DWORD code = 0, len = sizeof code, got, total = 0;
    char buf[16384];
    FILE *f = NULL;
    int ok = 0;

    if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, sizeof wurl / sizeof wurl[0]))
        return 0;
    memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.lpszHostName = host;
    uc.dwHostNameLength = sizeof host / sizeof host[0];
    uc.lpszUrlPath = request_path;
    uc.dwUrlPathLength = sizeof request_path / sizeof request_path[0];
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS)
        return 0;
    s = WinHttpOpen(L"doskit updater", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) goto win_out;
    WinHttpSetTimeouts(s, 15000, 15000, 15000, 15000);
    c = WinHttpConnect(s, host, uc.nPort, 0);
    if (c)
        r = WinHttpOpenRequest(c, L"GET", request_path, NULL, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!r || !WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(r, NULL)) goto win_out;
    if (!WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &code, &len,
                             WINHTTP_NO_HEADER_INDEX) || code != 200) goto win_out;
    f = fopen(path, "wb");
    if (!f) goto win_out;
    ok = 1;
    while (ok) {
        if (!WinHttpReadData(r, buf, sizeof buf, &got)) {
            ok = 0;
            break;
        }
        if (!got)
            break;
        total += got;
        ok = total <= MAX_PACKAGE && fwrite(buf, 1, got, f) == got;
    }
win_out:
    if (f && fclose(f) != 0) ok = 0;
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return ok;
#else
    char *argv[] = { "curl", "-fsSL", "--max-time", "180", "--max-filesize",
                     "536870912", "--proto", "=https", "--proto-redir", "=https",
                     "-o", (char *)path, (char *)url, NULL };
    pid_t pid = spawn(argv);
    int st;
    if (!pid)
        return 0;
    while (waitpid(pid, &st, 0) < 0)
        if (errno != EINTR) return 0;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
#endif
}

static int package_hash_ok(const char *path, const char *expected)
{
    FILE *f = fopen(path, "rb");
    Sha256 s;
    uint8_t buf[32768], digest[32];
    char hex[65];
    size_t n;
    int i, ok;

    if (!f)
        return 0;
    sha256_init(&s);
    while ((n = fread(buf, 1, sizeof buf, f)) != 0)
        sha256_update(&s, buf, n);
    ok = !ferror(f);
    if (fclose(f) != 0) ok = 0;
    if (!ok)
        return 0;
    sha256_final(&s, digest);
    for (i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    hex[64] = 0;
    return sys_stricmp(hex, expected) == 0;
}

static int make_package_url(const UpdateInfo *info, char *url, size_t n)
{
    const char *tag = strstr(info->page, "/releases/tag/");
    const char *v;
    char version[32];
    size_t i, prefix;

    if (strncmp(info->page, "https://github.com/", 19) || !tag || !info->package[0])
        return 0;
    v = tag + strlen("/releases/tag/");
    if (!*v || strcmp(v, info->version) || strchr(v, '/') || strlen(v) >= sizeof version)
        return 0;
    snprintf(version, sizeof version, "%s", v);
    for (i = 0; version[i]; i++)
        if (!((version[i] >= 'a' && version[i] <= 'z') ||
              (version[i] >= 'A' && version[i] <= 'Z') ||
              (version[i] >= '0' && version[i] <= '9')) && version[i] != '.' &&
            version[i] != '-' && version[i] != '_')
            return 0;
    prefix = (size_t)(tag - info->page);
    {
        int wrote = snprintf(url, n, "%.*s/releases/download/%s/%s", (int)prefix,
                             info->page, version, info->package);
        return wrote >= 0 && (size_t)wrote < n;
    }
}

#ifndef __APPLE__
static int shell_quote(char *out, size_t n, const char *s)
{
    size_t used = 0;
    if (n < 3) return 0;
    out[used++] = '\'';
    while (*s) {
        if (*s == '\'') {
            if (used + 4 >= n) return 0;
            memcpy(out + used, "'\\''", 4);
            used += 4;
        } else {
            if (used + 2 >= n) return 0;
            out[used++] = *s;
        }
        s++;
    }
    out[used++] = '\'';
    out[used] = 0;
    return 1;
}
#endif

#ifdef _WIN32
static int powershell_quote(char *out, size_t n, const char *s)
{
    size_t used = 0;
    if (n < 3) return 0;
    out[used++] = '\'';
    while (*s) {
        if (used + (*s == '\'' ? 2u : 1u) + 2u > n)
            return 0;
        out[used++] = *s;
        if (*s == '\'') out[used++] = '\'';
        s++;
    }
    out[used++] = '\'';
    out[used] = 0;
    return 1;
}
#endif

#ifndef __APPLE__
static int update_parent(const char *dir, char *parent, size_t n)
{
    char probe[SYS_PATH], name[64];
    int wrote;

    if (!sys_parent(dir, parent, n))
        return 0;
#ifdef _WIN32
    wrote = snprintf(name, sizeof name, ".doskit-write-probe-%lu",
                     (unsigned long)GetCurrentProcessId());
#else
    wrote = snprintf(name, sizeof name, ".doskit-write-probe-%ld", (long)getpid());
#endif
    if (wrote < 0 || (size_t)wrote >= sizeof name)
        return 0;
    sys_join(probe, sizeof probe, parent, name);
    if (strlen(probe) >= sizeof probe - 1)
        return 0;
#ifdef _WIN32
    {
        HANDLE h = CreateFileA(probe, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                               FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return 0;
        if (!CloseHandle(h) || !DeleteFileA(probe)) {
            DeleteFileA(probe);
            return 0;
        }
    }
#else
    {
        int fd = open(probe, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0)
            return 0;
        if (close(fd) != 0 || unlink(probe) != 0) {
            unlink(probe);
            return 0;
        }
    }
#endif
    return 1;
}
#endif

int update_install(const UpdateInfo *info)
{
    char url[2048], data[SYS_PATH], package[SYS_PATH], exe[SYS_PATH],
         dir[SYS_PATH], script[SYS_PATH];
#ifndef __APPLE__
    char parent[SYS_PATH];
#endif
    FILE *f;

    if (!info)
        return 0;

#ifdef __APPLE__
    return update_open(info->page) ? 2 : 0;
#else
    if (!info || !make_package_url(info, url, sizeof url))
        return 0;
    sys_exe_path(exe, sizeof exe);
    sys_exe_dir(dir, sizeof dir);
    if (!*exe || !*dir || !update_parent(dir, parent, sizeof parent))
        return 0;
    sys_data_dir(data, sizeof data);
#ifndef _WIN32
    {
        char *absolute = realpath(data, NULL);
        if (!absolute || strlen(absolute) >= sizeof data) {
            free(absolute);
            return 0;
        }
        snprintf(data, sizeof data, "%s", absolute);
        free(absolute);
    }
#endif
    sys_join(package, sizeof package, data, "update-package.part");
    if (strlen(package) >= sizeof package - 1) return 0;
    remove(package);
    if (!download_package(url, package) || !package_hash_ok(package, info->sha256)) {
        remove(package);
        return 0;
    }
#ifdef _WIN32
    {
        char qexe[SYS_PATH * 2], qdir[SYS_PATH * 2], qpkg[SYS_PATH * 2];
        char ps[SYS_PATH * 8], command[SYS_PATH * 2];
        int n;
        sys_join(script, sizeof script, data, "update-apply.ps1");
        if (strlen(script) >= sizeof script - 1) { remove(package); return 0; }
        /* PowerShell single quoted strings escape an apostrophe by doubling it. */
        if (!powershell_quote(qexe, sizeof qexe, exe) ||
            !powershell_quote(qdir, sizeof qdir, dir) ||
            !powershell_quote(qpkg, sizeof qpkg, package)) {
            remove(package); return 0;
        }
        n = snprintf(ps, sizeof ps,
            "$ErrorActionPreference='Stop'; Wait-Process -Id %lu -ErrorAction SilentlyContinue; "
            "$exe=%s; $dir=%s; $zip=%s; $parent=Split-Path $dir -Parent; "
            "$stage=Join-Path $parent '.doskit-update-%lu'; "
            "$backup=Join-Path $parent '.doskit-previous-%lu'; "
            "if (Test-Path $stage) { exit 1 }; if (Test-Path $backup) { exit 1 }; "
            "Expand-Archive -LiteralPath $zip -DestinationPath $stage; "
            "$roots=@(Get-ChildItem -LiteralPath $stage -Directory); "
            "if ($roots.Count -ne 1) { exit 1 }; Set-Location $env:TEMP; "
            "Move-Item -LiteralPath $dir -Destination $backup; "
            "try { Move-Item -LiteralPath $roots[0].FullName -Destination $dir; "
            "Start-Process -FilePath $exe; Remove-Item -LiteralPath $backup -Recurse -Force } "
            "catch { if (!(Test-Path $dir) -and (Test-Path $backup)) { "
            "Move-Item -LiteralPath $backup -Destination $dir }; throw }; "
            "Remove-Item -LiteralPath $stage -Recurse -Force; "
            "Remove-Item -LiteralPath $zip -Force; Remove-Item -LiteralPath $PSCommandPath -Force",
            (unsigned long)GetCurrentProcessId(), qexe, qdir, qpkg,
            (unsigned long)GetCurrentProcessId(), (unsigned long)GetCurrentProcessId());
        if (n < 0 || (size_t)n >= sizeof ps) { remove(package); return 0; }
        f = fopen(script, "wb");
        if (!f) { remove(package); return 0; }
        if (fwrite(ps, 1, (size_t)n, f) != (size_t)n) {
            fclose(f); remove(script); remove(package); return 0;
        }
        if (fclose(f) != 0) { remove(script); remove(package); return 0; }
        n = snprintf(command, sizeof command,
                     "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\"",
                     script);
        if (n < 0 || (size_t)n >= sizeof command) {
            remove(script); remove(package); return 0;
        }
        if ((INT_PTR)ShellExecuteA(NULL, "open", "powershell.exe", command, NULL, SW_HIDE) <= 32) {
            remove(script); remove(package); return 0;
        }
        return 1;
    }
#else
    {
        char qexe[SYS_PATH * 2], qdir[SYS_PATH * 2], qpkg[SYS_PATH * 2];
        char qscript[SYS_PATH * 2], qstage[SYS_PATH * 2], qbackup[SYS_PATH * 2];
        char stage[SYS_PATH], backup[SYS_PATH], body[SYS_PATH * 14];
        char pid[32];
        char *argv[4];
        int body_len;
        pid_t child;
        sys_join(script, sizeof script, data, "update-apply.sh");
        if (strlen(script) >= sizeof script - 1) { remove(package); return 0; }
        if (snprintf(stage, sizeof stage, "%s/.doskit-update-%ld", parent,
                     (long)getpid()) >= (int)sizeof stage ||
            snprintf(backup, sizeof backup, "%s/.doskit-previous-%ld", parent,
                     (long)getpid()) >= (int)sizeof backup) {
            remove(package); return 0;
        }
        if (!shell_quote(qexe, sizeof qexe, exe) || !shell_quote(qdir, sizeof qdir, dir) ||
            !shell_quote(qpkg, sizeof qpkg, package) || !shell_quote(qscript, sizeof qscript, script) ||
            !shell_quote(qstage, sizeof qstage, stage) ||
            !shell_quote(qbackup, sizeof qbackup, backup)) {
            remove(package); return 0;
        }
        snprintf(pid, sizeof pid, "%ld", (long)getpid());
        body_len = snprintf(body, sizeof body,
            "#!/bin/sh\nwhile kill -0 %s 2>/dev/null; do sleep 1; done\n"
            "cd / || exit 1\n"
            "[ ! -e %s ] || exit 1\n"
            "mkdir %s || exit 1\n"
            "tar -xzf %s -C %s --strip-components=1 --no-same-owner || { rm -rf %s; exit 1; }\n"
            "mv %s %s || { rm -rf %s; exit 1; }\n"
            "mv %s %s || { mv %s %s; exit 1; }\n"
            "rm -rf %s\nchmod +x %s\n%s >/dev/null 2>&1 &\n"
            "rm -f %s %s\n",
            pid, qbackup, qstage, qpkg, qstage, qstage, qdir, qbackup, qstage, qstage,
            qdir, qbackup, qdir, qbackup, qexe, qexe, qpkg, qscript);
        if (body_len < 0 || (size_t)body_len >= sizeof body) { remove(package); return 0; }
        f = fopen(script, "wb");
        if (!f) { remove(package); return 0; }
        if (fwrite(body, 1, strlen(body), f) != strlen(body)) {
            fclose(f); remove(script); remove(package); return 0;
        }
        if (fclose(f) != 0) { remove(script); remove(package); return 0; }
        chmod(script, 0700);
        argv[0] = "sh"; argv[1] = script; argv[2] = NULL;
        child = spawn(argv);
        if (!child) { remove(script); remove(package); }
        return child != 0;
    }
#endif
#endif
}
