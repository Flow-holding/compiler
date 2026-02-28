#include "cmd_update.h"
#include "util.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
  #include <sys/stat.h>
#endif

/* Piattaforma e arch corrente */
#ifdef _WIN32
  #define PLATFORM "windows"
  #define ASSET_EXT ".exe"
#elif defined(__APPLE__)
  #define PLATFORM "macos"
  #define ASSET_EXT ""
#else
  #define PLATFORM "linux"
  #define ASSET_EXT ""
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
  #define ARCH "arm64"
#else
  #define ARCH "x64"
#endif

#define API_URL  "https://api.github.com/repos/" FLOW_REPO "/releases/latest"

/* Estrae il valore di una chiave stringa da JSON grezzo */
static char *json_str(const char *json, const char *key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    if (*p != '"') return NULL;
    p++;
    const char *end = p;
    while (*end && *end != '"') { if (*end == '\\') end++; if (*end) end++; }
    size_t len = end - p;
    char *v = malloc(len + 1);
    memcpy(v, p, len); v[len] = 0;
    return v;
}

/* Trova la download URL per l'asset con quel nome */
static char *find_asset_url(const char *json, const char *asset_name) {
    const char *p = strstr(json, asset_name);
    if (!p) return NULL;
    /* browser_download_url viene dopo il nome nell'oggetto asset */
    const char *q = strstr(p, "browser_download_url");
    if (!q) return NULL;
    q += strlen("browser_download_url");
    while (*q == '"' || *q == ':' || *q == ' ') q++;
    if (*q != '"') return NULL;
    q++;
    const char *end = q;
    while (*end && *end != '"') end++;
    size_t len = end - q;
    char *url = malloc(len + 1);
    memcpy(url, q, len); url[len] = 0;
    return url;
}

/* Scarica un URL in un file temporaneo usando curl */
static int curl_download(const char *url, const char *outfile, const char *ua) {
    const char *argv[] = {
        "curl", "-fsSL",
        "-H", ua,
        "-o", outfile,
        url, NULL
    };
    return run_cmd(argv);
}

int cmd_update(void) {
    printf(C_CYAN "flow update" C_RESET " — versione attuale: "
           C_YELLOW FLOW_VERSION C_RESET "\n");
    printf("Controllo aggiornamenti...");
    fflush(stdout);

    /* 1. Scarica release JSON */
    char *home    = get_flow_home();
    char *tmpjson = str_fmt("%s" SEP "update-check.json", home);
    mkdir_p(home);

    if (curl_download(API_URL, tmpjson, "User-Agent: flow-cli") != 0) {
        printf(" " C_YELLOW "Impossibile raggiungere il server\x1b[0m\n");
        free(tmpjson); free(home);
        return 1;
    }

    char *json = read_file(tmpjson);
    free(tmpjson);
    if (!json) { printf(" errore lettura\n"); free(home); return 1; }

    /* 2. Estrai versione */
    char *tag = json_str(json, "tag_name");
    if (!tag) {
        printf(" errore JSON\n");
        free(json); free(home);
        return 1;
    }
    /* rimuovi 'v' iniziale */
    char *version = tag[0] == 'v' ? str_dup(tag + 1) : str_dup(tag);
    free(tag);

    if (strcmp(version, FLOW_VERSION) == 0) {
        printf(" " C_GREEN "✓ Già all'ultima versione (%s)\x1b[0m\n", version);
        free(version); free(json); free(home);
        return 0;
    }
    printf(" " C_GREEN "%s disponibile!\x1b[0m\n", version);

    /* 3. Trova URL download per questa piattaforma */
    char *asset_name = str_fmt("flow-%s-%s%s", PLATFORM, ARCH, ASSET_EXT);
    char *dl_url     = find_asset_url(json, asset_name);
    free(json);

    if (!dl_url) {
        fprintf(stderr, C_RED "✗" C_RESET " asset non trovato: %s\n", asset_name);
        free(asset_name); free(version); free(home);
        return 1;
    }
    printf("Download " C_CYAN "%s" C_RESET "...\n", asset_name);
    free(asset_name);

    /* 4. Scarica il nuovo binario */
    char *tmpbin = str_fmt("%s" SEP "flow-new" ASSET_EXT, home);
    if (curl_download(dl_url, tmpbin, "User-Agent: flow-cli") != 0) {
        fprintf(stderr, C_RED "✗" C_RESET " download fallito: %s\n", dl_url);
        free(dl_url); free(tmpbin); free(version); free(home);
        return 1;
    }
    free(dl_url);

    /* 5. Sostituisci il binario corrente */
    char exe[4096];
    if (get_exe_path(exe, sizeof(exe)) != 0) {
        fprintf(stderr, C_RED "✗" C_RESET " impossibile trovare il percorso del binario\n");
        free(tmpbin); free(version); free(home);
        return 1;
    }

#ifdef _WIN32
    /* Su Windows: muovi a .new e usa un .bat per la sostituzione */
    char *exenew = str_fmt("%s.new", exe);
    char *bat    = str_fmt("%s-update.bat", exe);

    /* Rinomina tmpbin → exe.new */
    const char *mv_argv[] = { "move", "/y", tmpbin, exenew, NULL };
    run_cmd(mv_argv);

    /* Scrivi batch file per sostituzione dopo l'uscita */
    char *bat_content = str_fmt(
        "@echo off\r\n"
        "timeout /t 1 /nobreak >nul\r\n"
        "move /y \"%s\" \"%s\"\r\n"
        "echo flow aggiornato a v%s\r\n"
        "del \"%%~f0\"\r\n",
        exenew, exe, version
    );
    write_file(bat, bat_content);
    free(bat_content);

    /* Avvia il bat in background */
    const char *bat_argv[] = { "cmd", "/c", "start", "/min", bat, NULL };
    run_cmd(bat_argv);

    printf(C_GREEN "✓" C_RESET " Aggiornamento preparato"
           " — riapri il terminale per completare\n");
    free(exenew); free(bat);
#else
    /* POSIX: chmod + rename atomico */
    chmod(tmpbin, 0755);
    if (rename(tmpbin, exe) != 0) {
        fprintf(stderr, C_RED "✗" C_RESET " impossibile sostituire il binario\n"
                "  Prova con sudo o sposta manualmente:\n"
                "  mv %s %s\n", tmpbin, exe);
        free(tmpbin); free(version); free(home);
        return 1;
    }
    printf(C_GREEN "✓" C_RESET " flow aggiornato a v%s\n", version);
#endif

    free(tmpbin); free(version); free(home);
    return 0;
}
