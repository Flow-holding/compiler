#include <stdio.h>
#include <string.h>
#include "cli.h"
#include "config.h"
#include "cmd/cmd_init.h"
#include "cmd/cmd_build.h"
#include "cmd/cmd_dev.h"
#include "cmd/cmd_update.h"
#ifdef _WIN32
  #include <windows.h>
#endif

static const char *HELP =
    "\nFlow v" FLOW_VERSION " — linguaggio e framework universale\n"
    "\nCOMANDI:\n"
    "  flow init [nome]    Crea un nuovo progetto\n"
    "  flow run            Compila ed esegue\n"
    "  flow build          Build produzione\n"
    "  flow dev            Server dev (WASM)\n"
    "  flow update         Aggiorna alla versione piu recente\n"
    "  flow help           Mostra questo messaggio\n"
    "\nESEMPI:\n"
    "  flow init mia-app\n"
    "  cd mia-app\n"
    "  flow dev\n\n";

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    /* Raccoglie override globali da qualsiasi posizione in argv */
    for (int i = 2; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--runtime")) g_cli_runtime = argv[i + 1];
        if (!strcmp(argv[i], "--flow"))    g_cli_flow    = argv[i + 1];
    }

    const char *cmd = argc > 1 ? argv[1] : NULL;

    if (!cmd
     || !strcmp(cmd, "help")
     || !strcmp(cmd, "--help")
     || !strcmp(cmd, "-h")) {
        printf("%s", HELP);
        return 0;
    }

    if (!strcmp(cmd, "init"))   return cmd_init(argc > 2 ? argv[2] : NULL);
    if (!strcmp(cmd, "build"))  return cmd_build(1, 0, 0, 0);
    if (!strcmp(cmd, "run"))    return cmd_build(0, 1, 0, 0);
    if (!strcmp(cmd, "dev"))    return cmd_dev();
    if (!strcmp(cmd, "update")) return cmd_update();

    fprintf(stderr, C_RED "✗" C_RESET " Comando sconosciuto: \"%s\"\n", cmd);
    fprintf(stderr, "  Usa \"flow help\" per vedere i comandi disponibili\n");
    return 1;
}
