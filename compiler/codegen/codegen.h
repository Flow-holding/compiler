#pragma once
#include "../parser/parser.h"

typedef enum { TARGET_NATIVE, TARGET_WASM } CodegenTarget;

// Genera codice C dal AST
// native_map_path: file "key=value" per override da flow/ e flow-os/
Str codegen_c(Arena* a, Node* program, CodegenTarget target, const char* runtime_dir, const char* native_map_path);

// Genera HTML dal AST (componenti @client)
Str codegen_html(Arena* a, Node* program);

// Genera CSS dai componenti
Str codegen_css(Arena* a, Node* program);

// Genera JS glue per WASM
Str codegen_js(Arena* a, Node* program);
