#pragma once
#include "../types/strings.c"
#include "router.c"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ─────────────────────────────────────────────────────────────────
// HTTP — server sulla stessa porta per client statico + server API
//
// Routing:
//   POST /_flow/fn/<nome>  →  server function (auto-registrate)
//   *    /api/*            →  REST routes (da server/routes/)
//   GET  /*                →  file statici da out/
// ─────────────────────────────────────────────────────────────────

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET flow_socket_t;
    #define FLOW_INVALID_SOCKET INVALID_SOCKET
    #define flow_close_socket   closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int flow_socket_t;
    #define FLOW_INVALID_SOCKET -1
    #define flow_close_socket   close
#endif

// ── Strutture request / response ─────────────────────────────────

typedef struct {
    char method[16];
    char path[512];
    char query[512];    // ?foo=bar&baz=1
    char body[65536];
    int  body_len;
    // headers grezzi (parse minimale)
    char headers_raw[4096];
    // path params estratti dal router
    FlowPathParam params[FLOW_PATH_PARAM_MAX];
    int           n_params;
} FlowReq;

typedef struct {
    int  status;
    char content_type[64];
    char body[65536];
    int  body_len;
} FlowRes;

typedef FlowRes* (*FlowHandler)(FlowReq*);

// ── Helpers ───────────────────────────────────────────────────────

static void res_json(FlowRes *r, int status, const char *json) {
    r->status    = status;
    r->body_len  = snprintf(r->body, sizeof(r->body), "%s", json);
    snprintf(r->content_type, sizeof(r->content_type), "application/json");
}

static void res_html(FlowRes *r, int status, const char *html) {
    r->status    = status;
    r->body_len  = snprintf(r->body, sizeof(r->body), "%s", html);
    snprintf(r->content_type, sizeof(r->content_type), "text/html; charset=utf-8");
}

static void res_text(FlowRes *r, int status, const char *text) {
    r->status    = status;
    r->body_len  = snprintf(r->body, sizeof(r->body), "%s", text);
    snprintf(r->content_type, sizeof(r->content_type), "text/plain; charset=utf-8");
}

// ── Logger automatico ─────────────────────────────────────────────

static void flow_log_request(const char *method, const char *path,
                               int status, long ms) {
    const char *color = status < 300 ? "\033[32m" :
                        status < 400 ? "\033[33m" : "\033[31m";
    fprintf(stdout,
        "  %s%s\033[0m  \033[2m%s\033[0m  %dms\n",
        color, method, path, (int)ms);
    fflush(stdout);
}

// ── File statico ──────────────────────────────────────────────────

static FlowRes *serve_static(const char *out_dir, const char *path) {
    FlowRes *res = (FlowRes*)calloc(1, sizeof(FlowRes));

    // path "/" → index.html
    char filepath[1024];
    if (strcmp(path, "/") == 0)
        snprintf(filepath, sizeof(filepath), "%s/index.html", out_dir);
    else
        snprintf(filepath, sizeof(filepath), "%s%s", out_dir, path);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        res_html(res, 404, "<h1>404 Not Found</h1>");
        return res;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size >= (long)sizeof(res->body)) size = sizeof(res->body) - 1;
    fread(res->body, 1, size, f);
    fclose(f);

    res->body_len = (int)size;
    res->status   = 200;

    // Content-Type per estensione
    if (strstr(filepath, ".html"))     snprintf(res->content_type, 64, "text/html; charset=utf-8");
    else if (strstr(filepath, ".css")) snprintf(res->content_type, 64, "text/css");
    else if (strstr(filepath, ".js"))  snprintf(res->content_type, 64, "application/javascript");
    else if (strstr(filepath, ".svg")) snprintf(res->content_type, 64, "image/svg+xml");
    else if (strstr(filepath, ".png")) snprintf(res->content_type, 64, "image/png");
    else if (strstr(filepath, ".ico")) snprintf(res->content_type, 64, "image/x-icon");
    else                               snprintf(res->content_type, 64, "application/octet-stream");

    return res;
}

// ── Parse HTTP request ────────────────────────────────────────────

static void parse_request(const char *raw, FlowReq *req) {
    // Prima riga: METHOD /path?query HTTP/1.1
    sscanf(raw, "%15s %511s", req->method, req->path);

    // Separa path da query string
    char *q = strchr(req->path, '?');
    if (q) {
        strncpy(req->query, q + 1, sizeof(req->query) - 1);
        *q = '\0';
    }

    // Body: dopo \r\n\r\n
    const char *body_start = strstr(raw, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        req->body_len = (int)strlen(body_start);
        if (req->body_len >= (int)sizeof(req->body))
            req->body_len = sizeof(req->body) - 1;
        memcpy(req->body, body_start, req->body_len);
    }

    // Headers grezzi
    const char *hend = strstr(raw, "\r\n\r\n");
    if (hend) {
        int hlen = (int)(hend - raw);
        if (hlen >= (int)sizeof(req->headers_raw))
            hlen = sizeof(req->headers_raw) - 1;
        memcpy(req->headers_raw, raw, hlen);
    }
}

// ── Invia risposta HTTP ───────────────────────────────────────────

static void send_response(flow_socket_t client, FlowRes *res) {
    char header[512];
    int  hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n",
        res->status, res->content_type, res->body_len);

    send(client, header, hlen, 0);
    if (res->body_len > 0)
        send(client, res->body, res->body_len, 0);
}

// ── Server principale ─────────────────────────────────────────────

static void flow_net_init() {
    #ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    #endif
}

// out_dir: cartella degli asset statici compilati (es. "out/")
void flow_http_serve(int port, const char *out_dir) {
    flow_net_init();

    flow_socket_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == FLOW_INVALID_SOCKET) {
        fprintf(stderr, "flow: impossibile creare socket\n");
        return;
    }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "flow: porta %d non disponibile\n", port);
        flow_close_socket(srv);
        return;
    }

    listen(srv, 64);
    fprintf(stdout, "\n  \033[32m▲ Flow\033[0m  \033[2mhttp://localhost:%d\033[0m\n\n", port);
    fflush(stdout);

    char raw[131072];

    while (1) {
        flow_socket_t client = accept(srv, NULL, NULL);
        if (client == FLOW_INVALID_SOCKET) continue;

        memset(raw, 0, sizeof(raw));
        recv(client, raw, sizeof(raw) - 1, 0);

        FlowReq *req = (FlowReq*)calloc(1, sizeof(FlowReq));
        parse_request(raw, req);

        clock_t t0 = clock();
        FlowRes *res = NULL;

        // ── Dispatch ──────────────────────────────────────────────
        FlowHandler handler = (FlowHandler)flow_router_dispatch(
            req->method, req->path, req->params, &req->n_params);

        if (handler) {
            res = handler(req);
        } else {
            // Nessuna route registrata → file statico
            res = serve_static(out_dir, req->path);
        }

        long ms = (long)((double)(clock() - t0) / CLOCKS_PER_SEC * 1000);
        flow_log_request(req->method, req->path, res->status, ms);

        send_response(client, res);
        flow_close_socket(client);
        free(req);
        free(res);
    }

    flow_close_socket(srv);
}
