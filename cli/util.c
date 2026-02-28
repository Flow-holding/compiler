#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #define getcwd    _getcwd
  #define stat      _stat
  #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <dirent.h>
  #ifdef __APPLE__
    #include <mach-o/dyld.h>
  #endif
#endif

/* ── path ─────────────────────────────────── */

char *path_join(const char *a, const char *b) {
    size_t la = strlen(a);
    while (la > 0 && (a[la-1] == '/' || a[la-1] == '\\')) la--;
    size_t lb = strlen(b);
    char *r = malloc(la + lb + 2);
    if (!r) return NULL;
    memcpy(r, a, la);
    r[la] = SEP_C;
    memcpy(r + la + 1, b, lb + 1);
    return r;
}

char *path_dirname(const char *path) {
    const char *last = NULL;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') last = p;
    if (!last) return str_dup(".");
    size_t len = last - path;
    char *d = malloc(len + 1);
    memcpy(d, path, len);
    d[len] = 0;
    return d;
}

int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int mkdir_p(const char *path) {
    char tmp[4096];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp; *p; p++)
        if (*p == '/') *p = SEP_C;
    if (len > 0 && tmp[len-1] == SEP_C) tmp[--len] = 0;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == SEP_C) {
            *p = 0;
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = SEP_C;
        }
    }
#ifdef _WIN32
    int r = _mkdir(tmp);
#else
    int r = mkdir(tmp, 0755);
#endif
    return (r == 0 || errno == EEXIST) ? 0 : -1;
}

/* ── file I/O ─────────────────────────────── */

int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(content, f);
    fclose(f);
    return 0;
}

char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    return buf;
}

/* ── process info ─────────────────────────── */

char *get_cwd(void) {
    char buf[4096];
    if (!getcwd(buf, sizeof(buf))) return NULL;
    return str_dup(buf);
}

char *get_flow_home(void) {
#ifdef _WIN32
    const char *base = getenv("LOCALAPPDATA");
    if (!base) base = getenv("USERPROFILE");
    if (!base) base = ".";
    return str_fmt("%s\\.flow", base);
#else
    const char *base = getenv("HOME");
    if (!base) base = ".";
    return str_fmt("%s/.flow", base);
#endif
}

int get_exe_path(char *buf, size_t len) {
#ifdef _WIN32
    return GetModuleFileNameA(NULL, buf, (DWORD)len) > 0 ? 0 : -1;
#elif defined(__APPLE__)
    uint32_t sz = (uint32_t)len;
    return _NSGetExecutablePath(buf, &sz) == 0 ? 0 : -1;
#else
    ssize_t n = readlink("/proc/self/exe", buf, len - 1);
    if (n < 0) return -1;
    buf[n] = 0;
    return 0;
#endif
}

/* ── strings ──────────────────────────────── */

char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

char *str_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = malloc(n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

char *str_replace(const char *src, const char *from, const char *to) {
    size_t flen = strlen(from), tlen = strlen(to), count = 0;
    const char *p = src;
    while ((p = strstr(p, from))) { count++; p += flen; }
    char *result = malloc(strlen(src) + count * (tlen + 1 - flen) + 1);
    if (!result) return NULL;
    char *out = result;
    p = src;
    const char *m;
    while ((m = strstr(p, from))) {
        size_t pre = m - p;
        memcpy(out, p, pre); out += pre;
        memcpy(out, to, tlen); out += tlen;
        p = m + flen;
    }
    strcpy(out, p);
    return result;
}

/* ── file system ──────────────────────────── */

long long file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long long)st.st_mtime * 1000LL;
}

static void glob_recurse(const char *dir, char ***out, int *n, int *cap) {
#ifdef _WIN32
    char pattern[4096];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        char *full = str_fmt("%s\\%s", dir, fd.cFileName);
        if (!full) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            glob_recurse(full, out, n, cap); free(full);
        } else {
            size_t l = strlen(fd.cFileName);
            if (l > 5 && !strcmp(fd.cFileName + l - 5, ".flow")) {
                if (*n >= *cap) { *cap = *cap * 2 + 8; *out = realloc(*out, *cap * sizeof(char*)); }
                (*out)[(*n)++] = full;
            } else free(full);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char *full = str_fmt("%s/%s", dir, e->d_name);
        if (!full) continue;
        struct stat st;
        if (stat(full, &st) != 0) { free(full); continue; }
        if (S_ISDIR(st.st_mode)) {
            glob_recurse(full, out, n, cap); free(full);
        } else {
            size_t l = strlen(e->d_name);
            if (l > 5 && !strcmp(e->d_name + l - 5, ".flow")) {
                if (*n >= *cap) { *cap = *cap * 2 + 8; *out = realloc(*out, *cap * sizeof(char*)); }
                (*out)[(*n)++] = full;
            } else free(full);
        }
    }
    closedir(d);
#endif
}

char **glob_flow(const char *dir, int *n) {
    int cap = 16;
    char **files = malloc(cap * sizeof(char*));
    *n = 0;
    if (files) glob_recurse(dir, &files, n, &cap);
    return files;
}

/* ── subprocess ───────────────────────────── */

int run_cmd(const char **argv) {
#ifdef _WIN32
    char cmd[8192] = {0};
    for (int i = 0; argv[i]; i++) {
        if (i > 0) strcat(cmd, " ");
        if (strchr(argv[i], ' ')) {
            strcat(cmd, "\""); strcat(cmd, argv[i]); strcat(cmd, "\"");
        } else {
            strcat(cmd, argv[i]);
        }
    }
    return system(cmd);
#else
    pid_t pid = fork();
    if (pid == 0) { execvp(argv[0], (char *const*)argv); _exit(127); }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}
