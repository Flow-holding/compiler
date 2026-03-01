#pragma once

/* Ritorna il path di flowc, NULL se non trovato (caller frees) */
char *get_flowc(void);

/* Esegue flowc in modalità client. server_fns: "fn1,fn2" per stubs JS. */
int run_flowc(const char *input, const char *outdir, const char *runtime,
              const char *flow_stdlib, int prod, int run, int dev, int fast,
              const char *server_fns);

/* Esegue flowc in modalità server. Genera server.c + server.exe */
int run_flowc_server(const char *input, const char *outdir, const char *runtime,
                     const char *flow_stdlib, int prod);
