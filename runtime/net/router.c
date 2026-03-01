#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// ─────────────────────────────────────────────────────────────────
// Flow Router — registra handler per metodo+path e fa dispatching
// ─────────────────────────────────────────────────────────────────

#define FLOW_ROUTER_MAX_ROUTES 256
#define FLOW_PATH_PARAM_MAX     16

typedef struct {
    char   key[64];
    char   val[256];
} FlowPathParam;

typedef struct {
    const char *method;     // "GET", "POST", "WS", "*"
    const char *pattern;    // "/api/users/:id" oppure "/api/*"
    void       *handler;    // puntatore alla funzione handler
    int         is_fn;      // 1 = server function, 0 = REST/static
} FlowRoute;

typedef struct {
    FlowRoute routes[FLOW_ROUTER_MAX_ROUTES];
    int       count;
} FlowRouter;

static FlowRouter g_router = {0};

// Registra una route
void flow_route(const char *method, const char *pattern, void *handler) {
    if (g_router.count >= FLOW_ROUTER_MAX_ROUTES) return;
    FlowRoute *r  = &g_router.routes[g_router.count++];
    r->method     = method;
    r->pattern    = pattern;
    r->handler    = handler;
    r->is_fn      = 0;
}

// Registra una server function (POST /_flow/fn/<name>)
void flow_fn(const char *name, void *handler) {
    if (g_router.count >= FLOW_ROUTER_MAX_ROUTES) return;
    char *pattern = (char*)malloc(64);
    snprintf(pattern, 64, "/_flow/fn/%s", name);
    FlowRoute *r  = &g_router.routes[g_router.count++];
    r->method     = "POST";
    r->pattern    = pattern;
    r->handler    = handler;
    r->is_fn      = 1;
}

// ── Pattern matching con :param e * wildcard ──────────────────────

// Controlla se `path` fa match con `pattern`, estrae i param
// Es: pattern="/api/users/:id" path="/api/users/42" → param id=42
static int flow_match_path(const char *pattern, const char *path,
                            FlowPathParam *params, int *n_params) {
    *n_params = 0;
    const char *p = pattern;
    const char *s = path;

    while (*p && *s) {
        if (*p == '*') return 1;  // wildcard — match tutto il resto

        if (*p == ':') {
            // estrai nome param
            p++;
            char key[64] = {0};
            int ki = 0;
            while (*p && *p != '/') key[ki++] = *p++;

            // estrai valore
            char val[256] = {0};
            int vi = 0;
            while (*s && *s != '/') val[vi++] = *s++;

            if (*n_params < FLOW_PATH_PARAM_MAX) {
                strncpy(params[*n_params].key, key, 63);
                strncpy(params[*n_params].val, val, 255);
                (*n_params)++;
            }
            continue;
        }

        if (*p != *s) return 0;
        p++; s++;
    }

    // Entrambi finiti = match esatto
    return (*p == '\0' && *s == '\0');
}

// Trova la route che corrisponde a metodo+path
// Restituisce il puntatore all'handler o NULL
void *flow_router_dispatch(const char *method, const char *path,
                            FlowPathParam *params, int *n_params) {
    for (int i = 0; i < g_router.count; i++) {
        FlowRoute *r = &g_router.routes[i];
        if (strcmp(r->method, "*") != 0 && strcmp(r->method, method) != 0)
            continue;
        if (flow_match_path(r->pattern, path, params, n_params))
            return r->handler;
    }
    return NULL;
}
