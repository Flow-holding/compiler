#include "cmd_build.h"
#include "config.h"
#include "flowc.h"
#include "util.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>

int cmd_build(int prod, int run_flag) {
    FlowConfig cfg = {0};
    if (!config_load(&cfg)) return 1;

    char *cwd    = get_cwd();
    char *entry  = path_join(cwd, cfg.entry);
    char *outdir = path_join(cwd, cfg.out);

    if (!path_exists(entry)) {
        fprintf(stderr, C_RED "✗" C_RESET " entry point non trovato: %s\n", entry);
        free(entry); free(outdir); free(cwd);
        config_free(&cfg);
        return 1;
    }

    mkdir_p(outdir);

    int code = run_flowc(entry, outdir, prod, run_flag);

    free(entry); free(outdir); free(cwd);
    config_free(&cfg);
    return code;
}
