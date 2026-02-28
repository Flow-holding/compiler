#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// ─────────────────────────────────────────
// Ogni oggetto Flow ha un reference counter.
// Quando arriva a zero → libera la memoria.
// ─────────────────────────────────────────

typedef struct {
    int32_t ref_count;
    size_t  size;
} FlowObject;

// Alloca un nuovo oggetto
FlowObject* flow_alloc(size_t size) {
    FlowObject* obj = (FlowObject*)malloc(sizeof(FlowObject) + size);
    if (!obj) {
        fprintf(stderr, "flow: out of memory\n");
        exit(1);
    }
    obj->ref_count = 1;
    obj->size      = size;
    return obj;
}

// Qualcuno in più usa questo oggetto
void flow_retain(FlowObject* obj) {
    if (obj) obj->ref_count++;
}

// Qualcuno smette di usare questo oggetto
void flow_release(FlowObject* obj) {
    if (!obj) return;
    obj->ref_count--;
    if (obj->ref_count <= 0) {
        free(obj);
    }
}

