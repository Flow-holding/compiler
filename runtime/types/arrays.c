#pragma once
#include "../memory/core.c"

// ─────────────────────────────────────────
// FlowArray — array dinamico con ref count
// Cresce automaticamente quando serve
// ─────────────────────────────────────────

typedef struct {
    FlowObject  base;
    int32_t     len;       // elementi attuali
    int32_t     cap;       // capacità allocata
    FlowObject* items[];   // puntatori agli elementi
} FlowArray;

// Crea un array vuoto con capacità iniziale
FlowArray* flow_array_new() {
    int32_t    cap = 8;
    FlowArray* arr = (FlowArray*)flow_alloc(
        sizeof(FlowArray) + cap * sizeof(FlowObject*)
    );
    arr->len = 0;
    arr->cap = cap;
    return arr;
}

// Aggiunge un elemento in fondo
FlowArray* flow_array_push(FlowArray* arr, FlowObject* item) {
    if (arr->len >= arr->cap) {
        // Raddoppia la capacità
        int32_t    new_cap = arr->cap * 2;
        FlowArray* new_arr = (FlowArray*)flow_alloc(
            sizeof(FlowArray) + new_cap * sizeof(FlowObject*)
        );
        new_arr->len = arr->len;
        new_arr->cap = new_cap;
        memcpy(new_arr->items, arr->items,
               arr->len * sizeof(FlowObject*));
        flow_release((FlowObject*)arr);
        arr = new_arr;
    }
    flow_retain(item);
    arr->items[arr->len++] = item;
    return arr;
}

// Prende elemento per indice
FlowObject* flow_array_get(FlowArray* arr, int32_t indice) {
    if (indice < 0 || indice >= arr->len) {
        fprintf(stderr, "flow: index %d out of bounds (len=%d)\n",
                indice, arr->len);
        exit(1);
    }
    return arr->items[indice];
}

// Ritorna la lunghezza
int32_t flow_array_len(FlowArray* arr) {
    return arr->len;
}
