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

int run_flowc(const char *input, const char *outdir, const char *runtime,
              const char *flow_stdlib, int prod, int run_flag, int dev, int fast,
              const char *server_fns) {
    char *flowc = get_flowc();
    if (!flowc) {
        fprintf(stderr, C_RED "✗" C_RESET " flowc non trovato"
                " — esegui prima: flow update\n");
        return 1;
    }

    char srv_fns_arg[4096] = {0};
    if (server_fns && server_fns[0])
        snprintf(srv_fns_arg, sizeof(srv_fns_arg), "--server-fns=%s", server_fns);

    const char *argv[32];
    int i = 0;
    argv[i++] = flowc;
    argv[i++] = input;
    argv[i++] = "--outdir";
    argv[i++] = outdir;
    if (runtime && runtime[0]) {
        argv[i++] = "--runtime";
        argv[i++] = runtime;
    }
    if (flow_stdlib && flow_stdlib[0]) {
        argv[i++] = "--flow";
        argv[i++] = flow_stdlib;
    }
    if (prod)     argv[i++] = "--prod";
    if (run_flag) argv[i++] = "--run";
    if (dev)      argv[i++] = "--dev";
    if (fast)     argv[i++] = "--fast";
    if (srv_fns_arg[0]) argv[i++] = srv_fns_arg;
    argv[i]   = NULL;

    int code = run_cmd(argv);
    free(flowc);
    return code;
}

int run_flowc_route(const char *input, const char *outdir, const char *runtime,
                    const char *flow_stdlib) {
    char *flowc = get_flowc();
    if (!flowc) {
        fprintf(stderr, C_RED "✗" C_RESET " flowc non trovato\n");
        return 1;
    }

    const char *argv[24];
    int i = 0;
    argv[i++] = flowc;
    argv[i++] = input;
    argv[i++] = "--outdir";
    argv[i++] = outdir;
    argv[i++] = "--outlet";
    if (runtime && runtime[0])         { argv[i++] = "--runtime"; argv[i++] = runtime;    }
    if (flow_stdlib && flow_stdlib[0]) { argv[i++] = "--flow";    argv[i++] = flow_stdlib; }
    argv[i] = NULL;

    int code = run_cmd(argv);
    free(flowc);
    return code;
}

int run_flowc_server(const char *input, const char *outdir, const char *runtime,
                     const char *flow_stdlib, int prod) {
    char *flowc = get_flowc();
    if (!flowc) {
        fprintf(stderr, C_RED "✗" C_RESET " flowc non trovato\n");
        return 1;
    }

    const char *argv[24];
    int i = 0;
    argv[i++] = flowc;
    argv[i++] = input;
    argv[i++] = "--outdir";
    argv[i++] = outdir;
    argv[i++] = "--server";
    if (runtime && runtime[0]) { argv[i++] = "--runtime"; argv[i++] = runtime; }
    if (flow_stdlib && flow_stdlib[0]) { argv[i++] = "--flow"; argv[i++] = flow_stdlib; }
    if (prod) argv[i++] = "--prod";
    argv[i] = NULL;

    int code = run_cmd(argv);
    free(flowc);
    return code;
}
