#include "cmd_update.h"
#include "../util.h"
#include "../cli.h"
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

    /* 3. Scarica flow + flowc per questa piattaforma */
    char *bin_dir = str_fmt("%s" SEP "bin", home);
    mkdir_p(bin_dir);

    char exe[4096];
    if (get_exe_path(exe, sizeof(exe)) != 0) {
        fprintf(stderr, C_RED "✗" C_RESET " impossibile trovare il percorso del binario\n");
        free(bin_dir); free(json); free(version); free(home);
        return 1;
    }
    char *exe_dir = path_dirname(exe);

    /* Scarica un asset e installalo */
    int errors = 0;
    const char *tools[] = { "flow", "flowc", NULL };
    const char *base = "https://github.com/" FLOW_REPO "/releases/download";
    for (int i = 0; tools[i]; i++) {
        char *asset = str_fmt("%s-%s-%s%s", tools[i], PLATFORM, ARCH, ASSET_EXT);
        char *url   = find_asset_url(json, asset);
        if (!url) {
            /* Fallback: URL diretto (pattern standard GitHub releases) */
            url = str_fmt("%s/v%s/%s", base, version, asset);
        }
        printf("Download " C_CYAN "%s" C_RESET "...", asset);
        fflush(stdout);

        char *tmp = str_fmt("%s" SEP "%s-new%s", bin_dir, tools[i], ASSET_EXT);
        if (curl_download(url, tmp, "User-Agent: flow-cli") != 0) {
            printf(" " C_RED "✗" C_RESET "\n");
            free(tmp); free(url); free(asset); errors++; continue;
        }

#ifdef _WIN32
        /* flow.exe non può essere sostituito mentre è in esecuzione — usa bat */
        if (i == 0) {
            char *dest    = str_fmt("%s\\%s%s", exe_dir, tools[i], ASSET_EXT);
            char *destnew = str_fmt("%s.new", dest);
            char *bat     = str_fmt("%s-update.bat", dest);
            const char *mv1[] = { "move", "/y", tmp, destnew, NULL };
            run_cmd(mv1);
            char *bat_src = str_fmt(
                "@echo off\r\ntimeout /t 1 /nobreak >nul\r\n"
                "move /y \"%s\" \"%s\"\r\ndel \"%%~f0\"\r\n",
                destnew, dest);
            write_file(bat, bat_src);
            free(bat_src);
            const char *bat_argv[] = { "cmd", "/c", "start", "/min", bat, NULL };
            run_cmd(bat_argv);
            printf(" " C_GREEN "✓" C_RESET " (attiva al prossimo avvio)\n");
            free(dest); free(destnew); free(bat);
        } else {
            char *dest = str_fmt("%s\\%s%s", exe_dir, tools[i], ASSET_EXT);
            const char *mv[] = { "move", "/y", tmp, dest, NULL };
            run_cmd(mv);
            printf(" " C_GREEN "✓" C_RESET "\n");
            free(dest);
        }
#else
        char *dest = str_fmt("%s/%s%s", exe_dir, tools[i], ASSET_EXT);
        chmod(tmp, 0755);
        if (rename(tmp, dest) != 0) {
            fprintf(stderr, " " C_RED "✗" C_RESET " (prova con sudo)\n");
            errors++;
        } else {
            printf(" " C_GREEN "✓" C_RESET "\n");
        }
        free(dest);
#endif
        free(tmp); free(url); free(asset);
    }

    if (errors == 0) {
#ifdef _WIN32
        printf(C_GREEN "✓" C_RESET " Aggiornamento completato — riapri il terminale\n");
#else
        printf(C_GREEN "✓" C_RESET " flow e flowc aggiornati a v%s\n", version);
#endif
    }
    free(bin_dir); free(exe_dir); free(json); free(version); free(home);
    return errors > 0 ? 1 : 0;
}
