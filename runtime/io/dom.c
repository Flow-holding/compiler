#pragma once
#include "../types/strings.c"

// ─────────────────────────────────────────
// DOM bridge — solo nel browser via WASM
// Genera HTML/CSS come stringhe
// Il compiler le inietta nel DOM tramite JS glue
// ─────────────────────────────────────────

// Struttura un nodo UI generico
typedef struct FlowNode {
    FlowObject    base;
    FlowStr*      tag;        // "div", "span", "button", ecc.
    FlowStr*      classe;     // classe CSS
    FlowStr*      testo;      // contenuto testo
    struct FlowNode** figli;  // nodi figli
    int32_t       n_figli;
} FlowNode;

// Crea un nodo con tag e classe
FlowNode* flow_node_new(const char* tag, const char* classe) {
    FlowNode* n  = (FlowNode*)flow_alloc(sizeof(FlowNode));
    n->tag       = flow_str_new(tag);
    n->classe    = flow_str_new(classe);
    n->testo     = flow_str_new("");
    n->figli     = NULL;
    n->n_figli   = 0;
    return n;
}

// Imposta il testo del nodo
void flow_node_text(FlowNode* n, const char* testo) {
    flow_release((FlowObject*)n->testo);
    n->testo = flow_str_new(testo);
}

// Aggiunge un figlio al nodo
void flow_node_append(FlowNode* genitore, FlowNode* figlio) {
    int32_t nuovo_n = genitore->n_figli + 1;
    FlowNode** nuovi = (FlowNode**)realloc(
        genitore->figli,
        nuovo_n * sizeof(FlowNode*)
    );
    flow_retain((FlowObject*)figlio);
    nuovi[genitore->n_figli] = figlio;
    genitore->figli  = nuovi;
    genitore->n_figli = nuovo_n;
}

// Serializza il nodo in HTML (ricorsivo)
// Il buffer deve essere pre-allocato abbastanza grande
static void flow_node_render_buf(FlowNode* n, char* buf, int32_t* pos) {
    // Apri tag
    *pos += sprintf(buf + *pos, "<%s", n->tag->data);
    if (n->classe->len > 0) {
        *pos += sprintf(buf + *pos, " class=\"%s\"", n->classe->data);
    }
    *pos += sprintf(buf + *pos, ">");

    // Testo
    if (n->testo->len > 0) {
        *pos += sprintf(buf + *pos, "%s", n->testo->data);
    }

    // Figli
    for (int32_t i = 0; i < n->n_figli; i++) {
        flow_node_render_buf(n->figli[i], buf, pos);
    }

    // Chiudi tag
    *pos += sprintf(buf + *pos, "</%s>", n->tag->data);
}

// Ritorna l'HTML del nodo come FlowStr
FlowStr* flow_node_render(FlowNode* n) {
    char    buf[65536] = {0};
    int32_t pos = 0;
    flow_node_render_buf(n, buf, &pos);
    return flow_str_new(buf);
}
