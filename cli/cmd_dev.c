#include "cmd_dev.h"
#include "cmd_build.h"
#include "config.h"
#include "util.h"
#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* ── Sockets cross-platform ─────────────────────────────────────────────── */

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET  sock_t;
  #define SOCK_BAD  INVALID_SOCKET
  #define sock_close(s) closesocket(s)
  #define sock_send(s,b,l) send(s, (const char*)(b), (int)(l), 0)
  #define sock_recv(s,b,l) recv(s, (char*)(b), (int)(l), 0)
  #define sleep_ms(ms) Sleep(ms)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <pthread.h>
  typedef int     sock_t;
  #define SOCK_BAD  (-1)
  #define sock_close(s) close(s)
  #define sock_send(s,b,l) send(s, (b), (l), 0)
  #define sock_recv(s,b,l) recv(s, (b), (l), 0)
  #define sleep_ms(ms) usleep((ms)*1000)
#endif

/* ── Stato globale ──────────────────────────────────────────────────────── */

static volatile int  g_running = 1;
static char          g_outdir[4096];

static void handle_sigint(int sig) { (void)sig; g_running = 0; }

/* ── MIME ───────────────────────────────────────────────────────────────── */

static const char *mime_of(const char *ext) {
    if (!ext) return "text/plain";
    if (!strcmp(ext, "html")) return "text/html; charset=utf-8";
    if (!strcmp(ext, "css"))  return "text/css";
    if (!strcmp(ext, "js"))   return "application/javascript";
    if (!strcmp(ext, "wasm")) return "application/wasm";
    if (!strcmp(ext, "png"))  return "image/png";
    if (!strcmp(ext, "jpg") || !strcmp(ext, "jpeg")) return "image/jpeg";
    if (!strcmp(ext, "svg"))  return "image/svg+xml";
    if (!strcmp(ext, "ico"))  return "image/x-icon";
    if (!strcmp(ext, "json")) return "application/json";
    return "text/plain";
}

/* ── HTTP server ────────────────────────────────────────────────────────── */

static void serve_client(sock_t client) {
    char req[4096] = {0};
    int  total = 0, n;

    /* Leggi headers fino a \r\n\r\n */
    while (total < (int)sizeof(req) - 1) {
        n = (int)sock_recv(client, req + total, sizeof(req) - total - 1);
        if (n <= 0) break;
        total += n;
        req[total] = 0;
        if (strstr(req, "\r\n\r\n")) break;
    }

    /* Estrai path dalla prima riga */
    char method[8], urlpath[2048];
    if (sscanf(req, "%7s %2047s", method, urlpath) != 2) {
        sock_close(client); return;
    }

    /* / → /index.html */
    if (strcmp(urlpath, "/") == 0) strcpy(urlpath, "/index.html");

    /* Sicurezza: nessun path traversal */
    if (strstr(urlpath, "..")) {
        const char *r403 = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
        sock_send(client, r403, strlen(r403));
        sock_close(client); return;
    }

    /* Costruisci path sul filesystem */
    char filepath[8192];
    /* normalizza separatori */
    char clean[2048];
    strncpy(clean, urlpath, sizeof(clean) - 1);
#ifdef _WIN32
    for (char *p = clean; *p; p++) if (*p == '/') *p = '\\';
    snprintf(filepath, sizeof(filepath), "%s%s", g_outdir, clean);
#else
    snprintf(filepath, sizeof(filepath), "%s%s", g_outdir, clean);
#endif

    /* Leggi file */
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        const char *r404 =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 9\r\n"
            "Connection: close\r\n\r\n"
            "Not Found";
        sock_send(client, r404, strlen(r404));
        sock_close(client); return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    /* Estrai estensione */
    const char *dot = strrchr(filepath, '.');
    const char *mime = mime_of(dot ? dot + 1 : "");

    /* Invia risposta */
    char hdr[512];
    int  hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n",
        mime, size);
    sock_send(client, hdr, hlen);
    sock_send(client, data, size);
    free(data);
    sock_close(client);
}

/* Thread che gira l'HTTP server */
#ifdef _WIN32
static DWORD WINAPI http_thread(LPVOID arg) {
#else
static void *http_thread(void *arg) {
#endif
    sock_t server = (sock_t)(intptr_t)arg;
    while (g_running) {
        sock_t client = accept(server, NULL, NULL);
        if (client == SOCK_BAD) continue;
        serve_client(client);
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ── File watcher (polling) ─────────────────────────────────────────────── */

typedef struct { char **paths; long long *mtimes; int n; } WatchState;

static WatchState watch_init(const char *srcdir) {
    WatchState ws = {0};
    ws.paths  = glob_flow(srcdir, &ws.n);
    ws.mtimes = malloc(ws.n * sizeof(long long));
    for (int i = 0; i < ws.n; i++)
        ws.mtimes[i] = file_mtime(ws.paths[i]);
    return ws;
}

static int watch_changed(WatchState *ws, const char *srcdir) {
    int changed = 0;
    int  n2;
    char **paths2 = glob_flow(srcdir, &n2);

    /* Controlla file esistenti */
    for (int i = 0; i < ws->n; i++) {
        long long m = file_mtime(ws->paths[i]);
        if (m != ws->mtimes[i]) { changed = 1; break; }
    }
    /* File aggiunti */
    if (!changed && n2 != ws->n) changed = 1;

    /* Aggiorna stato */
    if (changed) {
        for (int i = 0; i < ws->n; i++) free(ws->paths[i]);
        free(ws->paths); free(ws->mtimes);
        ws->paths  = paths2;
        ws->n      = n2;
        ws->mtimes = malloc(n2 * sizeof(long long));
        for (int i = 0; i < n2; i++)
            ws->mtimes[i] = file_mtime(paths2[i]);
    } else {
        for (int i = 0; i < n2; i++) free(paths2[i]);
        free(paths2);
    }
    return changed;
}

/* ── Comando principale ─────────────────────────────────────────────────── */

int cmd_dev(void) {
    FlowConfig cfg = {0};
    if (!config_load(&cfg)) return 1;

    char *cwd    = get_cwd();
    char *srcdir = path_join(cwd, "src");
    char *outdir = path_join(cwd, cfg.out);
    strncpy(g_outdir, outdir, sizeof(g_outdir) - 1);
    mkdir_p(outdir);

    signal(SIGINT, handle_sigint);

    /* Banner */
    printf("┌─────────────────────────────────┐\n");
    printf("│  Flow dev server                │\n");
    printf("│  http://localhost:3000          │\n");
    printf("└─────────────────────────────────┘\n\n");

    /* Prima build */
    int code = cmd_build(0, 0);
    if (code != 0) { free(srcdir); free(outdir); free(cwd); config_free(&cfg); return code; }

    /* Avvia HTTP server */
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock_t server = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(3000);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server, (struct sockaddr*)&addr, sizeof(addr));
    listen(server, 10);

#ifdef _WIN32
    CreateThread(NULL, 0, http_thread, (LPVOID)(intptr_t)server, 0, NULL);
#else
    pthread_t tid;
    pthread_create(&tid, NULL, http_thread, (void*)(intptr_t)server);
    pthread_detach(tid);
#endif

    printf(C_GREEN "✓" C_RESET " Server su http://localhost:3000\n");
    printf("  watching %s\n\n", srcdir);

    /* Watcher loop */
    WatchState ws = watch_init(srcdir);
    while (g_running) {
        sleep_ms(400);
        if (!g_running) break;
        if (watch_changed(&ws, srcdir)) {
            printf("  rebuild... ");
            fflush(stdout);
            long long t0;
#ifdef _WIN32
            LARGE_INTEGER freq, t1, t2;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&t1);
#else
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            t0 = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
#endif
            cmd_build(0, 0);
#ifdef _WIN32
            QueryPerformanceCounter(&t2);
            long long ms = (t2.QuadPart - t1.QuadPart) * 1000 / freq.QuadPart;
#else
            clock_gettime(CLOCK_MONOTONIC, &ts);
            long long ms = ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL - t0;
#endif
            printf(C_GREEN "✓" C_RESET " rebuild in %llums\n", ms);
        }
    }

    printf("\n" C_YELLOW "→" C_RESET " Dev server fermato\n");
    sock_close(server);
#ifdef _WIN32
    WSACleanup();
#endif
    for (int i = 0; i < ws.n; i++) free(ws.paths[i]);
    free(ws.paths); free(ws.mtimes);
    free(srcdir); free(outdir); free(cwd);
    config_free(&cfg);
    return 0;
}
