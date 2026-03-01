/* wv_stub.c — stub per build release senza webview (CI/GitHub Actions)
   Fornisce implementazioni vuote di wv_*; flow dev funziona senza finestra
   (solo server HTTP + watcher, apri http://localhost:3000 nel browser) */

#include "wv.h"
#include <stddef.h>

wv_handle_t wv_create(int width, int height, const char *title) {
    (void)width;
    (void)height;
    (void)title;
    return NULL;
}

void wv_navigate(wv_handle_t h, const char *url) {
    (void)h;
    (void)url;
}

void wv_eval(wv_handle_t h, const char *js) {
    (void)h;
    (void)js;
}

void wv_run(wv_handle_t h) {
    (void)h;
}

void wv_close(wv_handle_t h) {
    (void)h;
}

void wv_destroy(wv_handle_t h) {
    (void)h;
}
