#include "codegen.h"
#include <stdarg.h>

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
    str_catc(out, '\n');
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
            snprintf(buf, sizeof(buf), "flow_str_new(\"%s\")", n->value ? n->value : "");
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
            str_cat(out, ";\n");
            break;
        }

        case ND_EXPR_STMT: {
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            gen_expr(out, n->left);
            str_cat(out, ";\n");
            break;
        }

        case ND_RETURN: {
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "return");
            if (n->left) { str_cat(out, " "); gen_expr(out, n->left); }
            str_cat(out, ";\n");
            break;
        }

        case ND_IF: {
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "if ("); gen_expr(out, n->cond); str_cat(out, ") {\n");
            gen_block(out, n->body, ind + 1);
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "}");
            if (n->else_body) {
                str_cat(out, " else {\n");
                gen_block(out, n->else_body, ind + 1);
                for (int i = 0; i < ind; i++) str_cat(out, "    ");
                str_cat(out, "}");
            }
            str_cat(out, "\n");
            break;
        }

        case ND_FOR: {
            // for item in lista → FlowArray* loop
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "for (int _i = 0; _i < ");
            gen_expr(out, n->left);
            str_cat(out, "->len; _i++) {\n");
            for (int i = 0; i < ind + 1; i++) str_cat(out, "    ");
            str_cat(out, "FlowObject* "); str_cat(out, n->name);
            str_cat(out, " = flow_arr_get(");
            gen_expr(out, n->left);
            str_cat(out, ", _i);\n");
            gen_block(out, n->body, ind + 1);
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "}\n");
            break;
        }

        case ND_BLOCK:
            gen_block(out, n, ind);
            break;

        default:
            for (int i = 0; i < ind; i++) str_cat(out, "    ");
            str_cat(out, "/* stmt? */\n");
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
                str_catf(out, "__attribute__((export_name(\"%s\")))\n", n->name);

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
            str_cat(out, ") {\n");
            if (n->body) gen_block(out, n->body, 1);
            str_cat(out, "}\n\n");
            break;
        }

        case ND_VAR_DECL: {
            str_cat(out, gen_type(n->type));
            str_cat(out, " "); str_cat(out, n->name);
            if (n->left) { str_cat(out, " = "); gen_expr(out, n->left); }
            str_cat(out, ";\n\n");
            break;
        }

        case ND_STRUCT_DECL: {
            str_catf(out, "typedef struct {\n    FlowObject base;\n");
            for (int i = 0; i < n->fields.len; i++) {
                Field* f = (Field*)n->fields.data[i];
                str_catf(out, "    %s %s;\n", gen_type(f->type), f->name);
            }
            str_catf(out, "} Flow_%s;\n\n", n->name);
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
    for (int i = 0; rt[i]; i++) if (rt[i] == '\\') rt[i] = '/';

    if (target == TARGET_WASM) {
        str_catf(&out, "#include \"%s/wasm/runtime.c\"\n\n", rt);
    } else {
        str_catf(&out, "#include \"%s/io/print.c\"\n", rt);
        str_catf(&out, "#include \"%s/io/fs.c\"\n", rt);
        str_catf(&out, "#include \"%s/net/http.c\"\n\n", rt);
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
            str_catf(html, " src=\"%s\"", f->value->value ? f->value->value : "");
        } else if (str_eq(f->name, "placeholder") && f->value) {
            str_catf(html, " placeholder=\"%s\"", f->value->value ? f->value->value : "");
        }
    }
    str_cat(html, ">\n");

    // Testo interno
    for (int i = 0; i < n->style.len; i++) {
        Field* f = (Field*)n->style.data[i];
        if (str_eq(f->name, "text") && f->value && f->value->value) {
            for (int j = 0; j < ind + 1; j++) str_cat(html, "  ");
            str_catf(html, "%s\n", f->value->value);
        }
    }

    // Figli
    for (int i = 0; i < n->children.len; i++)
        gen_ui_node(html, (Node*)n->children.data[i], ind + 1);

    for (int i = 0; i < ind; i++) str_cat(html, "  ");
    str_catf(html, "</%s>\n", tag);
}

Str codegen_html(Arena* a, Node* program) {
    Str out = str_new();
    str_cat(&out,
        "<!DOCTYPE html>\n<html lang=\"it\">\n<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "  <title>Flow App</title>\n"
        "  <link rel=\"stylesheet\" href=\"style.css\">\n"
        "</head>\n<body>\n  <div id=\"root\">\n");

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
        "  </div>\n"
        "  <script src=\"app.js\"></script>\n"
        "</body>\n</html>\n");
    return out;
}

// ── CSS Generator ────────────────────────────────────────────────

Str codegen_css(Arena* a, Node* program) {
    Str out = str_new();
    str_cat(&out,
        "*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }\n"
        "body { font-family: system-ui, sans-serif; }\n"
        "#root { display: flex; flex-direction: column; min-height: 100vh; }\n\n");
    return out;
}

// ── JS Generator ─────────────────────────────────────────────────

Str codegen_js(Arena* a, Node* program) {
    Str out = str_new();
    str_cat(&out,
        "const memory = new WebAssembly.Memory({ initial: 16 });\n"
        "const imports = {\n"
        "  env: {\n"
        "    memory,\n"
        "    flow_js_print: (ptr, len) => {\n"
        "      const buf = new Uint8Array(memory.buffer, ptr, len);\n"
        "      console.log(new TextDecoder().decode(buf));\n"
        "    },\n"
        "    flow_js_eprint: (ptr, len) => {\n"
        "      const buf = new Uint8Array(memory.buffer, ptr, len);\n"
        "      console.error(new TextDecoder().decode(buf));\n"
        "    },\n"
        "  }\n"
        "};\n\n"
        "WebAssembly.instantiateStreaming(fetch('app.wasm'), imports)\n"
        "  .then(({ instance }) => {\n"
        "    window._flow = instance.exports;\n"
        "    if (instance.exports.main) instance.exports.main();\n"
        "  })\n"
        "  .catch(err => console.error('WASM error:', err));\n");
    return out;
}
