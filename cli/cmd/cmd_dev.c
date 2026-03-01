#include "cmd_dev.h"
#include "cmd_build.h"
#include "../config.h"
#include "../util.h"
#include "../cli.h"
#include "../webview/wv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET  sock_t;
  #define SOCK_BAD  INVALID_SOCKET
  #define sock_close(s)   closesocket(s)
  #define sock_send(s,b,l) send(s, (const char*)(b), (int)(l), 0)
  #define sock_recv(s,b,l) recv(s, (char*)(b), (int)(l), 0)
  #define sleep_ms(ms)    Sleep(ms)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <pthread.h>
  typedef int sock_t;
  #define SOCK_BAD   (-1)
  #define sock_close(s)    close(s)
  #define sock_send(s,b,l) send(s, (b), (l), 0)
  #define sock_recv(s,b,l) recv(s, (b), (l), 0)
  #define sleep_ms(ms)     usleep((ms)*1000)
#endif

static volatile int g_running    = 1;
static char         g_outdir[4096];
static wv_handle_t  g_wv         = NULL;
static int          g_srv_port   = 3001;  // porta server process

static void handle_sigint(int sig) { (void)sig; g_running = 0; }

/* Ottieni IP locale (per network URL) */
static void get_local_ip(char *buf, size_t len) {
    buf[0] = '\0';
#ifdef _WIN32
    sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == SOCK_BAD) return;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3000);
    addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        struct sockaddr_in local = {0};
        int loclen = sizeof(local);
        if (getsockname(s, (struct sockaddr*)&local, &loclen) == 0) {
            char *ip = inet_ntoa(local.sin_addr);
            if (ip) snprintf(buf, len, "http://%s:3000", ip);
        }
    }
    sock_close(s);
#else
    sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == SOCK_BAD) return;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3000);
    addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        struct sockaddr_in local = {0};
        socklen_t loclen = sizeof(local);
        if (getsockname(s, (struct sockaddr*)&local, &loclen) == 0) {
            char ip[32];
            inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
            snprintf(buf, len, "http://%s:3000", ip);
        }
    }
    sock_close(s);
#endif
}

/* Escape stringa per uso in JS (tra virgolette doppie) */
static char *js_escape(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n * 6 + 1);
    if (!out) return NULL;
    char *p = out;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if      (c == '"')  { *p++ = '\\'; *p++ = '"';  }
        else if (c == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else if (c == '\n') { *p++ = '\\'; *p++ = 'n';  }
        else if (c == '\r') { *p++ = '\\'; *p++ = 'r';  }
        else if (c == '\t') { *p++ = '\\'; *p++ = 't';  }
        else                { *p++ = (char)c; }
    }
    *p = '\0';
    return out;
}

/* HMR: aggiorna solo #root e <style>, swap WASM — no reload */
static void hmr_update(void) {
    char *html_path = str_fmt("%s" SEP "index.html", g_outdir);
    if (!html_path) return;
    char *html = read_file(html_path);
    free(html_path);
    if (!html) return;

    char *esc = js_escape(html);
    free(html);
    if (!esc) return;

    char *js = str_fmt(
        "(function(){"
        "var d=new DOMParser().parseFromString(\"%s\",'text/html');"
        "var r=document.getElementById('root'),nr=d.getElementById('root');"
        "if(r&&nr)r.innerHTML=nr.innerHTML;"
        "var s=document.querySelector('style'),ns=d.querySelector('style');"
        "if(s&&ns)s.textContent=ns.textContent;"
        "var m;var imp={env:{flow_print_str:function(p){var v=new Uint8Array(m.buffer),e=p;while(v[e])e++;console.log(new TextDecoder().decode(v.subarray(p,e)));},flow_eprint_str:function(p){var v=new Uint8Array(m.buffer),e=p;while(v[e])e++;console.error(new TextDecoder().decode(v.subarray(p,e)));}}};"
        "fetch('app.wasm?t='+Date.now()).then(function(r){return r.arrayBuffer();}).then(function(b){return WebAssembly.compile(b);}).then(function(mod){return WebAssembly.instantiate(mod,imp);}).then(function(r){m=r.instance.exports.memory;window._flow=r.instance.exports;if(r.instance.exports.main)r.instance.exports.main();}).catch(function(e){console.error('HMR:',e);});"
        "})();", esc);
    free(esc);
    if (!js) return;
    wv_eval(g_wv, js);
    free(js);
}

/* ── Proxy: inoltra la richiesta a localhost:g_srv_port ────────────────── */

static void proxy_to_server(sock_t client, const char *raw_req, int raw_len) {
    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == SOCK_BAD) {
        const char *err = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 11\r\nConnection: close\r\n\r\nBad Gateway";
        sock_send(client, err, (int)strlen(err));
        return;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)g_srv_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        sock_close(srv);
        const char *err = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 19\r\nConnection: close\r\n\r\nServer not running";
        sock_send(client, err, (int)strlen(err));
        return;
    }

    sock_send(srv, raw_req, raw_len);

    char buf[65536];
    int n;
    while ((n = (int)sock_recv(srv, buf, sizeof(buf))) > 0)
        sock_send(client, buf, n);

    sock_close(srv);
}

/* ── Avvia server.exe in background ────────────────────────────────────── */

static void spawn_server(const char *outdir) {
    char srv_exe[4096];
    snprintf(srv_exe, sizeof(srv_exe), "%s" SEP "server" EXE_EXT, outdir);
    if (!path_exists(srv_exe)) return;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", g_srv_port);

#ifdef _WIN32
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "\"%s\" %s \"%s\"", srv_exe, port_str, outdir);
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                   CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    if (fork() == 0) {
        execl(srv_exe, srv_exe, port_str, outdir, NULL);
        _exit(1);
    }
#endif
    sleep_ms(300);  // lascia il tempo al server di avviarsi
}

/* ── HTTP server (serve out/) ──────────────────────────────────────────── */

static const char *mime_of(const char *ext) {
    if (!ext) return "text/plain";
    if (!strcmp(ext, "html")) return "text/html; charset=utf-8";
    if (!strcmp(ext, "css"))  return "text/css";
    if (!strcmp(ext, "js"))   return "application/javascript";
    if (!strcmp(ext, "wasm")) return "application/wasm";
    return "text/plain";
}

static void serve_client(sock_t client) {
    char req[4096] = {0};
    int total = 0, n;
    while (total < (int)sizeof(req) - 1) {
        n = (int)sock_recv(client, req + total, sizeof(req) - total - 1);
        if (n <= 0) break;
        total += n;
        req[total] = '\0';
        if (strstr(req, "\r\n\r\n")) break;
    }

    char method[8], urlpath[2048];
    if (sscanf(req, "%7s %2047s", method, urlpath) != 2) {
        sock_close(client); return;
    }

    // Proxy: /_flow/fn/* e /api/* → server process
    if (strncmp(urlpath, "/_flow/fn/", 10) == 0 ||
        strncmp(urlpath, "/api/", 5) == 0) {
        proxy_to_server(client, req, total);
        sock_close(client);
        return;
    }

    char *q = strchr(urlpath, '?');
    if (q) *q = '\0';
    if (strcmp(urlpath, "/") == 0) strcpy(urlpath, "/index.html");
    if (strstr(urlpath, "..")) {
        sock_send(client, "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n", 45);
        sock_close(client); return;
    }

    char filepath[8192], clean[2048];
    strncpy(clean, urlpath, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
#ifdef _WIN32
    for (char *p = clean; *p; p++) if (*p == '/') *p = '\\';
#endif
    snprintf(filepath, sizeof(filepath), "%s%s", g_outdir, clean);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        sock_send(client,
            "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\nConnection: close\r\n\r\nNot Found",
            72);
        sock_close(client); return;
    }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char *data = malloc(size + 1);
    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);

    const char *dot  = strrchr(filepath, '.');
    const char *mime = mime_of(dot ? dot + 1 : "");
    char hdr[256];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n"
        "Cache-Control: no-cache\r\nConnection: close\r\n\r\n",
        mime, (long)size);
    sock_send(client, hdr, hlen);
    sock_send(client, data, size);
    free(data);
    sock_close(client);
}

#ifdef _WIN32
static DWORD WINAPI http_thread(LPVOID arg) {
#else
static void *http_thread(void *arg) {
#endif
    sock_t srv = (sock_t)(intptr_t)arg;
    while (g_running) {
        sock_t c = accept(srv, NULL, NULL);
        if (c != SOCK_BAD) {
            int nd = 1;
            setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (char*)&nd, sizeof(nd));
            serve_client(c);
        }
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ── File watcher ──────────────────────────────────────────────────────── */

typedef struct { char **paths; long long *mtimes; int n; } WatchState;

static WatchState watch_init(const char *srcdir) {
    WatchState ws = {0};
    ws.paths  = glob_flow(srcdir, &ws.n);
    ws.mtimes = malloc(ws.n * sizeof(long long));
    for (int i = 0; i < ws.n; i++) ws.mtimes[i] = file_mtime(ws.paths[i]);
    return ws;
}

static int watch_changed(WatchState *ws, const char *srcdir) {
    int n2; char **p2 = glob_flow(srcdir, &n2);
    int changed = (n2 != ws->n);
    for (int i = 0; !changed && i < ws->n; i++)
        if (file_mtime(ws->paths[i]) != ws->mtimes[i]) changed = 1;
    if (changed) {
        for (int i = 0; i < ws->n; i++) free(ws->paths[i]);
        free(ws->paths); free(ws->mtimes);
        ws->paths = p2; ws->n = n2;
        ws->mtimes = malloc(n2 * sizeof(long long));
        for (int i = 0; i < n2; i++) ws->mtimes[i] = file_mtime(p2[i]);
    } else {
        for (int i = 0; i < n2; i++) free(p2[i]);
        free(p2);
    }
    return changed;
}

#ifdef _WIN32
static DWORD WINAPI watcher_thread(LPVOID arg) {
#else
static void *watcher_thread(void *arg) {
#endif
    char *srcdir = (char*)arg;
    WatchState ws = watch_init(srcdir);
    int pending = 0, last = 0, tick = 0;

    while (g_running) {
        sleep_ms(5);
        if (!g_running) break;
        tick++;
        if (watch_changed(&ws, srcdir)) { pending = 1; last = tick; }
        if (!pending || tick - last < 1) continue;
        pending = 0;

        cmd_build(0, 0, 1, 0);
        hmr_update();
    }
    for (int i = 0; i < ws.n; i++) free(ws.paths[i]);
    free(ws.paths); free(ws.mtimes);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ── Comando principale ────────────────────────────────────────────────── */

int cmd_dev(void) {
    FlowConfig cfg = {0};
    if (!config_load(&cfg)) return 1;

    char *cwd    = get_cwd();
    char *srcdir = path_join(cwd, "client");
    char *outdir = path_join(cwd, cfg.out);
    strncpy(g_outdir, outdir, sizeof(g_outdir) - 1);
    g_outdir[sizeof(g_outdir) - 1] = '\0';
    mkdir_p(outdir);
    signal(SIGINT, handle_sigint);

    if (cmd_build(0, 0, 1, 0) != 0) {
        free(srcdir); free(outdir); free(cwd); config_free(&cfg);
        return 1;
    }

    // Avvia server.exe se esiste
    spawn_server(outdir);
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    char neturl[64] = "";
    get_local_ip(neturl, sizeof(neturl));
    if (neturl[0] == '\0') snprintf(neturl, sizeof(neturl), "http://localhost:3000");

    {
        /* Banner stile Flow OS: angoli stondati, rombi ◆, colore #ff0087 */
        /* W = larghezza interna (tra │ e │)                               */
        #define W 44
        #define TL "\xE2\x95\xAD"  /* ╭ */
        #define TR "\xE2\x95\xAE"  /* ╮ */
        #define BL "\xE2\x95\xB0"  /* ╰ */
        #define BR "\xE2\x95\xAF"  /* ╯ */
        #define HZ "\xE2\x94\x80"  /* ─ */
        #define VT "\xE2\x94\x82"  /* │ */
        #define DM "\xE2\x97\x86"  /* ◆ */
        /* URL_W: spazio per l'URL = W - "  ◆ Xxxxxxx: "(13) - "  "(2) */
        #define URL_W (W - 15)
        int half = (W - 10) / 2, rest = W - half - 10;

        /* bordo superiore */
        printf("\n  " C_FLOW TL C_RESET);
        for (int i = 0; i < half; i++) printf(C_FLOW HZ C_RESET);
        printf(C_FLOW " Flow dev " C_RESET);
        for (int i = 0; i < rest; i++) printf(C_FLOW HZ C_RESET);
        printf(C_FLOW TR "\n" C_RESET);

        /* riga vuota in cima */
        printf("  " C_FLOW VT C_RESET "%*s" C_FLOW VT C_RESET "\n", W, "");

        /* local */
        printf("  " C_FLOW VT C_RESET "  " C_FLOW DM C_RESET " Local:   "
               C_FLOW "%-*s" C_RESET "  " C_FLOW VT C_RESET "\n",
               URL_W, "http://localhost:3000");

        /* network */
        printf("  " C_FLOW VT C_RESET "  " C_FLOW DM C_RESET " Network: "
               C_FLOW "%-*s" C_RESET "  " C_FLOW VT C_RESET "\n",
               URL_W, neturl);

        /* bordo inferiore */
        printf("  " C_FLOW BL C_RESET);
        for (int i = 0; i < W; i++) printf(C_FLOW HZ C_RESET);
        printf(C_FLOW BR "\n\n" C_RESET);

        #undef W
        #undef URL_W
        #undef TL
        #undef TR
        #undef BL
        #undef BR
        #undef HZ
        #undef VT
        #undef DM
    }

    sock_t server = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(3000);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server, (struct sockaddr*)&addr, sizeof(addr));
    listen(server, 10);

#ifdef _WIN32
    CreateThread(NULL, 0, http_thread, (LPVOID)(intptr_t)server, 0, NULL);
#else
    {
        pthread_t tid;
        pthread_create(&tid, NULL, http_thread, (void*)(intptr_t)server);
        pthread_detach(tid);
    }
#endif

    g_wv = wv_create(1024, 768, "Flow dev");
    if (g_wv) {
        wv_navigate(g_wv, "http://localhost:3000");
#ifdef _WIN32
        CreateThread(NULL, 0, watcher_thread, srcdir, 0, NULL);
#else
        {
            pthread_t wtid;
            pthread_create(&wtid, NULL, watcher_thread, srcdir);
            pthread_detach(wtid);
        }
#endif
        wv_run(g_wv);
        g_running = 0;
        wv_destroy(g_wv);
        g_wv = NULL;
    } else {
        WatchState ws = watch_init(srcdir);
        int pending = 0, last = 0, tick = 0;
        while (g_running) {
            sleep_ms(50); tick++;
            if (watch_changed(&ws, srcdir)) { pending = 1; last = tick; }
            if (!pending || tick - last < 2) continue;
            pending = 0;
            cmd_build(0, 0, 1, 0);
        }
        for (int i = 0; i < ws.n; i++) free(ws.paths[i]);
        free(ws.paths); free(ws.mtimes);
    }

    printf(C_FLOW "\n  \u2190" C_RESET " stop\n");
    sock_close(server);
#ifdef _WIN32
    WSACleanup();
#endif
    free(srcdir); free(outdir); free(cwd);
    config_free(&cfg);
    return 0;
}
