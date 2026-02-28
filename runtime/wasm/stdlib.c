#pragma once

// ── Tipi base ────────────────────────────────────────────
#define NULL ((void*)0)
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
