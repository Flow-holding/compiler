#pragma once
#include "../types/strings.c"

// ─────────────────────────────────────────
// Output — stampa a schermo
// ─────────────────────────────────────────

// Stampa stringa Flow con newline
void flow_print(FlowStr* s) {
    printf("%s\n", s->data);
}

// Stampa numero intero
void flow_print_int(int32_t n) {
    printf("%d\n", n);
}

// Stampa numero decimale
void flow_print_float(double n) {
    printf("%g\n", n);
}

// Stampa booleano
void flow_print_bool(int32_t b) {
    printf("%s\n", b ? "true" : "false");
}

// Stampa su stderr (per errori)
void flow_eprint(FlowStr* s) {
    fprintf(stderr, "%s\n", s->data);
}
