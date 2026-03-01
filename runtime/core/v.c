#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

// ─────────────────────────────────────────────────────────────────
// Flow v — validatore runtime
// Implementa le regole definite in flow/core/v.flow
// Disponibile sia su server (C) che client (WASM)
// ─────────────────────────────────────────────────────────────────

#define V_ERR_MAX 32
#define V_ERR_MSG 256

typedef struct {
    char field[128];
    char message[V_ERR_MSG];
} VError;

typedef struct {
    int     ok;
    int     n_errors;
    VError  errors[V_ERR_MAX];
} VResult;

// Aggiunge un errore al result
static void v_err(VResult *r, const char *field, const char *fmt, ...) {
    if (r->n_errors >= V_ERR_MAX) return;
    r->ok = 0;
    VError *e = &r->errors[r->n_errors++];
    strncpy(e->field, field ? field : "", sizeof(e->field) - 1);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->message, sizeof(e->message), fmt, ap);
    va_end(ap);
}

// Serializza VResult a JSON per la risposta HTTP
static char* v_result_to_json(VResult *r) {
    char *buf = (char*)malloc(4096);
    if (r->ok) {
        snprintf(buf, 4096, "{\"ok\":true}");
    } else {
        char *p = buf;
        p += sprintf(p, "{\"ok\":false,\"errors\":[");
        for (int i = 0; i < r->n_errors; i++) {
            if (i > 0) *p++ = ',';
            p += sprintf(p, "{\"field\":\"%s\",\"message\":\"%s\"}",
                         r->errors[i].field, r->errors[i].message);
        }
        sprintf(p, "]}");
    }
    return buf;
}

// ── Schema string ─────────────────────────────────────────────────

typedef struct {
    int   has_min, has_max, has_len;
    int   min_val, max_val, len_val;
    int   is_email, is_url, is_uuid;
    int   is_optional;
    char  regex[256];
} VStrSchema;

static VStrSchema v_str_schema(void) {
    VStrSchema s = {0};
    return s;
}

static int v_validate_str(const char *val, const char *field,
                            VStrSchema *s, VResult *r) {
    if (!val || val[0] == '\0') {
        if (s->is_optional) return 1;
        v_err(r, field, "Campo obbligatorio");
        return 0;
    }
    int ok = 1;
    int len = (int)strlen(val);
    if (s->has_min && len < s->min_val) {
        v_err(r, field, "Minimo %d caratteri", s->min_val);
        ok = 0;
    }
    if (s->has_max && len > s->max_val) {
        v_err(r, field, "Massimo %d caratteri", s->max_val);
        ok = 0;
    }
    if (s->has_len && len != s->len_val) {
        v_err(r, field, "Deve essere esattamente %d caratteri", s->len_val);
        ok = 0;
    }
    if (s->is_email) {
        const char *at = strchr(val, '@');
        const char *dot = at ? strrchr(at, '.') : NULL;
        if (!at || !dot || dot == at + 1 || !*(dot + 1)) {
            v_err(r, field, "Email non valida");
            ok = 0;
        }
    }
    if (s->is_url) {
        if (strncmp(val, "http://", 7) != 0 && strncmp(val, "https://", 8) != 0) {
            v_err(r, field, "URL non valida (deve iniziare con http:// o https://)");
            ok = 0;
        }
    }
    return ok;
}

// ── Schema int ────────────────────────────────────────────────────

typedef struct {
    int has_min, has_max;
    int min_val, max_val;
    int is_optional;
    int positive, negative;
} VIntSchema;

static int v_validate_int(const char *val, const char *field,
                           VIntSchema *s, VResult *r) {
    if (!val || val[0] == '\0') {
        if (s->is_optional) return 1;
        v_err(r, field, "Campo obbligatorio");
        return 0;
    }
    // Verifica che sia un numero
    const char *p = val;
    if (*p == '-') p++;
    if (!isdigit((unsigned char)*p)) {
        v_err(r, field, "Deve essere un numero intero");
        return 0;
    }
    int n = atoi(val);
    int ok = 1;
    if (s->positive && n <= 0) { v_err(r, field, "Deve essere positivo"); ok = 0; }
    if (s->negative && n >= 0) { v_err(r, field, "Deve essere negativo"); ok = 0; }
    if (s->has_min && n < s->min_val) { v_err(r, field, "Minimo %d", s->min_val); ok = 0; }
    if (s->has_max && n > s->max_val) { v_err(r, field, "Massimo %d", s->max_val); ok = 0; }
    return ok;
}

// ── Validazione schema dal body JSON ─────────────────────────────
// Usato dal codice generato per `input({ campo: v.str().min(2) })`
// I campi del body sono già estratti come stringhe in _data_xxx

typedef struct {
    const char *field;
    int         type;   // 0=str, 1=int, 2=float, 3=bool
    int         required;
    int         min_len, max_len;
    int         min_val, max_val;
    int         is_email, is_url, is_uuid;
} VFieldSpec;

#define V_TYPE_STR   0
#define V_TYPE_INT   1
#define V_TYPE_FLOAT 2
#define V_TYPE_BOOL  3

static VResult v_validate_fields(VFieldSpec *specs, int n,
                                  const char **values) {
    VResult r = { .ok = 1 };
    for (int i = 0; i < n; i++) {
        VFieldSpec *s = &specs[i];
        const char *val = values[i];
        if (!val || val[0] == '\0') {
            if (s->required) v_err(&r, s->field, "Campo obbligatorio");
            continue;
        }
        if (s->type == V_TYPE_STR) {
            VStrSchema ss = {
                .has_min = s->min_len > 0, .min_val = s->min_len,
                .has_max = s->max_len > 0, .max_val = s->max_len,
                .is_email = s->is_email, .is_url = s->is_url
            };
            v_validate_str(val, s->field, &ss, &r);
        } else if (s->type == V_TYPE_INT) {
            VIntSchema is = {
                .has_min = s->min_val != 0, .min_val = s->min_val,
                .has_max = s->max_val != 0, .max_val = s->max_val
            };
            v_validate_int(val, s->field, &is, &r);
        }
    }
    return r;
}
