#include "config.h"
#include "util.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Estrae il valore stringa di una chiave JSON — solo per stringhe semplici */
static char *json_str(const char *json, const char *key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p == ' ' || *p == '\t' || *p == ':' || *p == ' ') p++;
    if (*p != '"') return NULL;
    p++;
    const char *end = p;
    while (*end && *end != '"') {
        if (*end == '\\') end++;
        if (*end) end++;
    }
    size_t len = end - p;
    char *v = malloc(len + 1);
    memcpy(v, p, len);
    v[len] = 0;
    return v;
}

int config_load(FlowConfig *cfg) {
    char *cwd  = get_cwd();
    char *path = path_join(cwd, "flow.config.json");
    free(cwd);

    char *json = read_file(path);
    free(path);

    if (!json) {
        fprintf(stderr, C_RED "✗" C_RESET " flow.config.json non trovato"
                " — sei nella cartella del progetto?\n");
        return 0;
    }

    cfg->name  = json_str(json, "name");
    cfg->entry = json_str(json, "entry");
    cfg->out   = json_str(json, "out");
    free(json);

    if (!cfg->entry) cfg->entry = str_dup("src/main.flow");
    if (!cfg->out)   cfg->out   = str_dup("out");

    return 1;
}

void config_free(FlowConfig *cfg) {
    free(cfg->name);
    free(cfg->entry);
    free(cfg->out);
}
