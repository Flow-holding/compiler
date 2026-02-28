// AUTO-GENERATED — non modificare manualmente
// Generato da scripts/bundle-runtime.ts
// Contiene 11 file C del runtime Flow

export const RUNTIME_BUNDLE: Record<string, string> = {
    "io/dom.c": `#pragma once
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
        *pos += sprintf(buf + *pos, " class=\\"%s\\"", n->classe->data);
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
`,

    "io/fs.c": `#pragma once
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
    s->data[size] = '\\0';
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
`,

    "io/print.c": `#pragma once
#include "../types/strings.c"

// ─────────────────────────────────────────
// Output — stampa a schermo
// ─────────────────────────────────────────

// Stampa stringa Flow con newline
void flow_print(FlowStr* s) {
    printf("%s\\n", s->data);
}

// Stampa numero intero
void flow_print_int(int32_t n) {
    printf("%d\\n", n);
}

// Stampa numero decimale
void flow_print_float(double n) {
    printf("%g\\n", n);
}

// Stampa booleano
void flow_print_bool(int32_t b) {
    printf("%s\\n", b ? "true" : "false");
}

// Stampa su stderr (per errori)
void flow_eprint(FlowStr* s) {
    fprintf(stderr, "%s\\n", s->data);
}
`,

    "memory/core.c": `#pragma once
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
        fprintf(stderr, "flow: out of memory\\n");
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

`,

    "net/http.c": `#pragma once
#include "../types/strings.c"

// ─────────────────────────────────────────
// HTTP — server e client minimali
// Usa le socket standard POSIX / Winsock
// ─────────────────────────────────────────

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET flow_socket_t;
    #define FLOW_INVALID_SOCKET INVALID_SOCKET
    #define flow_close_socket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int flow_socket_t;
    #define FLOW_INVALID_SOCKET -1
    #define flow_close_socket close
#endif

// Struttura per una richiesta HTTP
typedef struct {
    FlowObject base;
    FlowStr*   metodo;    // GET, POST, ecc.
    FlowStr*   percorso;  // /api/utenti
    FlowStr*   body;      // corpo della richiesta
} FlowRequest;

// Struttura per una risposta HTTP
typedef struct {
    FlowObject base;
    int32_t    status;    // 200, 404, ecc.
    FlowStr*   body;      // corpo della risposta
    FlowStr*   tipo;      // Content-Type
} FlowResponse;

// Crea una risposta
FlowResponse* flow_http_response(int32_t status,
                                  const char* body,
                                  const char* tipo) {
    FlowResponse* r = (FlowResponse*)flow_alloc(sizeof(FlowResponse));
    r->status = status;
    r->body   = flow_str_new(body);
    r->tipo   = flow_str_new(tipo);
    return r;
}

// Inizializza le socket (necessario su Windows)
static void flow_net_init() {
    #ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    #endif
}

// Avvia un server HTTP sulla porta specificata
// handler: funzione chiamata per ogni richiesta
void flow_http_serve(int32_t porta,
                     FlowResponse* (*handler)(FlowRequest*)) {
    flow_net_init();

    flow_socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == FLOW_INVALID_SOCKET) {
        fprintf(stderr, "flow: impossibile creare socket\\n");
        return;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)porta);

    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "flow: bind fallito sulla porta %d\\n", porta);
        flow_close_socket(server);
        return;
    }

    listen(server, 10);
    printf("flow: server in ascolto su http://localhost:%d\\n", porta);

    while (1) {
        flow_socket_t client = accept(server, NULL, NULL);
        if (client == FLOW_INVALID_SOCKET) continue;

        // Leggi la richiesta
        char buf[4096] = {0};
        recv(client, buf, sizeof(buf) - 1, 0);

        // Parsing minimo — estrai metodo e percorso
        FlowRequest* req = (FlowRequest*)flow_alloc(sizeof(FlowRequest));
        char metodo[16] = {0}, percorso[256] = {0};
        sscanf(buf, "%15s %255s", metodo, percorso);
        req->metodo   = flow_str_new(metodo);
        req->percorso = flow_str_new(percorso);
        req->body     = flow_str_new("");

        // Chiama l'handler
        FlowResponse* res = handler(req);

        // Invia la risposta
        char header[512];
        snprintf(header, sizeof(header),
            "HTTP/1.1 %d OK\\r\\n"
            "Content-Type: %s\\r\\n"
            "Content-Length: %d\\r\\n"
            "Connection: close\\r\\n\\r\\n",
            res->status, res->tipo->data, res->body->len);

        send(client, header, (int)strlen(header), 0);
        send(client, res->body->data, res->body->len, 0);

        flow_close_socket(client);
        flow_release((FlowObject*)req);
        flow_release((FlowObject*)res);
    }

    flow_close_socket(server);
}
`,

    "net/websocket.c": `#pragma once
#include "../types/strings.c"

// ─────────────────────────────────────────
// WebSocket — connessioni real-time
// Implementazione base del protocollo WS
// ─────────────────────────────────────────

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET flow_socket_t;
    #define flow_close_socket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    typedef int flow_socket_t;
    #define flow_close_socket close
#endif

#include <stdint.h>

// Struttura connessione WebSocket
typedef struct {
    FlowObject    base;
    flow_socket_t socket;
    int32_t       connesso;
} FlowWebSocket;

// Invia un messaggio di testo via WebSocket
// Frame WS: opcode 0x81 (text), payload length, data
int32_t flow_ws_send(FlowWebSocket* ws, FlowStr* messaggio) {
    if (!ws->connesso) return 0;

    int32_t len = messaggio->len;
    uint8_t header[10];
    int32_t header_len;

    header[0] = 0x81; // FIN + opcode text

    if (len <= 125) {
        header[1]  = (uint8_t)len;
        header_len = 2;
    } else if (len <= 65535) {
        header[1]  = 126;
        header[2]  = (len >> 8) & 0xFF;
        header[3]  = len & 0xFF;
        header_len = 4;
    } else {
        return 0; // messaggi > 64KB non supportati ora
    }

    send(ws->socket, (const char*)header, header_len, 0);
    send(ws->socket, messaggio->data, len, 0);
    return 1;
}

// Chiude la connessione
void flow_ws_close(FlowWebSocket* ws) {
    if (ws->connesso) {
        // Frame di chiusura WS
        uint8_t close_frame[] = {0x88, 0x00};
        send(ws->socket, (const char*)close_frame, 2, 0);
        flow_close_socket(ws->socket);
        ws->connesso = 0;
    }
}
`,

    "types/arrays.c": `#pragma once
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
        fprintf(stderr, "flow: index %d out of bounds (len=%d)\\n",
                indice, arr->len);
        exit(1);
    }
    return arr->items[indice];
}

// Ritorna la lunghezza
int32_t flow_array_len(FlowArray* arr) {
    return arr->len;
}
`,

    "types/maps.c": `#pragma once
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
`,

    "types/strings.c": `#pragma once
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

`,

    "wasm/runtime.c": `#pragma once
#include "stdlib.c"

// ── Tipi Flow per WASM ───────────────────────────────────
// Riadatta il runtime senza dipendenze da libc

typedef struct {
    int32_t ref_count;
    size_t  size;
} FlowObject;

FlowObject* flow_alloc(size_t size) {
    FlowObject* obj = (FlowObject*)malloc(sizeof(FlowObject) + size);
    if (!obj) { flow_js_eprint("flow: out of memory"); exit(1); }
    obj->ref_count = 1;
    obj->size      = size;
    return obj;
}

void flow_retain(FlowObject* obj) {
    if (obj) obj->ref_count++;
}

void flow_release(FlowObject* obj) {
    if (!obj) return;
    obj->ref_count--;
    // bump allocator — non liberiamo davvero
}

// ── FlowStr ──────────────────────────────────────────────
typedef struct {
    FlowObject base;
    int32_t    len;
    char       data[];
} FlowStr;

FlowStr* flow_str_new(const char* testo) {
    int32_t  len = (int32_t)strlen(testo);
    FlowStr* s   = (FlowStr*)flow_alloc(sizeof(FlowStr) + len + 1);
    s->len = len;
    memcpy(s->data, testo, len + 1);
    return s;
}

FlowStr* flow_str_concat(FlowStr* a, FlowStr* b) {
    int32_t  len = a->len + b->len;
    FlowStr* s   = (FlowStr*)flow_alloc(sizeof(FlowStr) + len + 1);
    s->len = len;
    memcpy(s->data, a->data, a->len);
    memcpy(s->data + a->len, b->data, b->len + 1);
    return s;
}

int32_t flow_str_eq(FlowStr* a, FlowStr* b) {
    if (a->len != b->len) return 0;
    return memcmp(a->data, b->data, a->len) == 0 ? 1 : 0;
}

int32_t flow_str_len(FlowStr* s) { return s->len; }

// ── Print → JS ───────────────────────────────────────────
void flow_print(FlowStr* s)      { flow_js_print(s->data); }
void flow_print_int(int32_t n) {
    char buf[20]; __itoa(n, buf); flow_js_print(buf);
}
void flow_print_float(double n)  { flow_js_print("(float)"); }
void flow_print_bool(int32_t b)  { flow_js_print(b ? "true" : "false"); }
void flow_eprint(FlowStr* s)     { flow_js_eprint(s->data); }

// ── FlowArray ────────────────────────────────────────────
typedef struct {
    FlowObject  base;
    int32_t     len;
    int32_t     cap;
    FlowObject* items[];
} FlowArray;

FlowArray* flow_array_new() {
    int32_t    cap = 8;
    FlowArray* arr = (FlowArray*)flow_alloc(sizeof(FlowArray) + cap * sizeof(FlowObject*));
    arr->len = 0; arr->cap = cap;
    return arr;
}

FlowArray* flow_array_push(FlowArray* arr, FlowObject* item) {
    if (arr->len >= arr->cap) {
        int32_t    new_cap = arr->cap * 2;
        FlowArray* new_arr = (FlowArray*)flow_alloc(sizeof(FlowArray) + new_cap * sizeof(FlowObject*));
        new_arr->len = arr->len; new_arr->cap = new_cap;
        memcpy(new_arr->items, arr->items, arr->len * sizeof(FlowObject*));
        arr = new_arr;
    }
    flow_retain(item);
    arr->items[arr->len++] = item;
    return arr;
}

FlowObject* flow_array_get(FlowArray* arr, int32_t i) {
    if (i < 0 || i >= arr->len) { flow_js_eprint("flow: index out of bounds"); exit(1); }
    return arr->items[i];
}

int32_t flow_array_len(FlowArray* arr) { return arr->len; }
`,

    "wasm/stdlib.c": `#pragma once

// ── Tipi base ────────────────────────────────────────────
typedef unsigned int   size_t;
typedef int            int32_t;
typedef unsigned int   uint32_t;
typedef unsigned char  uint8_t;
typedef long long      int64_t;

// ── Heap — bump allocator 4MB ────────────────────────────
#define HEAP_SIZE (4 * 1024 * 1024)
static char __heap[HEAP_SIZE];
static int  __heap_ptr = 0;

void* malloc(size_t size) {
    size = (size + 7) & ~7;  // allineamento 8 byte
    if (__heap_ptr + (int)size > HEAP_SIZE) return 0;
    void* ptr = &__heap[__heap_ptr];
    __heap_ptr += (int)size;
    return ptr;
}

void free(void* ptr) { /* bump allocator — nessuna liberazione */ }

void* realloc(void* ptr, size_t size) {
    void* new_ptr = malloc(size);
    return new_ptr;
}

// ── String ───────────────────────────────────────────────
size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void* memcpy(void* dst, const void* src, size_t n) {
    char* d = (char*)dst;
    const char* s = (const char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* memset(void* dst, int c, size_t n) {
    char* d = (char*)dst;
    for (size_t i = 0; i < n; i++) d[i] = (char)c;
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char* p = (const unsigned char*)a;
    const unsigned char* q = (const unsigned char*)b;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != q[i]) return p[i] - q[i];
    }
    return 0;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

// ── Print → JS import ────────────────────────────────────
// Queste funzioni sono fornite dal loader JS (app.js)
__attribute__((import_module("env"), import_name("flow_print_str")))
extern void flow_js_print(const char* s);

__attribute__((import_module("env"), import_name("flow_eprint_str")))
extern void flow_js_eprint(const char* s);

// printf minimale — solo %s e %d
static char __printf_buf[1024];

static void __itoa(int n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    int neg = n < 0;
    if (neg) n = -n;
    char tmp[20]; int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    if (neg) buf[j++] = '-';
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

int printf(const char* fmt, ...) {
    // Solo per compatibilità — in WASM usiamo flow_print direttamente
    flow_js_print(fmt);
    return 0;
}

int fprintf(void* stream, const char* fmt, ...) {
    flow_js_eprint(fmt);
    return 0;
}

// exit non disponibile in WASM — usa unreachable
void exit(int code) {
    __builtin_unreachable();
}
`,

}
