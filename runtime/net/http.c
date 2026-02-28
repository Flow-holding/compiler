#pragma once
#include "../types/strings.c"

// ─────────────────────────────────────────
// HTTP — server e client minimali
// Usa le socket standard POSIX / Winsock
// ─────────────────────────────────────────

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET flow_socket_t;
    #define FLOW_INVALID_SOCKET INVALID_SOCKET
    #define flow_close_socket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int flow_socket_t;
    #define FLOW_INVALID_SOCKET -1
    #define flow_close_socket close
#endif

// Struttura per una richiesta HTTP
typedef struct {
    FlowObject base;
    FlowStr*   metodo;    // GET, POST, ecc.
    FlowStr*   percorso;  // /api/utenti
    FlowStr*   body;      // corpo della richiesta
} FlowRequest;

// Struttura per una risposta HTTP
typedef struct {
    FlowObject base;
    int32_t    status;    // 200, 404, ecc.
    FlowStr*   body;      // corpo della risposta
    FlowStr*   tipo;      // Content-Type
} FlowResponse;

// Crea una risposta
FlowResponse* flow_http_response(int32_t status,
                                  const char* body,
                                  const char* tipo) {
    FlowResponse* r = (FlowResponse*)flow_alloc(sizeof(FlowResponse));
    r->status = status;
    r->body   = flow_str_new(body);
    r->tipo   = flow_str_new(tipo);
    return r;
}

// Inizializza le socket (necessario su Windows)
static void flow_net_init() {
    #ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    #endif
}

// Avvia un server HTTP sulla porta specificata
// handler: funzione chiamata per ogni richiesta
void flow_http_serve(int32_t porta,
                     FlowResponse* (*handler)(FlowRequest*)) {
    flow_net_init();

    flow_socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == FLOW_INVALID_SOCKET) {
        fprintf(stderr, "flow: impossibile creare socket\n");
        return;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)porta);

    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "flow: bind fallito sulla porta %d\n", porta);
        flow_close_socket(server);
        return;
    }

    listen(server, 10);
    printf("flow: server in ascolto su http://localhost:%d\n", porta);

    while (1) {
        flow_socket_t client = accept(server, NULL, NULL);
        if (client == FLOW_INVALID_SOCKET) continue;

        // Leggi la richiesta
        char buf[4096] = {0};
        recv(client, buf, sizeof(buf) - 1, 0);

        // Parsing minimo — estrai metodo e percorso
        FlowRequest* req = (FlowRequest*)flow_alloc(sizeof(FlowRequest));
        char metodo[16] = {0}, percorso[256] = {0};
        sscanf(buf, "%15s %255s", metodo, percorso);
        req->metodo   = flow_str_new(metodo);
        req->percorso = flow_str_new(percorso);
        req->body     = flow_str_new("");

        // Chiama l'handler
        FlowResponse* res = handler(req);

        // Invia la risposta
        char header[512];
        snprintf(header, sizeof(header),
            "HTTP/1.1 %d OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n\r\n",
            res->status, res->tipo->data, res->body->len);

        send(client, header, (int)strlen(header), 0);
        send(client, res->body->data, res->body->len, 0);

        flow_close_socket(client);
        flow_release((FlowObject*)req);
        flow_release((FlowObject*)res);
    }

    flow_close_socket(server);
}
