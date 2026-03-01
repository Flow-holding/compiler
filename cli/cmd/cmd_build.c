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
            if (srv_rc != 0) {
                fprintf(stderr, C_RED "⚠" C_RESET " Server build saltata (flowc aggiornato?)\n");
                srv_fns[0] = '\0';  // non iniettare stubs se server non compilato
            }

            for (int i = 0; i < srv_n; i++) free(srv_files[i]);
            free(srv_files);
        }
    }
    free(srv_fns_dir);

    // ── 2. Build client (root.flow → index.html / app.js / app.wasm) ────────
    code = run_flowc(entry, outdir, runtime, flow_stdlib,
                     prod, run_flag, dev, fast, srv_fns[0] ? srv_fns : NULL);

    // ── 3. Compila route e inietta in #fl-outlet ──────────────────────────
    // Scansiona client/routes/*.flow — la prima route trovata (index.flow
    // o il primo file) viene pre-renderizzata nell'outlet per SSG-like output.
    if (code == 0) {
        char *routes_dir = path_join(cwd, "client" SEP "routes");
        if (path_exists(routes_dir)) {
            int rn = 0;
            char **rfiles = glob_flow(routes_dir, &rn);
            if (rfiles && rn > 0) {
                // Preferisci index.flow, altrimenti il primo file
                char *route_entry = NULL;
                for (int i = 0; i < rn; i++) {
                    const char *base = strrchr(rfiles[i], SEP[0]);
                    if (!base) base = rfiles[i]; else base++;
                    if (strcmp(base, "index.flow") == 0) { route_entry = rfiles[i]; break; }
                }
                if (!route_entry) route_entry = rfiles[0];

                // Compila route → STEM.outlet.html
                if (run_flowc_route(route_entry, outdir, runtime, flow_stdlib) == 0) {
                    // Ricava il path dell'outlet generato
                    const char *base = strrchr(route_entry, SEP[0]);
                    if (!base) base = route_entry; else base++;
                    char stem[256] = {0};
                    strncpy(stem, base, sizeof(stem) - 1);
                    char *dot = strrchr(stem, '.');
                    if (dot) *dot = '\0';

                    char outlet_path[4096];
                    snprintf(outlet_path, sizeof(outlet_path),
                             "%s" SEP "%s.outlet.html", outdir, stem);

                    char *outlet_html = read_file(outlet_path);
                    if (outlet_html) {
                        // Leggi index.html e sostituisci placeholder outlet
                        char main_path[4096];
                        snprintf(main_path, sizeof(main_path), "%s" SEP "index.html", outdir);
                        char *main_html = read_file(main_path);
                        if (main_html) {
                            const char *needle = "<div id=\"fl-outlet\"></div>";
                            char *pos = strstr(main_html, needle);
                            if (pos) {
                                size_t prefix  = (size_t)(pos - main_html);
                                size_t needle_len = strlen(needle);
                                size_t outlet_len = strlen(outlet_html);
                                size_t suffix_len = strlen(pos + needle_len);
                                // "<div id="fl-outlet">" + outlet + "</div>"
                                const char *open  = "<div id=\"fl-outlet\">";
                                const char *close = "</div>";
                                size_t new_len = prefix + strlen(open) + outlet_len
                                                + strlen(close) + suffix_len + 1;
                                char *new_html = (char*)malloc(new_len);
                                if (new_html) {
                                    memcpy(new_html, main_html, prefix);
                                    char *p = new_html + prefix;
                                    memcpy(p, open,        strlen(open));  p += strlen(open);
                                    memcpy(p, outlet_html, outlet_len);    p += outlet_len;
                                    memcpy(p, close,       strlen(close)); p += strlen(close);
                                    memcpy(p, pos + needle_len, suffix_len + 1);
                                    write_file(main_path, new_html);
                                    free(new_html);
                                }
                            }
                            free(main_html);
                        }
                        free(outlet_html);
                    }
                }

                for (int i = 0; i < rn; i++) free(rfiles[i]);
                free(rfiles);
            }
        }
        free(routes_dir);
    }

cleanup:
    free(entry); free(outdir);
    if (runtime)     free(runtime);
    if (flow_stdlib) free(flow_stdlib);
    free(cwd);
    config_free(&cfg);
    return code;
}
