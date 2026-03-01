#include "config.h"
#include "util.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *g_cli_runtime = NULL;
char *g_cli_flow    = NULL;

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

/* Ritorna l'ultimo segmento del path (nome cartella) */
static char *path_basename(const char *p) {
    const char *last = p;
    for (const char *s = p; *s; s++)
        if (*s == '/' || *s == '\\') last = s + 1;
    return str_dup(*last ? last : p);
}

int config_load(FlowConfig *cfg) {
    char *cwd  = get_cwd();
    char *path = path_join(cwd, "flow.config.json");
    char *json = read_file(path);
    free(path);

    /* flow.config.json è opzionale — se manca si usano i defaults */
    cfg->name    = json ? json_str(json, "name")    : NULL;
    cfg->entry   = json ? json_str(json, "entry")   : NULL;
    cfg->out     = json ? json_str(json, "out")     : NULL;
    cfg->runtime = json ? json_str(json, "runtime") : NULL;
    cfg->flow    = json ? json_str(json, "flow")    : NULL;
    free(json);

    if (!cfg->name)  cfg->name  = path_basename(cwd);
    if (!cfg->entry) cfg->entry = str_dup("client/root.flow");
    if (!cfg->out)   cfg->out   = str_dup("out");

    /* Override da CLI (--runtime, --flow) */
    if (g_cli_runtime) { free(cfg->runtime); cfg->runtime = str_dup(g_cli_runtime); }
    if (g_cli_flow)    { free(cfg->flow);    cfg->flow    = str_dup(g_cli_flow); }

    free(cwd);
    return 1;
}

void config_free(FlowConfig *cfg) {
    free(cfg->name);
    free(cfg->entry);
    free(cfg->out);
    free(cfg->runtime);
    free(cfg->flow);
}
