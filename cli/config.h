#pragma once

typedef struct {
    char *name;
    char *entry;
    char *out;
    char *runtime;
    char *flow;
} FlowConfig;

/* Override da CLI — impostati in main() prima di config_load() */
extern char *g_cli_runtime;
extern char *g_cli_flow;

/* Carica la config (flow.config.json opzionale). Sempre 1. */
int  config_load(FlowConfig *cfg);
void config_free(FlowConfig *cfg);
