#pragma once

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

/* API C per finestra WebView2 nativa (solo Windows).
   wv_create: crea finestra, ritorna handle (NULL = errore)
   wv_navigate: carica URL (chiamare dopo wv_create)
   wv_eval: esegue JS (es. "location.reload()")
   wv_run: messaggio loop — blocca fino a chiusura
   wv_close: chiude la finestra (chiamare da altro thread)
   wv_destroy: rilascia risorse */

typedef void* wv_handle_t;

wv_handle_t wv_create(int width, int height, const char *title);
void        wv_navigate(wv_handle_t h, const char *url);
void        wv_eval(wv_handle_t h, const char *js);
void        wv_run(wv_handle_t h);   /* blocca */
void        wv_close(wv_handle_t h); /* thread-safe */
void        wv_destroy(wv_handle_t h);

#ifdef __cplusplus
}
#endif

#else

/* Su non-Windows: stub vuoti (flow dev usa browser) */
typedef void* wv_handle_t;
#define wv_create(w,h,t)   ((wv_handle_t)0)
#define wv_navigate(h,u)  ((void)0)
#define wv_eval(h,j)      ((void)0)
#define wv_run(h)         ((void)0)
#define wv_close(h)       ((void)0)
#define wv_destroy(h)     ((void)0)

#endif
