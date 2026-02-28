#include "cmd_init.h"
#include "util.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Template files ─────────────────────────────────────────────────────── */

static const char *TMPL_CONFIG =
    "{\n"
    "    \"name\":    \"{{name}}\",\n"
    "    \"version\": \"0.1.0\",\n"
    "    \"entry\":   \"src/main.flow\",\n"
    "    \"target\":  [\"web\"],\n"
    "    \"out\":     \"out\"\n"
    "}\n";

static const char *TMPL_MAIN =
    "// {{name}}\n"
    "\n"
    "fn main() {\n"
    "    print(\"Ciao da {{name}}\")\n"
    "}\n"
    "\n"
    "@client\n"
    "component App() {\n"
    "    let count = state.use(0)\n"
    "\n"
    "    return Column {\n"
    "        style: style({ padding: 24, gap: 16 })\n"
    "\n"
    "        Text {\n"
    "            value: \"Benvenuto in {{name}}\"\n"
    "            style: style({ size: 28, weight: \"bold\", color: \"#111\" })\n"
    "        }\n"
    "\n"
    "        Button {\n"
    "            text: \"Conta: {count}\"\n"
    "            onPress: fn() { count += 1 }\n"
    "            style: style({\n"
    "                bg:      \"#3b82f6\"\n"
    "                color:   \"#fff\"\n"
    "                radius:  8\n"
    "                padding: 12\n"
    "                hover:   { bg: \"#2563eb\" }\n"
    "            })\n"
    "        }\n"
    "    }\n"
    "}\n";

static const char *TMPL_GITIGNORE =
    "out/\n"
    "*.exe\n"
    "*.wasm\n"
    "*.o\n";

/* ── Helper ─────────────────────────────────────────────────────────────── */

static int write_template(const char *path, const char *tmpl, const char *name) {
    char *content = str_replace(tmpl, "{{name}}", name);
    if (!content) return -1;
    int r = write_file(path, content);
    free(content);
    return r;
}

/* ── Command ────────────────────────────────────────────────────────────── */

int cmd_init(const char *name) {
    if (!name || name[0] == 0) name = "my-flow-app";

    char *cwd     = get_cwd();
    char *projdir = path_join(cwd, name);
    free(cwd);

    if (path_exists(projdir)) {
        fprintf(stderr, C_RED "✗" C_RESET " cartella \"%s\" esiste già\n", name);
        free(projdir);
        return 1;
    }

    /* Crea struttura */
    char *srcdir    = path_join(projdir, "src");
    char *assetsdir = path_join(projdir, "assets");
    mkdir_p(srcdir);
    mkdir_p(assetsdir);

    /* Scrivi file dal template */
    char *config  = path_join(projdir, "flow.config.json");
    char *main_f  = path_join(srcdir,  "main.flow");
    char *gitign  = path_join(projdir, ".gitignore");

    write_template(config, TMPL_CONFIG,    name);
    write_template(main_f, TMPL_MAIN,      name);
    write_file(gitign,     TMPL_GITIGNORE);

    free(config); free(main_f); free(gitign);
    free(srcdir); free(assetsdir); free(projdir);

    printf("\n" C_GREEN "✓" C_RESET " Progetto \"%s\" creato\n\n", name);
    printf("  cd %s\n", name);
    printf("  flow dev\n\n");
    return 0;
}
