/* wv_webview.c — wrapper Flow su libreria webview (cross-platform)
   Usa webview_create, webview_navigate, webview_eval, webview_run, webview_destroy
   Su Windows: WebView2 | macOS: WebKit | Linux: WebKitGTK */

#include "wv.h"
#include "webview/webview.h"
#include <stdlib.h>
#include <string.h>

/* Per wv_eval da thread diverso: webview_dispatch esegue sul main thread */
struct eval_arg {
    char *js;
    webview_t w;
};

static void eval_on_main(webview_t w, void *arg) {
    struct eval_arg *ea = (struct eval_arg *)arg;
    if (ea && ea->js) {
        webview_eval(ea->w, ea->js);
        free(ea->js);
    }
    free(ea);
}

wv_handle_t wv_create(int width, int height, const char *title) {
    webview_t w = webview_create(0, NULL);
    if (!w) return NULL;
    if (title) webview_set_title(w, title);
    webview_set_size(w, width, height, WEBVIEW_HINT_NONE);
    return (wv_handle_t)w;
}

void wv_navigate(wv_handle_t h, const char *url) {
    if (!h || !url) return;
    webview_navigate((webview_t)h, url);
}

void wv_eval(wv_handle_t h, const char *js) {
    if (!h || !js) return;
    /* wv_eval può essere chiamato da watcher_thread; webview_dispatch è thread-safe */
    struct eval_arg *ea = malloc(sizeof(*ea));
    if (!ea) return;
    ea->js = (char*)malloc(strlen(js) + 1);
    if (ea->js) strcpy(ea->js, js);
    ea->w = (webview_t)h;
    if (ea->js)
        webview_dispatch((webview_t)h, eval_on_main, ea);
    else
        free(ea);
}

void wv_run(wv_handle_t h) {
    if (h) webview_run((webview_t)h);
}

void wv_close(wv_handle_t h) {
    if (h) webview_terminate((webview_t)h);
}

void wv_destroy(wv_handle_t h) {
    if (h) webview_destroy((webview_t)h);
}
