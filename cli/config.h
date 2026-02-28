#pragma once

typedef struct {
    char *name;
    char *entry;
    char *out;
} FlowConfig;

/* Legge flow.config.json dalla cwd. Ritorna 1 ok, 0 errore. */
int  config_load(FlowConfig *cfg);
void config_free(FlowConfig *cfg);
