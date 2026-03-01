#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* API C per webview cross-platform (webview library)
   Windows: WebView2 | macOS: WebKit | Linux: WebKitGTK
   wv_create: crea finestra, ritorna handle (NULL = errore)
   wv_navigate: carica URL
   wv_eval: esegue JS (thread-safe via webview_dispatch)
   wv_run: blocca fino a chiusura
   wv_close: chiude (thread-safe)
   wv_destroy: rilascia risorse */

typedef void* wv_handle_t;

wv_handle_t wv_create(int width, int height, const char *title);
void        wv_navigate(wv_handle_t h, const char *url);
void        wv_eval(wv_handle_t h, const char *js);
void        wv_run(wv_handle_t h);
void        wv_close(wv_handle_t h);
void        wv_destroy(wv_handle_t h);

#ifdef __cplusplus
}
#endif
