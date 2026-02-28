#pragma once
#include "../types/strings.c"
#include "../types/arrays.c"
#include <stdlib.h>
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <process.h>
#else
  #include <unistd.h>
  #include <sys/wait.h>
#endif

// ─────────────────────────────────────────
// Process — spawn, env, exit
// Solo su native, non nel browser
// ─────────────────────────────────────────

FlowStr* flow_process_env(const char* key) {
    const char* val = getenv(key);
    return val ? flow_str_new(val) : NULL;
}

FlowStr* flow_process_cwd(void) {
    char buf[4096];
#ifdef _WIN32
    if (GetCurrentDirectoryA((DWORD)sizeof(buf), buf))
        return flow_str_new(buf);
    return flow_str_new("");
#else
    return (getcwd(buf, sizeof(buf)) != NULL) ? flow_str_new(buf) : flow_str_new("");
#endif
}

int32_t flow_process_chdir(const char* path) {
#ifdef _WIN32
    return SetCurrentDirectoryA(path) ? 1 : 0;
#else
    return chdir(path) == 0 ? 1 : 0;
#endif
}

int32_t flow_process_spawn(const char* cmd, FlowArray* args) {
#ifdef _WIN32
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    char cmdline[8192];
    strcpy(cmdline, cmd);
    if (args && flow_array_len(args) > 0) {
        for (int32_t i = 0; i < flow_array_len(args); i++) {
            FlowStr* s = (FlowStr*)flow_array_get(args, i);
            if (s && s->len > 0) {
                strcat(cmdline, " ");
                strncat(cmdline, s->data, (size_t)s->len);
                cmdline[strlen(cmdline)] = '\0';
            }
        }
    }
    if (CreateProcessA(NULL, cmdline, NULL, NULL, 0, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        return (int32_t)(intptr_t)pi.hProcess;
    }
    return 0;
#else
    pid_t pid = fork();
    if (pid == 0) {
        char* argv[256];
        int n = 0;
        argv[n++] = strdup(cmd);
        if (args) {
            for (int32_t i = 0; i < flow_array_len(args) && n < 255; i++) {
                FlowStr* s = (FlowStr*)flow_array_get(args, i);
                if (s) argv[n++] = strdup(s->data);
            }
        }
        argv[n] = NULL;
        execvp(cmd, argv);
        _exit(127);
    }
    return pid > 0 ? (int32_t)pid : 0;
#endif
}

int32_t flow_process_spawn_wait(int32_t pid) {
#ifdef _WIN32
    HANDLE h = (HANDLE)(intptr_t)pid;
    WaitForSingleObject(h, INFINITE);
    DWORD code;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return (int32_t)code;
#else
    int status;
    waitpid((pid_t)pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

void flow_process_exit(int32_t code) {
    exit(code);
}
