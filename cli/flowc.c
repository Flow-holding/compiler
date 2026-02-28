#include "flowc.h"
#include "util.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *get_flowc(void) {
    char exe[4096];

    /* 1. Stessa cartella del binario corrente */
    if (get_exe_path(exe, sizeof(exe)) == 0) {
        char *dir   = path_dirname(exe);
        char *flowc = str_fmt("%s" SEP "flowc" EXE_EXT, dir);
        free(dir);
        if (path_exists(flowc)) return flowc;
        free(flowc);
    }

    /* 2. ~/.flow/bin/flowc */
    char *home  = get_flow_home();
    char *flowc = str_fmt("%s" SEP "bin" SEP "flowc" EXE_EXT, home);
    free(home);
    if (path_exists(flowc)) return flowc;
    free(flowc);

    return NULL;
}

int run_flowc(const char *input, const char *outdir, const char *runtime, int prod, int run_flag) {
    char *flowc = get_flowc();
    if (!flowc) {
        fprintf(stderr, C_RED "✗" C_RESET " flowc non trovato"
                " — esegui prima: flow update\n");
        return 1;
    }

    const char *argv[20];
    int i = 0;
    argv[i++] = flowc;
    argv[i++] = input;
    argv[i++] = "--outdir";
    argv[i++] = outdir;
    if (runtime && runtime[0]) {
        argv[i++] = "--runtime";
        argv[i++] = runtime;
    }
    if (prod)     argv[i++] = "--prod";
    if (run_flag) argv[i++] = "--run";
    argv[i]   = NULL;

    int code = run_cmd(argv);
    free(flowc);
    return code;
}
