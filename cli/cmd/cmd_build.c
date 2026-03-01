#include "cmd_build.h"
#include "../config.h"
#include "../flowc.h"
#include "../util.h"
#include "../cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Scansiona server/functions/ e raccoglie i nomi delle server functions ──
// Cerca pattern "export <name> = input(" nelle prime righe di ogni .flow file
// Risultato: "fn1,fn2,fn3" in out_buf (caller passa buffer)
static void collect_server_fns(const char *srv_dir, char *out_buf, int out_sz) {
    out_buf[0] = '\0';
    int n = 0;
    char **files = glob_flow(srv_dir, &n);
    if (!files) return;

    int first = 1;
    for (int i = 0; i < n; i++) {
        char *src = read_file(files[i]);
        if (!src) { free(files[i]); continue; }

        // Scansione semplice: cerca "export " poi identif poi " = input("
        const char *p = src;
        while ((p = strstr(p, "export ")) != NULL) {
            p += 7;
            while (*p == ' ') p++;
            // leggi nome
            const char *name_start = p;
            while (*p && (*p == '_' || isalnum((unsigned char)*p))) p++;
            int name_len = (int)(p - name_start);
            if (name_len == 0) continue;
            while (*p == ' ') p++;
            if (*p != '=') continue;
            p++;
            while (*p == ' ') p++;
            if (strncmp(p, "input(", 6) != 0) continue;

            // Trovata server function
            char name[128] = {0};
            if (name_len >= (int)sizeof(name)) name_len = (int)sizeof(name) - 1;
            memcpy(name, name_start, name_len);
            name[name_len] = '\0';

            int cur_len = (int)strlen(out_buf);
            if (!first && cur_len + 1 < out_sz) { out_buf[cur_len++] = ','; out_buf[cur_len] = '\0'; }
            strncat(out_buf, name, out_sz - cur_len - 1);
            first = 0;
        }
        free(src);
        free(files[i]);
    }
    free(files);
}

int cmd_build(int prod, int run_flag, int dev, int fast) {
    FlowConfig cfg = {0};
    if (!config_load(&cfg)) return 1;

    int code = 0;

    char *cwd         = get_cwd();
    char *entry       = path_join(cwd, cfg.entry);
    char *outdir      = path_join(cwd, cfg.out);
    char *runtime     = cfg.runtime ? path_join(cwd, cfg.runtime) : NULL;
    char *flow_stdlib = cfg.flow    ? path_join(cwd, cfg.flow)    : NULL;

    if (!path_exists(entry)) {
        fprintf(stderr, C_RED "✗" C_RESET " entry point non trovato: %s\n", entry);
        code = 1;
        goto cleanup;
    }

    mkdir_p(outdir);

    // ── 1. Build server (se esiste server/functions/) ─────────────
    char srv_fns[4096] = {0};
    char *srv_fns_dir = path_join(cwd, "server" SEP "functions");

    if (path_exists(srv_fns_dir)) {
        // Raccogli nomi server functions per stubs JS
        collect_server_fns(srv_fns_dir, srv_fns, sizeof(srv_fns));

        // Trova entry server (server/functions/main.flow o primo file)
        int srv_n = 0;
        char **srv_files = glob_flow(srv_fns_dir, &srv_n);
        if (srv_files && srv_n > 0) {
            // Cerca main.flow, altrimenti usa il primo file
            char *srv_entry = NULL;
            for (int i = 0; i < srv_n; i++) {
                if (strstr(srv_files[i], "main.flow")) { srv_entry = srv_files[i]; break; }
            }
            if (!srv_entry) srv_entry = srv_files[0];

            printf(C_FLOW "▲" C_RESET " Building server...\n");
            int srv_rc = run_flowc_server(srv_entry, outdir, runtime, flow_stdlib, prod);
            if (srv_rc != 0)
                fprintf(stderr, C_RED "✗" C_RESET " Server build fallita\n");

            for (int i = 0; i < srv_n; i++) free(srv_files[i]);
            free(srv_files);
        }
    }
    free(srv_fns_dir);

    // ── 2. Build client ───────────────────────────────────────────
    code = run_flowc(entry, outdir, runtime, flow_stdlib,
                     prod, run_flag, dev, fast, srv_fns[0] ? srv_fns : NULL);

cleanup:
    free(entry); free(outdir);
    if (runtime)     free(runtime);
    if (flow_stdlib) free(flow_stdlib);
    free(cwd);
    config_free(&cfg);
    return code;
}
