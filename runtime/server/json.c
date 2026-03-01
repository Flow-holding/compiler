#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

// ─────────────────────────────────────────────────────────────────
// Flow JSON Builder — usato dal codice generato per le server functions
// ─────────────────────────────────────────────────────────────────

#define JSON_BUF_SIZE 65536

// Crea una stringa JSON da una stringa C (con escape)
static char* flow_json_str(const char *s) {
    if (!s) return strdup("null");
    char *out = (char*)malloc(strlen(s) * 6 + 4);
    char *p   = out;
    *p++ = '"';
    for (const char *c = s; *c; c++) {
        if (*c == '"')       { *p++ = '\\'; *p++ = '"'; }
        else if (*c == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else if (*c == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (*c == '\r') { *p++ = '\\'; *p++ = 'r'; }
        else if (*c == '\t') { *p++ = '\\'; *p++ = 't'; }
        else                 { *p++ = *c; }
    }
    *p++ = '"';
    *p   = '\0';
    return out;
}

// Crea una stringa JSON da un intero
static char* flow_json_int(int n) {
    char *out = (char*)malloc(32);
    snprintf(out, 32, "%d", n);
    return out;
}

// Crea una stringa JSON da un double
static char* flow_json_float(double d) {
    char *out = (char*)malloc(64);
    snprintf(out, 64, "%g", d);
    return out;
}

// Crea una stringa JSON da un bool
static char* flow_json_bool(int b) {
    return strdup(b ? "true" : "false");
}

// Converte qualsiasi valore C (come stringa) a JSON
// Usato per espressioni dinamiche nel codice generato
static char* flow_json_from(const char *val) {
    if (!val) return strdup("null");
    return flow_json_str(val);
}

// Costruisce un oggetto JSON da coppie "chiave", valore, ..., NULL
// Esempio: flow_json_obj_begin(), "ok", flow_json_bool(1), "user", flow_json_str("mario"), NULL)
// Nota: usa varargs con terminatore NULL
static char* flow_json_obj(const char *first_key, ...) {
    char *buf = (char*)malloc(JSON_BUF_SIZE);
    char *p   = buf;
    *p++      = '{';

    va_list ap;
    va_start(ap, first_key);
    const char *key = first_key;
    int first = 1;
    while (key) {
        char *val = va_arg(ap, char*);
        if (!first) *p++ = ',';
        first = 0;
        // key
        *p++ = '"';
        for (const char *c = key; *c; c++) *p++ = *c;
        *p++ = '"';
        *p++ = ':';
        // val
        if (val) {
            for (const char *c = val; *c; c++) *p++ = *c;
            free(val);
        } else {
            memcpy(p, "null", 4); p += 4;
        }
        key = va_arg(ap, const char*);
    }
    va_end(ap);
    *p++ = '}';
    *p   = '\0';
    return buf;
}

// Macro di convenienza per oggetti con i loro campi
// Uso: flow_json_obj_begin() "campo", flow_json_str(val), ..., NULL)
#define flow_json_obj_begin() flow_json_obj

// Estrae il valore di una chiave da un JSON nested
// path: il path di chiavi separate da punto, es: "data.nome" oppure "data", "nome"
// out: buffer per il risultato
static int flow_json_extract(const char *json, const char *section,
                              const char *key, char *out, int out_sz) {
    out[0] = '\0';
    if (!json || !key) return 0;

    // Cerca prima la sezione (es. "data": {...})
    const char *search = json;
    if (section && section[0]) {
        char needle[128];
        snprintf(needle, sizeof(needle), "\"%s\"", section);
        search = strstr(json, needle);
        if (!search) return 0;
        search += strlen(needle);
        while (*search == ' ' || *search == ':' || *search == ' ') search++;
        if (*search != '{') return 0;
        search++;
    }

    // Cerca la chiave nella sezione
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(search, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ' || *p == ':' || *p == ' ') p++;

    // Leggi il valore
    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && *p != '"' && i < out_sz - 1) {
            if (*p == '\\' && *(p+1)) { p++; }
            out[i++] = *p++;
        }
        out[i] = '\0';
        return 1;
    }

    // Valore numerico/booleano/null
    int i = 0;
    while (*p && *p != ',' && *p != '}' && *p != '\n' && i < out_sz - 1)
        out[i++] = *p++;
    // trim trailing spaces
    while (i > 0 && out[i-1] == ' ') i--;
    out[i] = '\0';
    return i > 0;
}
