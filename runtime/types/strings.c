#pragma once
#include "../memory/core.c"

// ─────────────────────────────────────────
// FlowStr — stringa immutabile con ref count
// ─────────────────────────────────────────

typedef struct {
    FlowObject base;
    int32_t    len;
    char       data[];   // flessibile — i caratteri stanno qui
} FlowStr;

// Crea una nuova stringa da testo C
FlowStr* flow_str_new(const char* testo) {
    int32_t  len = (int32_t)strlen(testo);
    FlowStr* s   = (FlowStr*)flow_alloc(sizeof(FlowStr) + len + 1);
    s->len = len;
    memcpy(s->data, testo, len + 1);
    return s;
}

// Unisce due stringhe in una nuova
FlowStr* flow_str_concat(FlowStr* a, FlowStr* b) {
    int32_t  len = a->len + b->len;
    FlowStr* s   = (FlowStr*)flow_alloc(sizeof(FlowStr) + len + 1);
    s->len = len;
    memcpy(s->data, a->data, a->len);
    memcpy(s->data + a->len, b->data, b->len + 1);
    return s;
}

// Confronta due stringhe — 1 se uguali, 0 se diverse
int32_t flow_str_eq(FlowStr* a, FlowStr* b) {
    if (a->len != b->len) return 0;
    return memcmp(a->data, b->data, a->len) == 0 ? 1 : 0;
}

// Ritorna la lunghezza della stringa
int32_t flow_str_len(FlowStr* s) {
    return s->len;
}

