#pragma once
#include <stddef.h>

#ifdef _WIN32
  #define SEP   "\\"
  #define SEP_C '\\'
#else
  #define SEP   "/"
  #define SEP_C '/'
#endif

/* Path */
char      *path_join(const char *a, const char *b);     /* caller frees */
char      *path_dirname(const char *path);              /* caller frees */
int        path_exists(const char *path);
int        mkdir_p(const char *path);

/* File I/O */
int        write_file(const char *path, const char *content);
char      *read_file(const char *path);                 /* caller frees */

/* Process info */
char      *get_cwd(void);                               /* caller frees */
char      *get_flow_home(void);                         /* caller frees */
int        get_exe_path(char *buf, size_t len);

/* Strings */
char      *str_dup(const char *s);
char      *str_fmt(const char *fmt, ...);               /* caller frees */
char      *str_replace(const char *s, const char *from, const char *to); /* caller frees */

/* File system */
long long  file_mtime(const char *path);                /* ms epoch, -1 on error */
char     **glob_flow(const char *dir, int *n);          /* caller frees each + array */

/* Subprocess — returns exit code */
int        run_cmd(const char **argv);
