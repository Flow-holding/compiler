#pragma once
#include "../types/strings.c"

// ─────────────────────────────────────────
// WebSocket — connessioni real-time
// Implementazione base del protocollo WS
// ─────────────────────────────────────────

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET flow_socket_t;
    #define flow_close_socket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    typedef int flow_socket_t;
    #define flow_close_socket close
#endif

#include <stdint.h>

// Struttura connessione WebSocket
typedef struct {
    FlowObject    base;
    flow_socket_t socket;
    int32_t       connesso;
} FlowWebSocket;

// Invia un messaggio di testo via WebSocket
// Frame WS: opcode 0x81 (text), payload length, data
int32_t flow_ws_send(FlowWebSocket* ws, FlowStr* messaggio) {
    if (!ws->connesso) return 0;

    int32_t len = messaggio->len;
    uint8_t header[10];
    int32_t header_len;

    header[0] = 0x81; // FIN + opcode text

    if (len <= 125) {
        header[1]  = (uint8_t)len;
        header_len = 2;
    } else if (len <= 65535) {
        header[1]  = 126;
        header[2]  = (len >> 8) & 0xFF;
        header[3]  = len & 0xFF;
        header_len = 4;
    } else {
        return 0; // messaggi > 64KB non supportati ora
    }

    send(ws->socket, (const char*)header, header_len, 0);
    send(ws->socket, messaggio->data, len, 0);
    return 1;
}

// Chiude la connessione
void flow_ws_close(FlowWebSocket* ws) {
    if (ws->connesso) {
        // Frame di chiusura WS
        uint8_t close_frame[] = {0x88, 0x00};
        send(ws->socket, (const char*)close_frame, 2, 0);
        flow_close_socket(ws->socket);
        ws->connesso = 0;
    }
}
