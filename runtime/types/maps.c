#pragma once
#include "../memory/core.c"

// ─────────────────────────────────────────
// FlowMap — dizionario chiave/valore
// Usa hashtable con chaining per collisioni
// ─────────────────────────────────────────

#define MAP_BUCKETS 64

typedef struct FlowMapEntry {
    char*              chiave;
    FlowObject*        valore;
    struct FlowMapEntry* prossimo;
} FlowMapEntry;

typedef struct {
    FlowObject   base;
    int32_t      len;
    FlowMapEntry* buckets[MAP_BUCKETS];
} FlowMap;

// Funzione hash semplice
static uint32_t hash(const char* chiave) {
    uint32_t h = 0;
    while (*chiave) {
        h = h * 31 + (unsigned char)*chiave++;
    }
    return h % MAP_BUCKETS;
}

// Crea una mappa vuota
FlowMap* flow_map_new() {
    FlowMap* m = (FlowMap*)flow_alloc(sizeof(FlowMap));
    m->len = 0;
    memset(m->buckets, 0, sizeof(m->buckets));
    return m;
}

// Inserisce o aggiorna una chiave
void flow_map_set(FlowMap* m, const char* chiave, FlowObject* valore) {
    uint32_t     idx   = hash(chiave);
    FlowMapEntry* entry = m->buckets[idx];

    // Cerca se la chiave esiste già
    while (entry) {
        if (strcmp(entry->chiave, chiave) == 0) {
            flow_release(entry->valore);
            flow_retain(valore);
            entry->valore = valore;
            return;
        }
        entry = entry->prossimo;
    }

    // Nuova entry
    FlowMapEntry* nuovo = (FlowMapEntry*)malloc(sizeof(FlowMapEntry));
    nuovo->chiave    = strdup(chiave);
    nuovo->valore    = valore;
    nuovo->prossimo  = m->buckets[idx];
    flow_retain(valore);
    m->buckets[idx] = nuovo;
    m->len++;
}

// Prende un valore per chiave — NULL se non esiste
FlowObject* flow_map_get(FlowMap* m, const char* chiave) {
    uint32_t     idx   = hash(chiave);
    FlowMapEntry* entry = m->buckets[idx];
    while (entry) {
        if (strcmp(entry->chiave, chiave) == 0) {
            return entry->valore;
        }
        entry = entry->prossimo;
    }
    return NULL;
}

// Controlla se una chiave esiste
int32_t flow_map_has(FlowMap* m, const char* chiave) {
    return flow_map_get(m, chiave) != NULL ? 1 : 0;
}

// Ritorna il numero di chiavi
int32_t flow_map_len(FlowMap* m) {
    return m->len;
}
