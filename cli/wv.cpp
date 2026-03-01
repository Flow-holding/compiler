#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include "wv.h"
#include <windows.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <string>
#include <atomic>
#include <cstring>

#include "WebView2.h"

using Microsoft::WRL::Callback;

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

using Microsoft::WRL::ComPtr;

#define WM_WV_EVAL    (WM_USER + 1)
#define WM_WV_NAV     (WM_USER + 2)

/* Per wv_eval da altro thread: allociamo la stringa, PostMessage, handler libera */
struct WvState {
    HWND hwnd;
    ComPtr<ICoreWebView2> webview;
    ComPtr<ICoreWebView2Controller> controller;
    std::atomic<bool> ready{false};
    std::atomic<bool> closed{false};
    std::wstring pendingUrl;
};

static WvState* g_wv = nullptr;

static void utf8_to_wide(const char* utf8, std::wstring& out) {
    out.clear();
    if (!utf8 || !*utf8) return;
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (n <= 0) return;
    out.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], n);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WvState* s = (WvState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!s && msg != WM_CREATE) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE: {
        s = (WvState*)((CREATESTRUCT*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
        s->hwnd = hwnd;
        return 0;
    }
    case WM_WV_EVAL: {
        char* js = (char*)(void*)lp;
        if (s && s->webview && js) {
            std::wstring wjs;
            utf8_to_wide(js, wjs);
            s->webview->ExecuteScript(wjs.c_str(), nullptr);
            free(js);
        }
        return 0;
    }
    case WM_WV_NAV: {
        if (s && s->webview && !s->pendingUrl.empty()) {
            s->webview->Navigate(s->pendingUrl.c_str());
            s->pendingUrl.clear();
        }
        return 0;
    }
    case WM_DESTROY:
        if (s) s->closed = true;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (s && s->controller) {
            RECT r;
            GetClientRect(hwnd, &r);
            s->controller->put_Bounds(r);
        }
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

static void OnControllerCreated(HRESULT err, ICoreWebView2Controller* ctrl, WvState* s) {
    if (FAILED(err) || !ctrl) return;
    s->controller = ctrl;
    ComPtr<ICoreWebView2> wv;
    ctrl->get_CoreWebView2(&wv);
    if (!wv) return;
    s->webview = wv;
    RECT r;
    GetClientRect(s->hwnd, &r);
    s->controller->put_Bounds(r);
    s->controller->put_IsVisible(TRUE);
    s->ready = true;
    if (!s->pendingUrl.empty()) {
        s->webview->Navigate(s->pendingUrl.c_str());
        s->pendingUrl.clear();
    }
}

static void OnEnvCreated(HRESULT err, ICoreWebView2Environment* env, WvState* s) {
    if (FAILED(err) || !env) return;
    env->CreateCoreWebView2Controller(s->hwnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [s](HRESULT e, ICoreWebView2Controller* c) -> HRESULT {
                OnControllerCreated(e, c, s);
                return S_OK;
            }).Get());
}

extern "C" {

wv_handle_t wv_create(int width, int height, const char* title) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WvState* s = new WvState{};
    g_wv = s;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"FlowWv";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    std::wstring wtitle;
    utf8_to_wide(title ? title : "Flow", wtitle);
    HWND hwnd = CreateWindowExW(0, L"FlowWv", wtitle.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        width, height, nullptr, nullptr, wc.hInstance, s);
    if (!hwnd) { delete s; g_wv = nullptr; return nullptr; }
    ShowWindow(hwnd, SW_SHOW);

    CreateCoreWebView2Environment(
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [s](HRESULT e, ICoreWebView2Environment* env) -> HRESULT {
                OnEnvCreated(e, env, s);
                return S_OK;
            }).Get());

    while (!s->ready && !s->closed) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) { s->closed = true; break; }
            DispatchMessageW(&m);
        }
        if (!s->ready && !s->closed) Sleep(10);
    }

    return (wv_handle_t)s;
}

void wv_navigate(wv_handle_t h, const char* url) {
    WvState* s = (WvState*)h;
    if (!s) return;
    std::wstring wurl;
    utf8_to_wide(url ? url : "about:blank", wurl);
    if (s->webview)
        s->webview->Navigate(wurl.c_str());
    else {
        s->pendingUrl = wurl;
    }
}

void wv_eval(wv_handle_t h, const char* js) {
    WvState* s = (WvState*)h;
    if (!s || !js) return;
    size_t len = strlen(js) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, js, len);
    if (copy)
        PostMessageW(s->hwnd, WM_WV_EVAL, 0, (LPARAM)copy);
}

void wv_run(wv_handle_t h) {
    WvState* s = (WvState*)h;
    if (!s) return;
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0)
        DispatchMessageW(&m);
}

void wv_close(wv_handle_t h) {
    WvState* s = (WvState*)h;
    if (s && s->hwnd)
        PostMessageW(s->hwnd, WM_CLOSE, 0, 0);
}

void wv_destroy(wv_handle_t h) {
    WvState* s = (WvState*)h;
    if (s) {
        if (g_wv == s) g_wv = nullptr;
        s->controller.Reset();
        s->webview.Reset();
        delete s;
    }
    CoUninitialize();
}

} /* extern "C" */

#endif /* _WIN32 */
