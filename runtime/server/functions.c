#pragma once
#include "../net/http.c"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ─────────────────────────────────────────────────────────────────
// Flow Server Functions — dispatch layer
//
// Il compilatore genera per ogni `export xxx = input() { ... }`:
//
//   FlowRes* flow_fn_xxx(FlowReq *req) {
//       FlowCtx ctx = flow_ctx_from_req(req);       // shared del client
//       // ... codegen per il body dell'handler ...
//   }
//
//   // in flow_register_functions():
//   flow_fn("xxx", flow_fn_xxx);
//
// Questo file fornisce le utility usate dal codice generato.
// ─────────────────────────────────────────────────────────────────

// ── Context per-request ───────────────────────────────────────────
// Il compilatore estende questa struct con i campi aggiunti dai middleware.

typedef struct FlowCtx {
    // Campi di client.shared (inviati dal client ad ogni chiamata)
    char session[512];
    char locale[32];
    char timezone[64];
    // Campi aggiuntivi aggiunti dai middleware (via ctx.with)
    // Il compilatore li aggiunge come puntatori opachi o in una mappa
    void *ext;  // estensione tipata generata dal compilatore
} FlowCtx;

// ── Parse JSON minimale ───────────────────────────────────────────
// Estrae il valore stringa di una chiave da un JSON flat
// { "key": "value", ... }  →  "value"
static int json_get_str(const char *json, const char *key,
                         char *out, int out_sz) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ' || *p == ':' || *p == ' ') p++;
    if (*p != '"') return 0;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < out_sz - 1) out[i++] = *p++;
    out[i] = '\0';
    return 1;
}

// ── Costruisce il ctx dalla richiesta ────────────────────────────
// Legge il campo "shared" dal body JSON e popola FlowCtx
FlowCtx flow_ctx_from_req(FlowReq *req) {
    FlowCtx ctx = {0};

    // Body atteso: { "shared": { "session": "...", "locale": "it", ... }, "data": {...} }
    // Parsing minimale del nested shared
    const char *shared_start = strstr(req->body, "\"shared\"");
    if (shared_start) {
        const char *obj = strchr(shared_start, '{');
        if (obj) {
            char shared_json[1024] = {0};
            int depth = 0, i = 0;
            const char *c = obj;
            while (*c && i < (int)sizeof(shared_json) - 1) {
                if (*c == '{') depth++;
                if (*c == '}') { depth--; if (depth == 0) { shared_json[i++] = *c; break; } }
                shared_json[i++] = *c++;
            }
            json_get_str(shared_json, "session",  ctx.session,  sizeof(ctx.session));
            json_get_str(shared_json, "locale",   ctx.locale,   sizeof(ctx.locale));
            json_get_str(shared_json, "timezone", ctx.timezone, sizeof(ctx.timezone));
        }
    }

    return ctx;
}

// ── Risposta JSON per server functions ───────────────────────────
// Il compilatore usa questo per wrappare il valore di ritorno
FlowRes *flow_fn_ok(const char *json_result) {
    FlowRes *res = (FlowRes*)calloc(1, sizeof(FlowRes));
    res->body_len = snprintf(res->body, sizeof(res->body),
        "{\"ok\":true,\"data\":%s}", json_result);
    res->status = 200;
    snprintf(res->content_type, sizeof(res->content_type), "application/json");
    return res;
}

FlowRes *flow_fn_err(int status, const char *message) {
    FlowRes *res = (FlowRes*)calloc(1, sizeof(FlowRes));
    res->body_len = snprintf(res->body, sizeof(res->body),
        "{\"ok\":false,\"error\":\"%s\"}", message);
    res->status = status;
    snprintf(res->content_type, sizeof(res->content_type), "application/json");
    return res;
}

// ── Logger per server functions ───────────────────────────────────
// Iniettato dal compilatore come wrap attorno ad ogni handler
#define FLOW_FN_LOG_START()   clock_t _t0 = clock()
#define FLOW_FN_LOG_END(name, status) do { \
    long _ms = (long)((double)(clock() - _t0) / CLOCKS_PER_SEC * 1000); \
    const char *_col = (status) < 300 ? "\033[32m" : "\033[31m"; \
    fprintf(stdout, "  %s%s\033[0m  \033[2m%ldms\033[0m\n", _col, (name), _ms); \
    fflush(stdout); \
} while(0)
