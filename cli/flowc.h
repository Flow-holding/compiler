#pragma once

/* Ritorna il path di flowc, NULL se non trovato (caller frees) */
char *get_flowc(void);

/* Esegue flowc. dev=1: salta .exe; fast=1: solo HTML/CSS/JS. Ritorna 0 ok, 1 errore. */
int run_flowc(const char *input, const char *outdir, const char *runtime, const char *flow_stdlib, int prod, int run, int dev, int fast);
