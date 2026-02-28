#pragma once
#include "../types/strings.c"

// ─────────────────────────────────────────
// File system — leggi e scrivi file
// Solo lato server, non nel browser
// ─────────────────────────────────────────

// Legge un file e ritorna il contenuto come FlowStr
// Ritorna NULL se il file non esiste
FlowStr* flow_fs_read(const char* percorso) {
    FILE* f = fopen(percorso, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    FlowStr* s = (FlowStr*)flow_alloc(sizeof(FlowStr) + size + 1);
    s->len = (int32_t)size;
    fread(s->data, 1, size, f);
    s->data[size] = '\0';
    fclose(f);
    return s;
}

// Scrive una stringa su file
// Ritorna 1 se ok, 0 se errore
int32_t flow_fs_write(const char* percorso, FlowStr* contenuto) {
    FILE* f = fopen(percorso, "wb");
    if (!f) return 0;
    fwrite(contenuto->data, 1, contenuto->len, f);
    fclose(f);
    return 1;
}

// Controlla se un file esiste
int32_t flow_fs_exists(const char* percorso) {
    FILE* f = fopen(percorso, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

// Cancella un file
int32_t flow_fs_delete(const char* percorso) {
    return remove(percorso) == 0 ? 1 : 0;
}
