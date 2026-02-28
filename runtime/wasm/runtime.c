#pragma once
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

// ── Stub native-only (fs, process) — su web ritornano null/0 ─────────────
// Evita crash: l'app funziona, le feature non disponibili restano silenti

FlowStr* flow_fs_read(const char* path) { (void)path; return NULL; }
int32_t flow_fs_write(const char* path, FlowStr* s) { (void)path; (void)s; return 0; }
int32_t flow_fs_exists(const char* path) { (void)path; return 0; }
int32_t flow_fs_delete(const char* path) { (void)path; return 0; }
int32_t flow_fs_mkdir(const char* path) { (void)path; return 0; }
int32_t flow_fs_rmdir(const char* path) { (void)path; return 0; }
FlowArray* flow_fs_list_dir(const char* path) { (void)path; return flow_array_new(); }
int32_t flow_fs_is_dir(const char* path) { (void)path; return 0; }
int32_t flow_fs_is_file(const char* path) { (void)path; return 0; }
int32_t flow_fs_size(const char* path) { (void)path; return 0; }

FlowStr* flow_process_env(const char* key) { (void)key; return NULL; }
FlowStr* flow_process_cwd(void) { return flow_str_new(""); }
int32_t flow_process_chdir(const char* path) { (void)path; return 0; }
int32_t flow_process_spawn(const char* cmd, FlowArray* args) { (void)cmd; (void)args; return 0; }
int32_t flow_process_spawn_wait(int32_t pid) { (void)pid; return -1; }
void flow_process_exit(int32_t code) { (void)code; __builtin_unreachable(); }
