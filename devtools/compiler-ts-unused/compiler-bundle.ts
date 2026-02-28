// AUTO-GENERATED — non modificare manualmente
// Generato da scripts/bundle-compiler.ts
// Contiene 8 file C del compiler Flow

export const COMPILER_BUNDLE: Record<string, string> = {
    "codegen/codegen.c": `#include "codegen.h"

// ── Tabella native bindings (stdlib Flow → C) ───────────────────
// Corrisponde alle dichiarazioni @native nei file flow/core/*.flow

static const char* native_map[][2] = {
    { "print",   "flow_print"       },
    { "eprint",  "flow_eprint"      },
    { "concat",  "flow_str_concat"  },
    { "len",     "flow_str_len"     },
    { "push",    "flow_arr_push"    },
    { "get",     "flow_arr_get"     },
    { "set",     "flow_map_set"     },
    { "has",     "flow_map_has"     },
    { "read",    "flow_fs_read"     },
    { "write",   "flow_fs_write"    },
    { "exists",  "flow_fs_exists"   },
    { NULL, NULL }
};

static const char* resolve_native(const char* name) {
    for (int i = 0; native_map[i][0]; i++)
        if (str_eq(name, native_map[i][0])) return native_map[i][1];
    return name;
}

// ── Utility ──────────────────────────────────────────────────────

static void indent_push(int* ind) { (*ind)++; }
static void indent_pop(int* ind)  { if (*ind > 0) (*ind)--; }

static void emit(Str* out, int ind, const char* fmt, ...) {
    for (int i = 0; i < ind; i++) str_cat(out, "    ");
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    str_cat(out, buf);
    str_catc(out, '\\n');
}

// ── Tipi Flow → C ────────────────────────────────────────────────

static const char* gen_type(Node* t) {
    if (!t) return "void";
    switch (t->kind) {
        case ND_TYPE_PRIM:
            if (str_eq(t->name, "str"))   return "FlowStr*";
            if (str_eq(t->name, "int"))   return "int32_t";
            if (str_eq(t->name, "float")) return "double";
            if (str_eq(t->name, "bool"))  return "int32_t";
            if (str_eq(t->name, "void"))  return "void";
            if (str_eq(t->name, "any"))   return "FlowObject*";
            return "void";
        case ND_TYPE_ARRAY:    return "FlowArray*";
        case ND_TYPE_MAP:      return "FlowMap*";
        case ND_TYPE_OPTIONAL: return gen_type(t->left);
        case ND_TYPE_NAMED: {
            static char buf[64];
            snprintf(buf, sizeof(buf), "Flow_%s*", t->name);
            return buf;
        }
        default: return "void";
    }
}

// ── Generatore espressioni C ─────────────────────────────────────

static void gen_expr(Str* out, Node* n) {
    if (!n) return;
    switch (n->kind) {
        case ND_LIT_INT:
        case ND_LIT_FLOAT:
            str_cat(out, n->value);
            break;

        case ND_LIT_STRING: {
            char buf[512];
            snprintf(buf, sizeof(buf), "flow_str_new(\\"%s\\")", n->value ? n->value : "");
            str_cat(out, buf);
            break;
        }

        case ND_LIT_BOOL:
            str_cat(out, str_eq(n->value, "true") ? "1" : "0");
            break;

        case ND_LIT_NULL:
            str_cat(out, "NULL");
            break;

        case ND_LIT_ARRAY: {
            str_cat(out, "flow_arr_new()");
            break;
        }

        case ND_IDENT:
            str_cat(out, n->name);
            break;

        case ND_BINARY: {
            const char* op = n->op;
            // Operatori Flow → C
            if (str_eq(op, "=="))  op = "==";
            if (str_eq(op, "!="))  op = "!=";
            if (str_eq(op, "&&"))  op = "&&";
            if (str_eq(op, "||"))  op = "||";
            str_cat(out, "(");
            gen_expr(out, n->left);
            str_cat(out, " "); str_cat(out, op); str_cat(out, " ");
            gen_expr(out, n->right);
            str_cat(out, ")");
            break;
        }

        case ND_UNARY:
            str_cat(out, n->op);
            gen_expr(out, n->right);
            break;

        case ND_CALL: {
            // Funzione libera: risolvi native binding se esiste
            str_cat(out, resolve_native(n->name ? n->name : ""));
            str_cat(out, "(");
            for (int i = 0; i < n->children.len; i++) {
                if (i > 0) str_cat(out, ", ");
                gen_expr(out, (Node*)n->children.data[i]);
            }
            str_cat(out, ")");
            break;
        }

        case ND_MEMBER: {
            // obj.field o obj.method(args)
            gen_expr(out, n->left);
            str_cat(out, "->");
            str_cat(out, n->name);
            if (n->kind == ND_CALL) {
                str_cat(out, "(");
                for (int i = 0; i < n->children.len; i++) {
                    if (i > 0) str_cat(out, ", ");
                    gen_expr(out, (Node*)n->children.data[i]);
                }
                str_cat(out, ")");
            }
            break;
        }

        case ND_OPT_MEMBER: {
            char buf[256];
            snprintf(buf, sizeof(buf), "(%s? %s->%s : NULL)",
                "left_tmp", "left_tmp", n->name);
            str_cat(out, buf);
            break;
        }

        case ND_INDEX: {
            str_cat(out, "flow_arr_get(");
            gen_expr(out, n->left);
            str_cat(out, ", ");
            gen_expr(out, n->right);
            str_cat(out, ")");
            break;
        }

        case ND_ASSIGN: {
            gen_expr(out, n->left);
            str_cat(out, " = ");
            gen_expr(out, n->right);
            break;
        }

        case ND_CAST: {
            str_cat(out, "((");
            str_cat(out, gen_type(n->type));
            str_cat(out, ")(");
            gen_expr(out, n->left);
            str_cat(out, "))");
            break;
        }

        default:
            str_cat(out, "/* expr? */");
    }
}

// ── Generatore statement C ───────────────────────────────────────

static void gen_stmt(Str* out, Node* n, int ind);

static void gen_block(Str* out, Node* block, int ind) {
    if (!block) return;
    for (int i = 0; i < block->children.len; i++)
        gen_stmt(out, (Node*)block->children.data[i], ind);
}

static void gen_stmt(Str* out, Node* n, int ind) {
    if (!n) return;
    switch (n->kind) {
        case ND_VAR_DECL: {
            const char* type = n->type ? gen_type(n->type) : "auto";
            // In C non abbiamo auto — usiamo int come fallback se non c'è tipo
            if (str_eq(type, "auto")) type = "FlowObject*";
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, type); str_cat(out, " "); str_cat(out, n->name);
            if (n->left) {
                str_cat(out, " = ");
                gen_expr(out, n->left);
            }
            str_cat(out, ";\\n");
            break;
        }

        case ND_EXPR_STMT: {
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            gen_expr(out, n->left);
            str_cat(out, ";\\n");
            break;
        }

        case ND_RETURN: {
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "return");
            if (n->left) { str_cat(out, " "); gen_expr(out, n->left); }
            str_cat(out, ";\\n");
            break;
        }

        case ND_IF: {
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "if ("); gen_expr(out, n->cond); str_cat(out, ") {\\n");
            gen_block(out, n->body, ind + 1);
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "}");
            if (n->else_body) {
                str_cat(out, " else {\\n");
                gen_block(out, n->else_body, ind + 1);
                for (int i = 0; i < ind; i++) str_cat(out, "    ");
                str_cat(out, "}");
            }
            str_cat(out, "\\n");
            break;
        }

        case ND_FOR: {
            // for item in lista → FlowArray* loop
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "for (int _i = 0; _i < ");
            gen_expr(out, n->left);
            str_cat(out, "->len; _i++) {\\n");
            for (int i = 0; i < ind + 1; i++) str_cat(out, "    ");
            str_cat(out, "FlowObject* "); str_cat(out, n->name);
            str_cat(out, " = flow_arr_get(");
            gen_expr(out, n->left);
            str_cat(out, ", _i);\\n");
            gen_block(out, n->body, ind + 1);
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "}\\n");
            break;
        }

        case ND_BLOCK:
            gen_block(out, n, ind);
            break;

        default:
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "/* stmt? */\\n");
    }
}

// ── Generatore top-level C ───────────────────────────────────────

static void gen_top(Str* out, Node* n, CodegenTarget target) {
    if (!n) return;
    switch (n->kind) {

        case ND_FN_DECL: {
            // Funzione @native — solo dichiarazione
            if (n->annotation && str_eq(n->annotation->name, "native")) break;

            // Attributo export per WASM
            if (target == TARGET_WASM)
                if (!str_eq(n->name, "main")) str_catf(out, "__attribute__((export_name(\\"%s\\")))\\n", n->name);

            // main() deve restituire int in C
            const char* ret = str_eq(n->name, "main") ? "int" : gen_type(n->type);
            str_cat(out, ret);
            str_cat(out, " "); str_cat(out, resolve_native(n->name)); str_cat(out, "(");
            for (int i = 0; i < n->params.len; i++) {
                Param* p = (Param*)n->params.data[i];
                if (i > 0) str_cat(out, ", ");
                str_cat(out, gen_type(p->type));
                str_cat(out, " "); str_cat(out, p->name);
            }
            str_cat(out, ") {\\n");
            if (n->body) gen_block(out, n->body, 1);
            str_cat(out, "}\\n\\n");
            break;
        }

        case ND_VAR_DECL: {
            str_cat(out, gen_type(n->type));
            str_cat(out, " "); str_cat(out, n->name);
            if (n->left) { str_cat(out, " = "); gen_expr(out, n->left); }
            str_cat(out, ";\\n\\n");
            break;
        }

        case ND_STRUCT_DECL: {
            str_catf(out, "typedef struct {\\n    FlowObject base;\\n");
            for (int i = 0; i < n->fields.len; i++) {
                Field* f = (Field*)n->fields.data[i];
                str_catf(out, "    %s %s;\\n", gen_type(f->type), f->name);
            }
            str_catf(out, "} Flow_%s;\\n\\n", n->name);
            break;
        }

        case ND_COMPONENT_DECL:
            break;  // gestito da htmlgen/cssgen

        default: break;
    }
}

Str codegen_c(Arena* a, Node* program, CodegenTarget target, const char* runtime_dir) {
    Str out = str_new();
    char rt[512];
    snprintf(rt, sizeof(rt), "%s", runtime_dir ? runtime_dir : "../../runtime");

    // Sostituisci backslash con slash (Windows)
    for (int i = 0; rt[i]; i++) if (rt[i] == '\\\\') rt[i] = '/';

    if (target == TARGET_WASM) {
        str_catf(&out, "#include \\"%s/wasm/runtime.c\\"\\n\\n", rt);
    } else {
        str_catf(&out, "#include \\"%s/io/print.c\\"\\n", rt);
        str_catf(&out, "#include \\"%s/io/fs.c\\"\\n", rt);
        str_catf(&out, "#include \\"%s/net/http.c\\"\\n\\n", rt);
    }

    for (int i = 0; i < program->children.len; i++)
        gen_top(&out, (Node*)program->children.data[i], target);

    return out;
}

// ── HTML Generator ───────────────────────────────────────────────

static void gen_ui_node(Str* html, Node* n, int ind) {
    if (!n || n->kind != ND_UI_NODE) return;

    const char* tag = "div";
    if      (str_eq(n->name, "Text"))   tag = "p";
    else if (str_eq(n->name, "Button")) tag = "button";
    else if (str_eq(n->name, "Input"))  tag = "input";
    else if (str_eq(n->name, "Image"))  tag = "img";
    else if (str_eq(n->name, "Row"))    tag = "div";
    else if (str_eq(n->name, "Column")) tag = "div";

    for (int i = 0; i < ind; i++) str_cat(html, "  ");
    str_catf(html, "<%s", tag);

    // Attributi/props
    for (int i = 0; i < n->style.len; i++) {
        Field* f = (Field*)n->style.data[i];
        if (str_eq(f->name, "text") && f->value) {
            // testo — va nel body, non come attributo
        } else if (str_eq(f->name, "src") && f->value) {
            str_catf(html, " src=\\"%s\\"", f->value->value ? f->value->value : "");
        } else if (str_eq(f->name, "placeholder") && f->value) {
            str_catf(html, " placeholder=\\"%s\\"", f->value->value ? f->value->value : "");
        }
    }
    str_cat(html, ">\\n");

    // Testo interno
    for (int i = 0; i < n->style.len; i++) {
        Field* f = (Field*)n->style.data[i];
        if (str_eq(f->name, "text") && f->value && f->value->value) {
            for (int j = 0; j < ind + 1; j++) str_cat(html, "  ");
            str_catf(html, "%s\\n", f->value->value);
        }
    }

    // Figli
    for (int i = 0; i < n->children.len; i++)
        gen_ui_node(html, (Node*)n->children.data[i], ind + 1);

    for (int i = 0; i < ind; i++) str_cat(html, "  ");
    str_catf(html, "</%s>\\n", tag);
}

Str codegen_html(Arena* a, Node* program) {
    Str out = str_new();
    str_cat(&out,
        "<!DOCTYPE html>\\n<html lang=\\"it\\">\\n<head>\\n"
        "  <meta charset=\\"UTF-8\\">\\n"
        "  <meta name=\\"viewport\\" content=\\"width=device-width, initial-scale=1.0\\">\\n"
        "  <title>Flow App</title>\\n"
        "  <link rel=\\"stylesheet\\" href=\\"style.css\\">\\n"
        "</head>\\n<body>\\n  <div id=\\"root\\">\\n");

    for (int i = 0; i < program->children.len; i++) {
        Node* n = (Node*)program->children.data[i];
        if (n->kind == ND_COMPONENT_DECL && n->annotation &&
            str_eq(n->annotation->name, "client")) {
            if (n->body) {
                for (int j = 0; j < n->body->children.len; j++)
                    gen_ui_node(&out, (Node*)n->body->children.data[j], 2);
            }
        }
    }

    str_cat(&out,
        "  </div>\\n"
        "  <script src=\\"app.js\\"></script>\\n"
        "</body>\\n</html>\\n");
    return out;
}

// ── CSS Generator ────────────────────────────────────────────────

Str codegen_css(Arena* a, Node* program) {
    Str out = str_new();
    str_cat(&out,
        "*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }\\n"
        "body { font-family: system-ui, sans-serif; }\\n"
        "#root { display: flex; flex-direction: column; min-height: 100vh; }\\n\\n");
    return out;
}

// ── JS Generator ─────────────────────────────────────────────────

Str codegen_js(Arena* a, Node* program) {
    Str out = str_new();
    str_cat(&out,
        "let wasmMem;\\n"
        "const imports = {\\n"
        "  env: {\\n"
        "    flow_print_str: (ptr) => {\\n"
        "      const view = new Uint8Array(wasmMem.buffer);\\n"
        "      let end = ptr;\\n"
        "      while (view[end]) end++;\\n"
        "      console.log(new TextDecoder().decode(view.subarray(ptr, end)));\\n"
        "    },\\n"
        "    flow_eprint_str: (ptr) => {\\n"
        "      const view = new Uint8Array(wasmMem.buffer);\\n"
        "      let end = ptr;\\n"
        "      while (view[end]) end++;\\n"
        "      console.error(new TextDecoder().decode(view.subarray(ptr, end)));\\n"
        "    },\\n"
        "  }\\n"
        "};\\n\\n"
        "WebAssembly.instantiateStreaming(fetch('app.wasm'), imports)\\n"
        "  .then(({ instance }) => {\\n"
        "    wasmMem = instance.exports.memory;\\n"
        "    window._flow = instance.exports;\\n"
        "    if (instance.exports.main) instance.exports.main();\\n"
        "  })\\n"
        "  .catch(err => console.error('WASM error:', err));\\n");
    return out;
}
`,

    "codegen/codegen.h": `#pragma once
#include "../parser/parser.h"

typedef enum { TARGET_NATIVE, TARGET_WASM } CodegenTarget;

// Genera codice C dal AST
Str codegen_c(Arena* a, Node* program, CodegenTarget target, const char* runtime_dir);

// Genera HTML dal AST (componenti @client)
Str codegen_html(Arena* a, Node* program);

// Genera CSS dai componenti
Str codegen_css(Arena* a, Node* program);

// Genera JS glue per WASM
Str codegen_js(Arena* a, Node* program);
`,

    "common.h": `#pragma once
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
    s.data[0] = '\\0';
    return s;
}

static inline void str_push(Str* s, const char* src, int n) {
    if (s->len + n + 1 > s->cap) {
        s->cap = s->cap * 2 + n + 1;
        s->data = (char*)realloc(s->data, s->cap);
    }
    memcpy(s->data + s->len, src, n);
    s->len += n;
    s->data[s->len] = '\\0';
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
    p[len] = '\\0';
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
`,

    "lexer/lexer.c": `#include "lexer.h"
// lexer.h include già ../common.h
#include <stdarg.h>
#include <ctype.h>

static void tklist_push(Arena* a, TokenList* tl, TkKind kind, const char* val, int line, int col) {
    if (tl->len >= tl->cap) {
        tl->cap *= 2;
        tl->data = (Token*)realloc(tl->data, tl->cap * sizeof(Token));
    }
    tl->data[tl->len++] = (Token){ kind, val, line, col };
}

// Parole chiave
static TkKind keyword(const char* s, int len) {
    #define KW(str, tk) if (len == (int)strlen(str) && memcmp(s, str, len) == 0) return tk
    KW("fn",        TK_FN);
    KW("if",        TK_IF);
    KW("else",      TK_ELSE);
    KW("for",       TK_FOR);
    KW("in",        TK_IN);
    KW("return",    TK_RETURN);
    KW("match",     TK_MATCH);
    KW("struct",    TK_STRUCT);
    KW("component", TK_COMPONENT);
    KW("true",      TK_TRUE);
    KW("false",     TK_FALSE);
    KW("null",      TK_NULL);
    KW("as",        TK_AS);
    KW("type",      TK_TYPE);
    KW("str",       TK_STR_T);
    KW("int",       TK_INT_T);
    KW("float",     TK_FLOAT_T);
    KW("bool",      TK_BOOL_T);
    KW("void",      TK_VOID);
    KW("any",       TK_ANY);
    #undef KW
    return TK_IDENT;
}

TokenList lex(Arena* a, const char* src, ErrorList* errors) {
    TokenList tl;
    tl.data = (Token*)malloc(256 * sizeof(Token));
    tl.len  = 0;
    tl.cap  = 256;

    int i    = 0;
    int line = 1;
    int col  = 1;
    int n    = (int)strlen(src);

    #define PEEK()    (i < n ? src[i] : '\\0')
    #define PEEK2()   (i+1 < n ? src[i+1] : '\\0')
    #define ADV()     (col++, src[i++])
    #define PUSH(k,v) tklist_push(a, &tl, k, v, line, col)
    #define LIT(k)    { PUSH(k, NULL); ADV(); continue; }

    while (i < n) {
        char c = PEEK();

        // Whitespace (non newline)
        if (c == ' ' || c == '\\t' || c == '\\r') { ADV(); continue; }

        // Newline
        if (c == '\\n') { line++; col = 1; i++; continue; }

        // Commenti // e /* */
        if (c == '/' && PEEK2() == '/') {
            while (i < n && src[i] != '\\n') i++;
            continue;
        }
        if (c == '/' && PEEK2() == '*') {
            i += 2; col += 2;
            while (i < n && !(src[i] == '*' && i+1 < n && src[i+1] == '/')) {
                if (src[i] == '\\n') { line++; col = 1; } else col++;
                i++;
            }
            i += 2; col += 2;
            continue;
        }

        // Annotazione @nome
        if (c == '@') {
            ADV();
            int start = i;
            while (i < n && (isalnum(src[i]) || src[i] == '_')) ADV();
            PUSH(TK_AT, str_intern(a, src + start, i - start));
            continue;
        }

        // Stringa
        if (c == '"') {
            ADV();
            int start = i;
            while (i < n && src[i] != '"') {
                if (src[i] == '\\\\') { ADV(); }
                ADV();
            }
            char* val = str_intern(a, src + start, i - start);
            ADV();  // chiudi "
            PUSH(TK_STRING, val);
            continue;
        }

        // Numero
        if (isdigit(c) || (c == '-' && isdigit(PEEK2()))) {
            int start = i;
            bool is_float = false;
            if (c == '-') ADV();
            while (i < n && isdigit(src[i])) ADV();
            if (i < n && src[i] == '.' && isdigit(src[i+1])) {
                is_float = true;
                ADV();
                while (i < n && isdigit(src[i])) ADV();
            }
            char* val = str_intern(a, src + start, i - start);
            PUSH(is_float ? TK_FLOAT : TK_INT, val);
            continue;
        }

        // Identificatore / keyword
        if (isalpha(c) || c == '_') {
            int start = i;
            while (i < n && (isalnum(src[i]) || src[i] == '_')) ADV();
            TkKind k   = keyword(src + start, i - start);
            char*  val = str_intern(a, src + start, i - start);
            PUSH(k, val);
            continue;
        }

        // Operatori multi-char
        if (c == '?' && PEEK2() == '.') { ADV(); ADV(); PUSH(TK_QUEST_DOT, NULL); continue; }
        if (c == '?' && PEEK2() == '?') { ADV(); ADV(); PUSH(TK_QUEST_QUEST, NULL); continue; }
        if (c == '|' && PEEK2() == '>') { ADV(); ADV(); PUSH(TK_PIPE_GT, NULL); continue; }
        if (c == '-' && PEEK2() == '>') { ADV(); ADV(); PUSH(TK_ARROW, NULL); continue; }
        if (c == '=' && PEEK2() == '>') { ADV(); ADV(); PUSH(TK_FAT_ARROW, NULL); continue; }
        if (c == '=' && PEEK2() == '=') { ADV(); ADV(); PUSH(TK_EQ, NULL); continue; }
        if (c == '!' && PEEK2() == '=') { ADV(); ADV(); PUSH(TK_NEQ, NULL); continue; }
        if (c == '<' && PEEK2() == '=') { ADV(); ADV(); PUSH(TK_LTE, NULL); continue; }
        if (c == '>' && PEEK2() == '=') { ADV(); ADV(); PUSH(TK_GTE, NULL); continue; }
        if (c == '&' && PEEK2() == '&') { ADV(); ADV(); PUSH(TK_AND, NULL); continue; }
        if (c == '|' && PEEK2() == '|') { ADV(); ADV(); PUSH(TK_OR, NULL); continue; }
        if (c == '.' && PEEK2() == '.') { ADV(); ADV(); PUSH(TK_DOTDOT, NULL); continue; }

        // Operatori singoli
        switch (c) {
            case '+': LIT(TK_PLUS);
            case '-': LIT(TK_MINUS);
            case '*': LIT(TK_STAR);
            case '/': LIT(TK_SLASH);
            case '%': LIT(TK_PERCENT);
            case '<': LIT(TK_LT);
            case '>': LIT(TK_GT);
            case '!': LIT(TK_NOT);
            case '|': LIT(TK_PIPE);
            case '=': LIT(TK_ASSIGN);
            case ':': LIT(TK_COLON);
            case '.': LIT(TK_DOT);
            case '?': LIT(TK_QUESTION);
            case '(': LIT(TK_LPAREN);
            case ')': LIT(TK_RPAREN);
            case '{': LIT(TK_LBRACE);
            case '}': LIT(TK_RBRACE);
            case '[': LIT(TK_LBRACKET);
            case ']': LIT(TK_RBRACKET);
            case ',': LIT(TK_COMMA);
            case ';': LIT(TK_SEMICOLON);
            case '#': LIT(TK_HASH);
            default:
                errlist_add(errors, line, col, "carattere sconosciuto: '%c'", c);
                ADV();
        }
    }

    PUSH(TK_EOF, NULL);
    return tl;
}

const char* tk_name(TkKind k) {
    switch (k) {
        case TK_IDENT:     return "IDENT";
        case TK_INT:       return "INT";
        case TK_FLOAT:     return "FLOAT";
        case TK_STRING:    return "STRING";
        case TK_BOOL:      return "BOOL";
        case TK_FN:        return "fn";
        case TK_IF:        return "if";
        case TK_ELSE:      return "else";
        case TK_FOR:       return "for";
        case TK_IN:        return "in";
        case TK_RETURN:    return "return";
        case TK_MATCH:     return "match";
        case TK_STRUCT:    return "struct";
        case TK_COMPONENT: return "component";
        case TK_TRUE:      return "true";
        case TK_FALSE:     return "false";
        case TK_STR_T:     return "str";
        case TK_INT_T:     return "int";
        case TK_FLOAT_T:   return "float";
        case TK_BOOL_T:    return "bool";
        case TK_VOID:      return "void";
        case TK_ANY:       return "any";
        case TK_ASSIGN:    return "=";
        case TK_EQ:        return "==";
        case TK_NEQ:       return "!=";
        case TK_LT:        return "<";
        case TK_GT:        return ">";
        case TK_AT:        return "@";
        case TK_EOF:       return "EOF";
        default:           return "?";
    }
}
`,

    "lexer/lexer.h": `#pragma once
#include "../common.h"

typedef enum {
    // Letterali
    TK_IDENT, TK_INT, TK_FLOAT, TK_STRING, TK_BOOL,

    // Keywords
    TK_FN, TK_IF, TK_ELSE, TK_FOR, TK_IN, TK_RETURN,
    TK_MATCH, TK_STRUCT, TK_COMPONENT, TK_TRUE, TK_FALSE,
    TK_NULL, TK_AS, TK_TYPE,

    // Tipi primitivi
    TK_STR_T, TK_INT_T, TK_FLOAT_T, TK_BOOL_T, TK_VOID, TK_ANY,

    // Operatori
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LTE, TK_GTE,
    TK_AND, TK_OR, TK_NOT, TK_PIPE,
    TK_ASSIGN,        // =
    TK_COLON,         // :
    TK_ARROW,         // ->
    TK_FAT_ARROW,     // =>
    TK_DOT,           // .
    TK_DOTDOT,        // ..
    TK_QUESTION,      // ?
    TK_QUEST_DOT,     // ?.
    TK_QUEST_QUEST,   // ??
    TK_AT,            // @
    TK_HASH,          // #
    TK_PIPE_GT,       // |>

    // Delimitatori
    TK_LPAREN, TK_RPAREN,
    TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_SEMICOLON, TK_NEWLINE,

    TK_EOF,
} TkKind;

typedef struct {
    TkKind      kind;
    const char* val;   // puntatore nell'arena — non alloca
    int         line;
    int         col;
} Token;

typedef struct {
    Token* data;
    int    len;
    int    cap;
} TokenList;

// Tokenizza source e restituisce una lista di token
TokenList lex(Arena* a, const char* source, ErrorList* errors);

// Debug
const char* tk_name(TkKind k);
`,

    "main.c": `#include "codegen/codegen.h"
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
  #include <process.h>
  #define PATH_SEP "\\\\"
  #define EXE_EXT  ".exe"
#else
  #include <unistd.h>
  #include <sys/wait.h>
  #include <sys/stat.h>
  #define PATH_SEP "/"
  #define EXE_EXT  ""
#endif

// ── Utility file system ──────────────────────────────────────────

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* buf = (char*)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\\0';
    fclose(f);
    return buf;
}

static bool write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    return true;
}

static void make_dir(const char* path) {
#ifdef _WIN32
    CreateDirectoryA(path, NULL);
#else
    mkdir(path, 0755);
#endif
}

static bool file_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return true; }
    return false;
}

static void path_join(char* out, int max, const char* a, const char* b) {
    snprintf(out, max, "%s%s%s", a, PATH_SEP, b);
}

static void path_dir(char* out, int max, const char* path) {
    strncpy(out, path, max);
    char* last = strrchr(out, '/');
    char* last2 = strrchr(out, '\\\\');
    char* sep = last > last2 ? last : last2;
    if (sep) *sep = '\\0'; else strncpy(out, ".", max);
}

static void path_stem(char* out, int max, const char* path) {
    const char* base = strrchr(path, '/');
    const char* base2 = strrchr(path, '\\\\');
    const char* b = base > base2 ? base : base2;
    if (b) b++; else b = path;
    strncpy(out, b, max);
    char* dot = strrchr(out, '.');
    if (dot) *dot = '\\0';
}

// ── Esegui processo esterno ──────────────────────────────────────

static int run_cmd(const char* cmd) {
    return system(cmd);
}

// ── Pipeline di compilazione ─────────────────────────────────────

static void pipeline(
    const char* input_path,
    const char* out_dir,
    const char* runtime_dir,
    bool        do_run,
    bool        prod
) {
    clock_t start = clock();

    // Leggi sorgente
    char* source = read_file(input_path);
    if (!source) {
        fprintf(stderr, "flowc: file non trovato \\"%s\\"\\n", input_path);
        exit(1);
    }

    // Setup arena + error list
    Arena     arena  = arena_new(4 * 1024 * 1024);  // 4MB
    ErrorList errors = errlist_new();

    // ── Lexer
    TokenList tokens = lex(&arena, source, &errors);
    if (errors.len > 0) {
        for (int i = 0; i < errors.len; i++)
            fprintf(stderr, "  ✗ [%d:%d] %s\\n",
                errors.data[i].line, errors.data[i].col, errors.data[i].msg);
        exit(1);
    }

    // ── Parser
    Node* ast = parse(&arena, tokens, &errors);
    if (errors.len > 0) {
        for (int i = 0; i < errors.len; i++)
            fprintf(stderr, "  ✗ [%d:%d] %s\\n",
                errors.data[i].line, errors.data[i].col, errors.data[i].msg);
        exit(1);
    }

    // Crea out_dir
    make_dir(out_dir);

    char stem[256]; path_stem(stem, sizeof(stem), input_path);

    // ── Codegen C nativo
    char c_path[512]; snprintf(c_path, sizeof(c_path), "%s/%s.c", out_dir, stem);
    Str c_code = codegen_c(&arena, ast, TARGET_NATIVE, runtime_dir);
    write_file(c_path, c_code.data);

    // ── Compila .exe
    char exe_path[512]; snprintf(exe_path, sizeof(exe_path), "%s/%s%s", out_dir, stem, EXE_EXT);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "clang \\"%s\\" -o \\"%s\\" %s -D_CRT_SECURE_NO_WARNINGS -w",
        c_path, exe_path, prod ? "-O2" : "-g");
    int rc = run_cmd(cmd);

    if (rc != 0) {
        fprintf(stderr, "✗ Errore compilazione nativa\\n");
    } else {
        printf("  .exe    %s\\n", exe_path);
    }

    // ── Codegen WASM
    char wasm_c[512]; snprintf(wasm_c, sizeof(wasm_c), "%s/%s.wasm.c", out_dir, stem);
    char wasm_path[512]; snprintf(wasm_path, sizeof(wasm_path), "%s/app.wasm", out_dir);
    Str wasm_code = codegen_c(&arena, ast, TARGET_WASM, runtime_dir);
    write_file(wasm_c, wasm_code.data);

    snprintf(cmd, sizeof(cmd),
        "clang --target=wasm32-unknown-unknown -nostdlib "
        "-Wl,--no-entry -Wl,--allow-undefined -Wl,--export=main "
        "-D_CRT_SECURE_NO_WARNINGS -w %s \\"%s\\" -o \\"%s\\"",
        prod ? "-O2" : "-O1", wasm_c, wasm_path);
    if (run_cmd(cmd) == 0) printf("  .wasm   %s\\n", wasm_path);

    // ── HTML / CSS / JS
    bool has_client = false;
    for (int i = 0; i < ast->children.len; i++) {
        Node* n = (Node*)ast->children.data[i];
        if (n->kind == ND_COMPONENT_DECL && n->annotation &&
            str_eq(n->annotation->name, "client")) {
            has_client = true; break;
        }
    }

    if (has_client) {
        char html_path[512]; snprintf(html_path, sizeof(html_path), "%s/index.html", out_dir);
        char css_path[512];  snprintf(css_path,  sizeof(css_path),  "%s/style.css",  out_dir);
        char js_path[512];   snprintf(js_path,   sizeof(js_path),   "%s/app.js",     out_dir);

        Str html = codegen_html(&arena, ast);
        Str css  = codegen_css(&arena,  ast);
        Str js   = codegen_js(&arena,   ast);

        write_file(html_path, html.data);
        write_file(css_path,  css.data);
        write_file(js_path,   js.data);

        printf("  html    %s\\n", html_path);
        printf("  css     %s\\n", css_path);
        printf("  js      %s\\n", js_path);
    }

    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    printf("\\n✓ Build completata in %.0fms\\n\\n", elapsed);

    // ── Esegui se --run
    if (do_run && rc == 0) {
        printf("── output ──────────────────────────\\n");
        run_cmd(exe_path);
        printf("────────────────────────────────────\\n");
    }

    arena_free(&arena);
    free(source);
}

// ── Entry point ──────────────────────────────────────────────────

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);  // UTF-8
#endif
    if (argc < 2) {
        fprintf(stderr, "uso: flowc <file.flow> [--run] [--prod]\\n");
        return 1;
    }

    const char* input_path  = NULL;
    const char* out_dir_arg = NULL;
    bool        do_run      = false;
    bool        prod        = false;

    for (int i = 1; i < argc; i++) {
        if (str_eq(argv[i], "--run"))  { do_run = true; continue; }
        if (str_eq(argv[i], "--prod")) { prod = true;   continue; }
        if (str_eq(argv[i], "--outdir") && i + 1 < argc) { out_dir_arg = argv[++i]; continue; }
        if (!input_path) input_path = argv[i];
    }

    if (!input_path) {
        fprintf(stderr, "flowc: specifica un file .flow\\n");
        return 1;
    }

    // Cartella output — usa --outdir se specificato, altrimenti accanto al sorgente
    char out_dir[512];
    if (out_dir_arg) {
        snprintf(out_dir, sizeof(out_dir), "%s", out_dir_arg);
    } else {
        char dir[512]; path_dir(dir, sizeof(dir), input_path);
        snprintf(out_dir, sizeof(out_dir), "%s/out", dir);
    }

    // Runtime dir — cerca prima accanto al compiler, poi ~/.flow/runtime
    char runtime_dir[512] = "../../runtime";
#ifdef _WIN32
    const char* userprofile = getenv("USERPROFILE");
    if (userprofile) snprintf(runtime_dir, sizeof(runtime_dir), "%s/.flow/runtime", userprofile);
#else
    const char* home = getenv("HOME");
    if (home) snprintf(runtime_dir, sizeof(runtime_dir), "%s/.flow/runtime", home);
#endif

    pipeline(input_path, out_dir, runtime_dir, do_run, prod);
    return 0;
}
`,

    "parser/parser.c": `#include "parser.h"
#include <ctype.h>

// ── Helpers parser ───────────────────────────────────────────────

static Token* peek(Parser* p) {
    return &p->tokens.data[p->pos];
}

static Token* peek2(Parser* p) {
    int next = p->pos + 1;
    if (next >= p->tokens.len) next = p->tokens.len - 1;
    return &p->tokens.data[next];
}

static Token* advance(Parser* p) {
    Token* t = &p->tokens.data[p->pos];
    if (p->pos < p->tokens.len - 1) p->pos++;
    return t;
}

static bool check(Parser* p, TkKind k) {
    return peek(p)->kind == k;
}

static bool match(Parser* p, TkKind k) {
    if (check(p, k)) { advance(p); return true; }
    return false;
}

static Token* eat(Parser* p, TkKind k) {
    if (!check(p, k)) {
        Token* t = peek(p);
        errlist_add(p->errors, t->line, t->col,
            "atteso '%s', trovato '%s'", tk_name(k), tk_name(t->kind));
        return t;
    }
    return advance(p);
}

static Node* node_new(Parser* p, NodeKind kind) {
    Node* n  = (Node*)arena_alloc(p->arena, sizeof(Node));
    n->kind  = kind;
    n->line  = peek(p)->line;
    n->children = vec_new();
    n->params   = vec_new();
    n->fields   = vec_new();
    n->arms     = vec_new();
    n->style    = vec_new();
    return n;
}

// ── Forward declarations ─────────────────────────────────────────

static Node* parse_expr(Parser* p);
static Node* parse_stmt(Parser* p);
static Node* parse_block(Parser* p);
static Node* parse_type(Parser* p);
static Node* parse_top(Parser* p);

// ── Tipi ─────────────────────────────────────────────────────────

static Node* parse_type(Parser* p) {
    Token* t = peek(p);
    Node*  n = node_new(p, ND_TYPE_PRIM);

    // Opzionale ?T
    if (match(p, TK_QUESTION)) {
        n->kind = ND_TYPE_OPTIONAL;
        n->left = parse_type(p);
        return n;
    }

    // fn(T, T): R
    if (match(p, TK_FN)) {
        n->kind = ND_TYPE_FN;
        eat(p, TK_LPAREN);
        while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
            vec_push(&n->params, parse_type(p));
            if (!match(p, TK_COMMA)) break;
        }
        eat(p, TK_RPAREN);
        if (match(p, TK_COLON)) n->type = parse_type(p);
        return n;
    }

    // { K: V }
    if (match(p, TK_LBRACE)) {
        n->kind  = ND_TYPE_MAP;
        n->left  = parse_type(p);
        eat(p, TK_COLON);
        n->right = parse_type(p);
        eat(p, TK_RBRACE);
        return n;
    }

    // Primitivi
    switch (t->kind) {
        case TK_STR_T:   n->name = "str";   advance(p); break;
        case TK_INT_T:   n->name = "int";   advance(p); break;
        case TK_FLOAT_T: n->name = "float"; advance(p); break;
        case TK_BOOL_T:  n->name = "bool";  advance(p); break;
        case TK_VOID:    n->name = "void";  advance(p); break;
        case TK_ANY:     n->name = "any";   advance(p); break;
        case TK_IDENT:
            n->kind = ND_TYPE_NAMED;
            n->name = t->val;
            advance(p);
            break;
        default:
            errlist_add(p->errors, t->line, t->col, "tipo atteso, trovato '%s'", tk_name(t->kind));
            advance(p);
            return n;
    }

    // T[]
    if (match(p, TK_LBRACKET)) {
        eat(p, TK_RBRACKET);
        Node* arr  = node_new(p, ND_TYPE_ARRAY);
        arr->left  = n;
        return arr;
    }

    // T | err
    if (match(p, TK_PIPE)) {
        Node* u  = node_new(p, ND_TYPE_UNION);
        u->left  = n;
        u->right = parse_type(p);
        return u;
    }

    return n;
}

// ── Espressioni ──────────────────────────────────────────────────

static Node* parse_primary(Parser* p) {
    Token* t = peek(p);
    Node*  n = node_new(p, ND_LIT_INT);

    switch (t->kind) {
        case TK_INT:    n->kind = ND_LIT_INT;    n->value = t->val; advance(p); return n;
        case TK_FLOAT:  n->kind = ND_LIT_FLOAT;  n->value = t->val; advance(p); return n;
        case TK_STRING: n->kind = ND_LIT_STRING; n->value = t->val; advance(p); return n;
        case TK_TRUE:   n->kind = ND_LIT_BOOL;   n->value = "true";  advance(p); return n;
        case TK_FALSE:  n->kind = ND_LIT_BOOL;   n->value = "false"; advance(p); return n;
        case TK_NULL:   n->kind = ND_LIT_NULL;   advance(p); return n;

        case TK_LBRACKET: {
            n->kind = ND_LIT_ARRAY;
            advance(p);
            while (!check(p, TK_RBRACKET) && !check(p, TK_EOF)) {
                vec_push(&n->children, parse_expr(p));
                if (!match(p, TK_COMMA)) break;
            }
            eat(p, TK_RBRACKET);
            return n;
        }

        case TK_LBRACE: {
            n->kind = ND_LIT_MAP;
            advance(p);
            while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
                Field* f   = (Field*)arena_alloc(p->arena, sizeof(Field));
                // Accetta sia "stringa" che identificatore come chiave
                Token* key = peek(p);
                if (key->kind == TK_STRING || key->kind == TK_IDENT ||
                    key->kind == TK_STR_T  || key->kind == TK_INT_T  ||
                    key->kind == TK_BOOL_T || key->kind == TK_TYPE) {
                    f->name = key->val ? key->val : tk_name(key->kind);
                    advance(p);
                } else {
                    errlist_add(p->errors, key->line, key->col, "chiave attesa nel map");
                    advance(p); break;
                }
                eat(p, TK_COLON);
                f->value   = parse_expr(p);
                vec_push(&n->fields, f);
                match(p, TK_COMMA);
            }
            eat(p, TK_RBRACE);
            return n;
        }

        case TK_IDENT: {
            n->kind = ND_IDENT;
            n->name = t->val;
            advance(p);

            // Chiamata funzione
            if (check(p, TK_LPAREN)) {
                n->kind = ND_CALL;
                advance(p);
                while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                    vec_push(&n->children, parse_expr(p));
                    if (!match(p, TK_COMMA)) break;
                }
                eat(p, TK_RPAREN);
            }

            // UI component { ... }
            if (check(p, TK_LBRACE) && isupper((unsigned char)n->name[0])) {
                n->kind = ND_UI_NODE;
                advance(p);
                while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
                    // prop: value  o  ChildNode { }
                    if (check(p, TK_IDENT) && peek2(p)->kind == TK_COLON) {
                        Field* f = (Field*)arena_alloc(p->arena, sizeof(Field));
                        f->name  = advance(p)->val;
                        advance(p);  // :
                        f->value = parse_expr(p);
                        vec_push(&n->style, f);
                    } else {
                        vec_push(&n->children, parse_expr(p));
                    }
                    match(p, TK_COMMA);
                }
                eat(p, TK_RBRACE);
            }
            return n;
        }

        case TK_LPAREN: {
            advance(p);
            Node* inner = parse_expr(p);
            eat(p, TK_RPAREN);
            return inner;
        }

        default:
            errlist_add(p->errors, t->line, t->col, "espressione attesa, trovato '%s'", tk_name(t->kind));
            advance(p);
            return n;
    }
}

static Node* parse_postfix(Parser* p) {
    Node* left = parse_primary(p);

    while (true) {
        if (match(p, TK_DOT)) {
            Node* n  = node_new(p, ND_MEMBER);
            n->left  = left;
            n->name  = eat(p, TK_IDENT)->val;
            if (match(p, TK_LPAREN)) {
                n->kind = ND_CALL;
                while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                    vec_push(&n->children, parse_expr(p));
                    if (!match(p, TK_COMMA)) break;
                }
                eat(p, TK_RPAREN);
            }
            left = n;
        } else if (match(p, TK_QUEST_DOT)) {
            Node* n  = node_new(p, ND_OPT_MEMBER);
            n->left  = left;
            n->name  = eat(p, TK_IDENT)->val;
            left = n;
        } else if (match(p, TK_LBRACKET)) {
            Node* n  = node_new(p, ND_INDEX);
            n->left  = left;
            n->right = parse_expr(p);
            eat(p, TK_RBRACKET);
            left = n;
        } else if (match(p, TK_AS)) {
            Node* n  = node_new(p, ND_CAST);
            n->left  = left;
            n->type  = parse_type(p);
            left = n;
        } else {
            break;
        }
    }
    return left;
}

static Node* parse_unary(Parser* p) {
    if (check(p, TK_NOT) || check(p, TK_MINUS)) {
        Node* n  = node_new(p, ND_UNARY);
        n->op    = tk_name(advance(p)->kind);
        n->right = parse_unary(p);
        return n;
    }
    return parse_postfix(p);
}

// Precedenza operatori binari
static int op_prec(TkKind k) {
    switch (k) {
        case TK_OR:       return 1;
        case TK_AND:      return 2;
        case TK_EQ: case TK_NEQ: return 3;
        case TK_LT: case TK_GT: case TK_LTE: case TK_GTE: return 4;
        case TK_PIPE:     return 5;
        case TK_PLUS: case TK_MINUS: return 6;
        case TK_STAR: case TK_SLASH: case TK_PERCENT: return 7;
        case TK_PIPE_GT:  return 8;
        case TK_QUEST_QUEST: return 0;
        default:          return -1;
    }
}

static Node* parse_binary(Parser* p, int min_prec) {
    Node* left = parse_unary(p);

    while (true) {
        int prec = op_prec(peek(p)->kind);
        if (prec < min_prec) break;
        Token* op_tok = advance(p);
        Node* right   = parse_binary(p, prec + 1);
        Node* n       = node_new(p, ND_BINARY);
        n->left  = left;
        n->right = right;
        n->op    = op_tok->val ? op_tok->val : tk_name(op_tok->kind);
        left = n;
    }
    return left;
}

static Node* parse_expr(Parser* p) {
    Node* left = parse_binary(p, 0);

    // Assegnazione
    if (match(p, TK_ASSIGN)) {
        Node* n  = node_new(p, ND_ASSIGN);
        n->left  = left;
        n->right = parse_expr(p);
        return n;
    }
    return left;
}

// ── Statement ────────────────────────────────────────────────────

static Node* parse_block(Parser* p) {
    Node* n = node_new(p, ND_BLOCK);
    eat(p, TK_LBRACE);
    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        Node* s = parse_stmt(p);
        if (s) vec_push(&n->children, s);
    }
    eat(p, TK_RBRACE);
    return n;
}

static Node* parse_stmt(Parser* p) {
    Token* t = peek(p);

    // return
    if (match(p, TK_RETURN)) {
        Node* n = node_new(p, ND_RETURN);
        if (!check(p, TK_RBRACE) && !check(p, TK_EOF))
            n->left = parse_expr(p);
        return n;
    }

    // if
    if (match(p, TK_IF)) {
        Node* n = node_new(p, ND_IF);
        eat(p, TK_LPAREN);
        n->cond = parse_expr(p);
        eat(p, TK_RPAREN);
        n->body = parse_block(p);
        if (match(p, TK_ELSE))
            n->else_body = check(p, TK_IF) ? parse_stmt(p) : parse_block(p);
        return n;
    }

    // for x in list
    if (match(p, TK_FOR)) {
        Node* n  = node_new(p, ND_FOR);
        n->name  = eat(p, TK_IDENT)->val;
        eat(p, TK_IN);
        n->left  = parse_expr(p);
        n->body  = parse_block(p);
        return n;
    }

    // match
    if (match(p, TK_MATCH)) {
        Node* n = node_new(p, ND_MATCH);
        n->left = parse_expr(p);
        eat(p, TK_LBRACE);
        while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
            Node* arm   = node_new(p, ND_MATCH_ARM);
            arm->name   = eat(p, TK_IDENT)->val;  // pattern
            if (match(p, TK_LPAREN)) {
                arm->value = eat(p, TK_IDENT)->val;
                eat(p, TK_RPAREN);
            }
            arm->body   = parse_block(p);
            vec_push(&n->arms, arm);
        }
        eat(p, TK_RBRACE);
        return n;
    }

    // Variabile: nome = expr  o  nome: tipo = expr
    if (t->kind == TK_IDENT && (peek2(p)->kind == TK_ASSIGN || peek2(p)->kind == TK_COLON)) {
        Node* n = node_new(p, ND_VAR_DECL);
        n->name = advance(p)->val;
        if (match(p, TK_COLON)) n->type = parse_type(p);
        if (match(p, TK_ASSIGN)) n->left = parse_expr(p);
        return n;
    }

    // Espressione-statement
    Node* expr = parse_expr(p);
    Node* s    = node_new(p, ND_EXPR_STMT);
    s->left    = expr;
    return s;
}

// ── Top-level ────────────────────────────────────────────────────

static Node* parse_annotation(Parser* p) {
    Node* n   = node_new(p, ND_ANNOTATION);
    n->name   = peek(p)->val;  // già consumato il @ nel lexer, val è il nome
    advance(p);
    if (match(p, TK_LPAREN)) {
        if (check(p, TK_STRING)) n->value = advance(p)->val;
        eat(p, TK_RPAREN);
    }
    return n;
}

static Node* parse_top(Parser* p) {
    Node* ann = NULL;

    // Annotazione @nome(...)
    if (check(p, TK_AT)) {
        ann = parse_annotation(p);
    }

    Token* t = peek(p);

    // fn nome(params): tipo { body }
    if (match(p, TK_FN)) {
        Node* n      = node_new(p, ND_FN_DECL);
        n->name      = eat(p, TK_IDENT)->val;
        n->annotation = ann;
        eat(p, TK_LPAREN);
        while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
            Param* param  = (Param*)arena_alloc(p->arena, sizeof(Param));
            param->name   = eat(p, TK_IDENT)->val;
            if (match(p, TK_COLON)) param->type = parse_type(p);
            vec_push(&n->params, param);
            if (!match(p, TK_COMMA)) break;
        }
        eat(p, TK_RPAREN);
        if (match(p, TK_COLON)) n->type = parse_type(p);
        if (check(p, TK_LBRACE)) n->body = parse_block(p);
        return n;
    }

    // struct Nome { fields }
    if (match(p, TK_STRUCT)) {
        Node* n  = node_new(p, ND_STRUCT_DECL);
        n->name  = eat(p, TK_IDENT)->val;
        n->annotation = ann;
        eat(p, TK_LBRACE);
        while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
            Field* f = (Field*)arena_alloc(p->arena, sizeof(Field));
            f->name  = eat(p, TK_IDENT)->val;
            eat(p, TK_COLON);
            f->type  = parse_type(p);
            if (match(p, TK_ASSIGN)) f->value = parse_expr(p);
            vec_push(&n->fields, f);
            match(p, TK_COMMA);
        }
        eat(p, TK_RBRACE);
        return n;
    }

    // component Nome(props) { body }
    if (match(p, TK_COMPONENT)) {
        Node* n  = node_new(p, ND_COMPONENT_DECL);
        n->name  = eat(p, TK_IDENT)->val;
        n->annotation = ann;
        if (match(p, TK_LPAREN)) {
            while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                Param* param  = (Param*)arena_alloc(p->arena, sizeof(Param));
                param->name   = eat(p, TK_IDENT)->val;
                if (match(p, TK_COLON)) param->type = parse_type(p);
                vec_push(&n->params, param);
                if (!match(p, TK_COMMA)) break;
            }
            eat(p, TK_RPAREN);
        }
        n->body = parse_block(p);
        return n;
    }

    // Variabile globale
    if (t->kind == TK_IDENT) {
        Node* n = node_new(p, ND_VAR_DECL);
        n->name = advance(p)->val;
        n->annotation = ann;
        if (match(p, TK_COLON)) n->type = parse_type(p);
        if (match(p, TK_ASSIGN)) n->left = parse_expr(p);
        return n;
    }

    errlist_add(p->errors, t->line, t->col, "dichiarazione attesa, trovato '%s'", tk_name(t->kind));
    advance(p);
    return NULL;
}

// ── Entry point ──────────────────────────────────────────────────

Node* parse(Arena* a, TokenList tokens, ErrorList* errors) {
    Parser p = { tokens, 0, a, errors };
    Node*  prog = node_new(&p, ND_PROGRAM);

    while (!check(&p, TK_EOF)) {
        Node* n = parse_top(&p);
        if (n) vec_push(&prog->children, n);
    }
    return prog;
}
`,

    "parser/parser.h": `#pragma once
#include "../lexer/lexer.h"

// ── Tipi AST ─────────────────────────────────────────────────────

typedef enum {
    // Programma
    ND_PROGRAM,

    // Dichiarazioni
    ND_FN_DECL,
    ND_VAR_DECL,
    ND_STRUCT_DECL,
    ND_COMPONENT_DECL,
    ND_ANNOTATION,

    // Statement
    ND_BLOCK,
    ND_IF,
    ND_FOR,
    ND_RETURN,
    ND_MATCH,
    ND_MATCH_ARM,
    ND_EXPR_STMT,

    // Espressioni
    ND_BINARY,
    ND_UNARY,
    ND_CALL,
    ND_MEMBER,         // a.b
    ND_OPT_MEMBER,     // a?.b
    ND_INDEX,          // a[i]
    ND_ASSIGN,
    ND_CAST,           // x as int
    ND_PIPE,           // x |> f

    // Letterali
    ND_LIT_INT,
    ND_LIT_FLOAT,
    ND_LIT_STRING,
    ND_LIT_BOOL,
    ND_LIT_NULL,
    ND_LIT_ARRAY,      // [1, 2, 3]
    ND_LIT_MAP,        // { "k": v }
    ND_IDENT,

    // Tipi
    ND_TYPE_PRIM,      // str, int, float, bool, void, any
    ND_TYPE_ARRAY,     // T[]
    ND_TYPE_MAP,       // {K: V}
    ND_TYPE_OPTIONAL,  // ?T
    ND_TYPE_UNION,     // T | err
    ND_TYPE_FN,        // fn(T): R
    ND_TYPE_NAMED,     // Utente

    // UI
    ND_UI_NODE,
    ND_STYLE_PROP,
    ND_STYLE_BLOCK,
} NodeKind;

typedef struct Node Node;
typedef struct Param Param;
typedef struct Field Field;

struct Param {
    const char* name;
    Node*       type;   // tipo opzionale
};

struct Field {
    const char* name;
    Node*       type;
    Node*       value;  // valore default opzionale
};

struct Node {
    NodeKind    kind;
    int         line;

    // Nome/valore (per ident, literal, annotation, ecc.)
    const char* name;
    const char* value;
    const char* op;

    // Figli generici
    Node*  left;
    Node*  right;
    Node*  cond;
    Node*  body;
    Node*  else_body;
    Node*  type;        // tipo dichiarato
    Node*  annotation;  // @native("...") ecc.

    // Liste
    Vec    children;    // body di blocchi, argomenti, ecc.
    Vec    params;      // parametri funzione (Param*)
    Vec    fields;      // campi struct (Field*)
    Vec    arms;        // match arms
    Vec    style;       // style props (Field*)

    // Flags
    bool   is_exported;
};

// ── Parser ───────────────────────────────────────────────────────

typedef struct {
    TokenList   tokens;
    int         pos;
    Arena*      arena;
    ErrorList*  errors;
} Parser;

Node* parse(Arena* a, TokenList tokens, ErrorList* errors);
`,

}
