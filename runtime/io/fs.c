#pragma once
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../types/strings.c"
#include "../types/arrays.c"
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <direct.h>
  #ifndef S_ISDIR
  #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
  #endif
  #ifndef S_ISREG
  #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
  #endif
  #define rmdir(p) _rmdir(p)
#else
  #include <dirent.h>
#endif

// ─────────────────────────────────────────
// File system — leggi e scrivi file
// Solo lato server, non nel browser
// ─────────────────────────────────────────

// Legge un file e ritorna il contenuto come FlowStr
// Ritorna NULL se il file non esiste
FlowStr* flow_fs_read(const char* percorso) {
    FILE* f = fopen(percorso, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    FlowStr* s = (FlowStr*)flow_alloc(sizeof(FlowStr) + size + 1);
    s->len = (int32_t)size;
    fread(s->data, 1, size, f);
    s->data[size] = '\0';
    fclose(f);
    return s;
}

// Scrive una stringa su file
// Ritorna 1 se ok, 0 se errore
int32_t flow_fs_write(const char* percorso, FlowStr* contenuto) {
    FILE* f = fopen(percorso, "wb");
    if (!f) return 0;
    fwrite(contenuto->data, 1, contenuto->len, f);
    fclose(f);
    return 1;
}

// Controlla se un file esiste
int32_t flow_fs_exists(const char* percorso) {
    FILE* f = fopen(percorso, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

// Cancella un file
int32_t flow_fs_delete(const char* percorso) {
    return remove(percorso) == 0 ? 1 : 0;
}

// Crea directory
int32_t flow_fs_mkdir(const char* percorso) {
#ifdef _WIN32
    return CreateDirectoryA(percorso, NULL) ? 1 : 0;
#else
    return mkdir(percorso, 0755) == 0 ? 1 : 0;
#endif
}

// Rimuovi directory vuota
int32_t flow_fs_rmdir(const char* percorso) {
    return rmdir(percorso) == 0 ? 1 : 0;
}

// Lista contenuto directory — ritorna array di nomi
FlowArray* flow_fs_list_dir(const char* percorso) {
    FlowArray* arr = flow_array_new();
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", percorso);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return arr;
    do {
        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
            arr = flow_array_push(arr, (FlowObject*)flow_str_new(fd.cFileName));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(percorso);
    if (!d) return arr;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
            arr = flow_array_push(arr, (FlowObject*)flow_str_new(ent->d_name));
    }
    closedir(d);
#endif
    return arr;
}

// È directory?
int32_t flow_fs_is_dir(const char* percorso) {
    struct stat st;
    return (stat(percorso, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

// È file?
int32_t flow_fs_is_file(const char* percorso) {
    struct stat st;
    return (stat(percorso, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

// Dimensione file in byte
int32_t flow_fs_size(const char* percorso) {
    struct stat st;
    return (stat(percorso, &st) == 0) ? (int32_t)st.st_size : 0;
}
