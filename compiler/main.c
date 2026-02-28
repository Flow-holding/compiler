#include "codegen.h"
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
  #include <process.h>
  #define PATH_SEP "\\"
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
    buf[size] = '\0';
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
    char* last2 = strrchr(out, '\\');
    char* sep = last > last2 ? last : last2;
    if (sep) *sep = '\0'; else strncpy(out, ".", max);
}

static void path_stem(char* out, int max, const char* path) {
    const char* base = strrchr(path, '/');
    const char* base2 = strrchr(path, '\\');
    const char* b = base > base2 ? base : base2;
    if (b) b++; else b = path;
    strncpy(out, b, max);
    char* dot = strrchr(out, '.');
    if (dot) *dot = '\0';
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
        fprintf(stderr, "flowc: file non trovato \"%s\"\n", input_path);
        exit(1);
    }

    // Setup arena + error list
    Arena     arena  = arena_new(4 * 1024 * 1024);  // 4MB
    ErrorList errors = errlist_new();

    // ── Lexer
    TokenList tokens = lex(&arena, source, &errors);
    if (errors.len > 0) {
        for (int i = 0; i < errors.len; i++)
            fprintf(stderr, "  ✗ [%d:%d] %s\n",
                errors.data[i].line, errors.data[i].col, errors.data[i].msg);
        exit(1);
    }

    // ── Parser
    Node* ast = parse(&arena, tokens, &errors);
    if (errors.len > 0) {
        for (int i = 0; i < errors.len; i++)
            fprintf(stderr, "  ✗ [%d:%d] %s\n",
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
    snprintf(cmd, sizeof(cmd), "clang \"%s\" -o \"%s\" %s -D_CRT_SECURE_NO_WARNINGS -w",
        c_path, exe_path, prod ? "-O2" : "-g");
    int rc = run_cmd(cmd);

    if (rc != 0) {
        fprintf(stderr, "✗ Errore compilazione nativa\n");
    } else {
        printf("  .exe    %s\n", exe_path);
    }

    // ── Codegen WASM
    char wasm_c[512]; snprintf(wasm_c, sizeof(wasm_c), "%s/%s.wasm.c", out_dir, stem);
    char wasm_path[512]; snprintf(wasm_path, sizeof(wasm_path), "%s/app.wasm", out_dir);
    Str wasm_code = codegen_c(&arena, ast, TARGET_WASM, runtime_dir);
    write_file(wasm_c, wasm_code.data);

    snprintf(cmd, sizeof(cmd),
        "clang --target=wasm32-unknown-unknown -nostdlib "
        "-Wl,--no-entry -Wl,--allow-undefined -Wl,--export-dynamic "
        "-D_CRT_SECURE_NO_WARNINGS -w %s \"%s\" -o \"%s\"",
        prod ? "-O2" : "-O1", wasm_c, wasm_path);
    if (run_cmd(cmd) == 0) printf("  .wasm   %s\n", wasm_path);

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

        printf("  html    %s\n", html_path);
        printf("  css     %s\n", css_path);
        printf("  js      %s\n", js_path);
    }

    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;
    printf("\n✓ Build completata in %.0fms\n\n", elapsed);

    // ── Esegui se --run
    if (do_run && rc == 0) {
        printf("── output ──────────────────────────\n");
        run_cmd(exe_path);
        printf("────────────────────────────────────\n");
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
        fprintf(stderr, "uso: flowc <file.flow> [--run] [--prod]\n");
        return 1;
    }

    const char* input_path  = NULL;
    bool        do_run      = false;
    bool        prod        = false;

    for (int i = 1; i < argc; i++) {
        if (str_eq(argv[i], "--run"))  { do_run = true; continue; }
        if (str_eq(argv[i], "--prod")) { prod = true;   continue; }
        if (!input_path) input_path = argv[i];
    }

    if (!input_path) {
        fprintf(stderr, "flowc: specifica un file .flow\n");
        return 1;
    }

    // Cartella output accanto al sorgente
    char out_dir[512];
    char dir[512]; path_dir(dir, sizeof(dir), input_path);
    snprintf(out_dir, sizeof(out_dir), "%s/out", dir);

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
