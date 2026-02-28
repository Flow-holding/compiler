#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

// ── Arena allocator ──────────────────────────────────────────────
// Tutta la memoria del compiler viene allocata in una arena
// e liberata in un colpo solo alla fine — zero free() individuali

typedef struct Arena {
    uint8_t* buf;
    size_t   cap;
    size_t   used;
} Arena;

static inline Arena arena_new(size_t cap) {
    Arena a;
    a.buf  = (uint8_t*)malloc(cap);
    a.cap  = cap;
    a.used = 0;
    return a;
}

static inline void* arena_alloc(Arena* a, size_t size) {
    size = (size + 7) & ~7;  // align a 8 byte
    if (a->used + size > a->cap) {
        a->cap  = a->cap * 2 + size;
        a->buf  = (uint8_t*)realloc(a->buf, a->cap);
    }
    void* p   = a->buf + a->used;
    a->used  += size;
    memset(p, 0, size);
    return p;
}

static inline void arena_free(Arena* a) {
    free(a->buf);
    a->buf  = NULL;
    a->used = 0;
    a->cap  = 0;
}

// ── Stringa dinamica ─────────────────────────────────────────────

typedef struct Str {
    char*  data;
    int    len;
    int    cap;
} Str;

static inline Str str_new() {
    Str s = { (char*)malloc(64), 0, 64 };
    s.data[0] = '\0';
    return s;
}

static inline void str_push(Str* s, const char* src, int n) {
    if (s->len + n + 1 > s->cap) {
        s->cap = s->cap * 2 + n + 1;
        s->data = (char*)realloc(s->data, s->cap);
    }
    memcpy(s->data + s->len, src, n);
    s->len += n;
    s->data[s->len] = '\0';
}

static inline void str_cat(Str* s, const char* src) {
    str_push(s, src, (int)strlen(src));
}

static inline void str_catc(Str* s, char c) {
    str_push(s, &c, 1);
}

static inline void str_catf(Str* s, const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    str_push(s, buf, n);
}

// ── Vec (array dinamico di puntatori) ────────────────────────────

typedef struct Vec {
    void** data;
    int    len;
    int    cap;
} Vec;

static inline Vec vec_new() {
    Vec v = { (void**)malloc(8 * sizeof(void*)), 0, 8 };
    return v;
}

static inline void vec_push(Vec* v, void* item) {
    if (v->len >= v->cap) {
        v->cap  *= 2;
        v->data  = (void**)realloc(v->data, v->cap * sizeof(void*));
    }
    v->data[v->len++] = item;
}

static inline void* vec_get(Vec* v, int i) {
    return (i >= 0 && i < v->len) ? v->data[i] : NULL;
}

// ── Utility string ───────────────────────────────────────────────

static inline char* str_dup(const char* s) {
    if (!s) return NULL;
    char* d = (char*)malloc(strlen(s) + 1);
    strcpy(d, s);
    return d;
}

static inline bool str_eq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

static inline char* str_intern(Arena* a, const char* s, int len) {
    char* p = (char*)arena_alloc(a, len + 1);
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

// ── Errore ───────────────────────────────────────────────────────

typedef struct Error {
    char*  msg;
    int    line;
    int    col;
} Error;

typedef struct ErrorList {
    Error* data;
    int    len;
    int    cap;
} ErrorList;

static inline ErrorList errlist_new() {
    ErrorList e = { (Error*)malloc(8 * sizeof(Error)), 0, 8 };
    return e;
}

static inline void errlist_add(ErrorList* e, int line, int col, const char* fmt, ...) {
    if (e->len >= e->cap) {
        e->cap *= 2;
        e->data = (Error*)realloc(e->data, e->cap * sizeof(Error));
    }
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    e->data[e->len++] = (Error){ str_dup(buf), line, col };
}
