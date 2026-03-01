#include "codegen.h"

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
    { "mkdir",   "flow_fs_mkdir"    },
    { "rmdir",   "flow_fs_rmdir"    },
    { "list_dir","flow_fs_list_dir" },
    { "is_dir",  "flow_fs_is_dir"   },
    { "is_file", "flow_fs_is_file"  },
    { "size",    "flow_fs_size"     },
    { "spawn",   "flow_process_spawn"     },
    { "spawn_wait","flow_process_spawn_wait"},
    { "exit",    "flow_process_exit" },
    { "env",     "flow_process_env"  },
    { "cwd",     "flow_process_cwd"  },
    { "chdir",   "flow_process_chdir"},
    { NULL, NULL }
};

#define DYN_MAP_MAX 256
static char dyn_map_keys[DYN_MAP_MAX][64];
static char dyn_map_vals[DYN_MAP_MAX][64];
static int  dyn_map_n = 0;

static void load_dyn_native_map(const char* path) {
    dyn_map_n = 0;
    if (!path) return;
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (dyn_map_n < DYN_MAP_MAX && fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = line;
        char* val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;
        char* end = val + strlen(val);
        while (end > val && (end[-1] == '\n' || end[-1] == '\r')) *--end = '\0';
        if (strlen(key) < 60 && strlen(val) < 60) {
            strcpy(dyn_map_keys[dyn_map_n], key);
            strcpy(dyn_map_vals[dyn_map_n], val);
            dyn_map_n++;
        }
    }
    fclose(f);
}

static const char* resolve_native(const char* base, const char* member) {
    char key[128];
    if (base && base[0]) {
        snprintf(key, sizeof(key), "%s.%s", base, member);
    } else {
        snprintf(key, sizeof(key), "%s", member ? member : "");
    }
    for (int i = 0; i < dyn_map_n; i++)
        if (str_eq(key, dyn_map_keys[i])) return dyn_map_vals[i];
    for (int i = 0; native_map[i][0]; i++)
        if (str_eq(member ? member : key, native_map[i][0])) return native_map[i][1];
    return member ? member : key;
}

// Per WASM: ritorna stub (default) per funzioni native-only, altrimenti NULL
static const char* native_only_stub(CodegenTarget target, const char* base, const char* member) {
    if (target != TARGET_WASM) return NULL;
    if (!base || !member) return NULL;
    if (str_eq(base, "fs")) {
        if (str_eq(member, "read"))       return "NULL";
        if (str_eq(member, "write"))     return "0";
        if (str_eq(member, "exists"))    return "0";
        if (str_eq(member, "delete"))    return "0";
        if (str_eq(member, "mkdir"))     return "0";
        if (str_eq(member, "rmdir"))     return "0";
        if (str_eq(member, "list_dir"))  return "flow_array_new()";
        if (str_eq(member, "is_dir"))    return "0";
        if (str_eq(member, "is_file"))   return "0";
        if (str_eq(member, "size"))      return "0";
    }
    if (str_eq(base, "process")) {
        if (str_eq(member, "spawn"))      return "0";
        if (str_eq(member, "spawn_wait")) return "-1";
        if (str_eq(member, "exit"))       return "__builtin_unreachable()";
        if (str_eq(member, "env"))        return "NULL";
        if (str_eq(member, "cwd"))        return "flow_str_new(\"\")";
        if (str_eq(member, "chdir"))      return "0";
    }
    return NULL;
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

static CodegenTarget _target = TARGET_NATIVE;

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
            // Su WASM: sostituisci fs/process con stub (default)
            const char* base = (n->left && n->left->kind == ND_IDENT) ? n->left->name : NULL;
            const char* stub = native_only_stub(_target, base, n->name);
            if (stub) {
                str_cat(out, stub);
            } else {
                str_cat(out, resolve_native(base, n->name ? n->name : ""));
                str_cat(out, "(");
                for (int i = 0; i < n->children.len; i++) {
                    if (i > 0) str_cat(out, ", ");
                    gen_expr(out, (Node*)n->children.data[i]);
                }
                str_cat(out, ")");
            }
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

            // Attributo export per WASM (main esportato via -Wl,--export=main)
            if (target == TARGET_WASM && !str_eq(n->name, "main"))
                str_catf(out, "__attribute__((export_name(\"%s\")))\n", n->name);

            // main() deve restituire int in C
            const char* ret = str_eq(n->name, "main") ? "int" : gen_type(n->type);
            str_cat(out, ret);
            str_cat(out, " "); str_cat(out, resolve_native(NULL, n->name)); str_cat(out, "(");
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

Str codegen_c(Arena* a, Node* program, CodegenTarget target, const char* runtime_dir, const char* native_map_path) {
    _target = target;
    load_dyn_native_map(native_map_path);
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
        str_catf(&out, "#include \"%s/os/process.c\"\n", rt);
        str_catf(&out, "#include \"%s/net/http.c\"\n\n", rt);
    }

    for (int i = 0; i < program->children.len; i++)
        gen_top(&out, (Node*)program->children.data[i], target);

    return out;
}

// ── HTML Generator ───────────────────────────────────────────────

static int html_compact = 1;  // minify: no indent/newlines

// Dichiara prima di gen_ui_node (mutual recursion)
static void gen_ui_node(Str* html, Node* n, int ind);

static void gen_show(Str* html, Node* n, int ind) {
    const char* when_expr = "false";
    for (int i = 0; i < n->style.len; i++) {
        Field* f = (Field*)n->style.data[i];
        if (str_eq(f->name, "when") && f->value)
            when_expr = f->value->value ? f->value->value :
                        f->value->name  ? f->value->name  : "false";
    }
    if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
    str_catf(html, "<div data-fl-show=\"%s\">%s", when_expr, html_compact ? "" : "\n");
    for (int i = 0; i < n->children.len; i++)
        gen_ui_node(html, (Node*)n->children.data[i], ind + 1);
    if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
    str_cat(html, html_compact ? "</div>" : "</div>\n");
}

static void gen_switch_flow(Str* html, Node* n, int ind) {
    const char* val_expr = "";
    for (int i = 0; i < n->style.len; i++) {
        Field* f = (Field*)n->style.data[i];
        if (str_eq(f->name, "value") && f->value)
            val_expr = f->value->value ? f->value->value :
                       f->value->name  ? f->value->name  : "";
    }
    if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
    str_catf(html, "<div data-fl-switch=\"%s\">%s", val_expr, html_compact ? "" : "\n");
    for (int i = 0; i < n->children.len; i++) {
        Node* child = (Node*)n->children.data[i];
        if (child->kind == ND_UI_NODE && str_eq(child->name, "match")) {
            const char* when_val = "";
            bool is_default_arm = true;
            for (int j = 0; j < child->style.len; j++) {
                Field* f = (Field*)child->style.data[j];
                if (str_eq(f->name, "when") && f->value) {
                    when_val = f->value->value ? f->value->value : "";
                    is_default_arm = false;
                }
            }
            if (!html_compact) { for (int k = 0; k < ind + 1; k++) str_cat(html, "  "); }
            if (is_default_arm)
                str_catf(html, "<div data-fl-match-default>%s", html_compact ? "" : "\n");
            else
                str_catf(html, "<div data-fl-match=\"%s\">%s", when_val, html_compact ? "" : "\n");
            for (int j = 0; j < child->children.len; j++)
                gen_ui_node(html, (Node*)child->children.data[j], ind + 2);
            if (!html_compact) { for (int k = 0; k < ind + 1; k++) str_cat(html, "  "); }
            str_cat(html, html_compact ? "</div>" : "</div>\n");
        }
    }
    if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
    str_cat(html, html_compact ? "</div>" : "</div>\n");
}

static void gen_each(Str* html, Node* n, int ind) {
    const char* items_expr = "";
    const char* as_name    = "item";
    for (int i = 0; i < n->style.len; i++) {
        Field* f = (Field*)n->style.data[i];
        if (str_eq(f->name, "items") && f->value)
            items_expr = f->value->value ? f->value->value :
                         f->value->name  ? f->value->name  : "";
        if (str_eq(f->name, "as") && f->value)
            as_name = f->value->value ? f->value->value :
                      f->value->name  ? f->value->name  : "item";
    }
    if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
    str_catf(html, "<template data-fl-each=\"%s\" data-fl-as=\"%s\">%s",
             items_expr, as_name, html_compact ? "" : "\n");
    for (int i = 0; i < n->children.len; i++)
        gen_ui_node(html, (Node*)n->children.data[i], ind + 1);
    if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
    str_cat(html, html_compact ? "</template>" : "</template>\n");
}

static void gen_ui_node(Str* html, Node* n, int ind) {
    if (!n) return;

    // Fragment: renderizza tutti i figli direttamente
    if (n->kind == ND_FRAGMENT) {
        for (int i = 0; i < n->children.len; i++)
            gen_ui_node(html, (Node*)n->children.data[i], ind);
        return;
    }

    if (n->kind != ND_UI_NODE) return;

    /* <App /> — router outlet */
    if (str_eq(n->name, "App")) {
        if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
        str_cat(html, "<div id=\"fl-outlet\"></div>");
        str_cat(html, html_compact ? "" : "\n");
        return;
    }

    /* Control flow built-ins */
    if (str_eq(n->name, "show"))   { gen_show(html, n, ind);        return; }
    if (str_eq(n->name, "switch")) { gen_switch_flow(html, n, ind); return; }
    if (str_eq(n->name, "each"))   { gen_each(html, n, ind);        return; }

    /* Mappatura tag — minuscolo (nuova sintassi) + maiuscolo (legacy) */
    const char* tag      = "div";
    const char* fl_class = NULL;
    if      (str_eq(n->name, "text")   || str_eq(n->name, "Text"))   tag = "p";
    else if (str_eq(n->name, "button") || str_eq(n->name, "Button")) tag = "button";
    else if (str_eq(n->name, "input")  || str_eq(n->name, "Input"))  tag = "input";
    else if (str_eq(n->name, "img")    || str_eq(n->name, "Image"))  tag = "img";
    else if (str_eq(n->name, "link"))                                 tag = "a";
    else if (str_eq(n->name, "col"))    { tag = "div"; fl_class = "fl-col";    }
    else if (str_eq(n->name, "row"))    { tag = "div"; fl_class = "fl-row";    }
    else if (str_eq(n->name, "stack"))  { tag = "div"; fl_class = "fl-stack";  }
    else if (str_eq(n->name, "scroll")) { tag = "div"; fl_class = "fl-scroll"; }
    else if (str_eq(n->name, "grid"))   { tag = "div"; fl_class = "fl-grid";   }
    else if (str_eq(n->name, "Row")    || str_eq(n->name, "Column")) tag = "div";

    if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
    str_catf(html, "<%s", tag);
    if (fl_class) str_catf(html, " class=\"%s\"", fl_class);

    for (int i = 0; i < n->style.len; i++) {
        Field* f = (Field*)n->style.data[i];
        if (str_eq(f->name, "text") || str_eq(f->name, "value") ||
            str_eq(f->name, "style")) continue;
        if (str_eq(f->name, "src") && f->value)
            str_catf(html, " src=\"%s\"", f->value->value ? f->value->value : "");
        else if (str_eq(f->name, "placeholder") && f->value)
            str_catf(html, " placeholder=\"%s\"", f->value->value ? f->value->value : "");
        else if (f->name && f->name[0] == 'o' && f->name[1] == 'n' && f->name[2] == ':') {
            // on:event={handler} → data-fl-on-event="code"
            char attr[64];
            snprintf(attr, sizeof(attr), " data-fl-on-%s", f->name + 3);
            str_cat(html, attr);
        }
    }
    str_cat(html, html_compact ? ">" : ">\n");

    for (int i = 0; i < n->style.len; i++) {
        Field* f = (Field*)n->style.data[i];
        if ((str_eq(f->name, "text") || str_eq(f->name, "value")) && f->value && f->value->value) {
            if (!html_compact) { for (int j = 0; j < ind + 1; j++) str_cat(html, "  "); }
            str_catf(html, "%s%s", f->value->value, html_compact ? "" : "\n");
        }
    }

    for (int i = 0; i < n->children.len; i++)
        gen_ui_node(html, (Node*)n->children.data[i], ind + 1);

    if (!html_compact) { for (int i = 0; i < ind; i++) str_cat(html, "  "); }
    str_catf(html, "</%s>%s", tag, html_compact ? "" : "\n");
}

Str codegen_html(Arena* a, Node* program) {
    Str out = str_new();
    Str css = codegen_css(a, program);
    str_cat(&out,
        "<!DOCTYPE html><html lang=\"it\"><head>"
        "<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Flow App</title>"
        "<link rel=\"preload\" href=\"app.wasm\" as=\"fetch\" crossorigin fetchpriority=\"high\">"
        "<style>");
    str_cat(&out, css.data);
    str_cat(&out,
        "</style></head><body><div id=\"root\">");

    for (int i = 0; i < program->children.len; i++) {
        Node* n = (Node*)program->children.data[i];
        if (n->kind != ND_COMPONENT_DECL) continue;

        /* Renderizza solo componenti default/root o con @client (legacy) */
        bool is_root = n->is_default || str_eq(n->name, "root");
        bool is_client = n->annotation && str_eq(n->annotation->name, "client");
        if (!is_root && !is_client) continue;

        if (n->body) {
            for (int j = 0; j < n->body->children.len; j++) {
                Node* stmt = (Node*)n->body->children.data[j];
                Node* root = (stmt->kind == ND_RETURN && stmt->left) ? stmt->left : stmt;
                gen_ui_node(&out, root, 2);
            }
        }
    }

    str_cat(&out,
        "</div><script async src=\"app.js\"></script></body></html>");
    return out;
}

// ── CSS Generator ────────────────────────────────────────────────

Str codegen_css(Arena* a, Node* program) {
    (void)program;
    Str out = str_new();
    str_cat(&out,
        /* Reset */
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:system-ui,-apple-system,sans-serif;background:#fff;color:#111}"
        "#root{display:flex;flex-direction:column;min-height:100vh}"
        /* Layout built-ins */
        ".fl-col{display:flex;flex-direction:column}"
        ".fl-row{display:flex;flex-direction:row;align-items:center}"
        ".fl-stack{position:relative}"
        ".fl-scroll{overflow:auto}"
        ".fl-grid{display:grid}"
        /* Control flow — nascosti di default, il JS runtime li mostra */
        "[data-fl-show]{display:none}"
        "[data-fl-match]{display:none}"
        "[data-fl-match-default]{display:none}"
    );
    return out;
}

// ── JS Generator (WASM loader) ───────────────────────────────────

Str codegen_js(Arena* a, Node* program) {
    (void)program;
    Str out = str_new();
    str_cat(&out,
        "let wasmMem;const imports={env:{"
        "flow_print_str:ptr=>{const v=new Uint8Array(wasmMem.buffer);let e=ptr;while(v[e])e++;console.log(new TextDecoder().decode(v.subarray(ptr,e)));},"
        "flow_eprint_str:ptr=>{const v=new Uint8Array(wasmMem.buffer);let e=ptr;while(v[e])e++;console.error(new TextDecoder().decode(v.subarray(ptr,e)));}"
        "}};"
        "function loadWasm(){WebAssembly.instantiateStreaming(fetch('app.wasm'),imports)"
        ".then(({instance})=>{wasmMem=instance.exports.memory;window._flow=instance.exports;if(instance.exports.main)instance.exports.main();})"
        ".catch(e=>console.error('WASM error:',e));}"
        "document.readyState==='loading'?document.addEventListener('DOMContentLoaded',loadWasm):loadWasm();");
    return out;
}
