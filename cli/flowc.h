#pragma once

/* Ritorna il path di flowc, NULL se non trovato (caller frees) */
char *get_flowc(void);

/* Esegue flowc su un file .flow. Ritorna 0 ok, 1 errore. */
int run_flowc(const char *input, const char *outdir, const char *runtime, int prod, int run);
