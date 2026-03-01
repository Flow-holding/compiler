#pragma once
#include "../lexer/lexer.h"

// ── Tipi AST ─────────────────────────────────────────────────────

typedef enum {
    // Programma
    ND_PROGRAM,

    // Dichiarazioni
    ND_FN_DECL,
    ND_VAR_DECL,
    ND_STRUCT_DECL,
    ND_COMPONENT_DECL,
    ND_ANNOTATION,

    // Statement
    ND_BLOCK,
    ND_IF,
    ND_FOR,
    ND_RETURN,
    ND_MATCH,
    ND_MATCH_ARM,
    ND_EXPR_STMT,

    // Espressioni
    ND_BINARY,
    ND_UNARY,
    ND_CALL,
    ND_MEMBER,         // a.b
    ND_OPT_MEMBER,     // a?.b
    ND_INDEX,          // a[i]
    ND_ASSIGN,
    ND_CAST,           // x as int
    ND_PIPE,           // x |> f

    // Letterali
    ND_LIT_INT,
    ND_LIT_FLOAT,
    ND_LIT_STRING,
    ND_LIT_BOOL,
    ND_LIT_NULL,
    ND_LIT_ARRAY,      // [1, 2, 3]
    ND_LIT_MAP,        // { "k": v }
    ND_IDENT,

    // Tipi
    ND_TYPE_PRIM,      // str, int, float, bool, void, any
    ND_TYPE_ARRAY,     // T[]
    ND_TYPE_MAP,       // {K: V}
    ND_TYPE_OPTIONAL,  // ?T
    ND_TYPE_UNION,     // T | err
    ND_TYPE_FN,        // fn(T): R
    ND_TYPE_NAMED,     // Utente

    // UI
    ND_UI_NODE,
    ND_STYLE_PROP,
    ND_STYLE_BLOCK,
    ND_FRAGMENT,     // fragment implicito — più figli senza wrapper
} NodeKind;

typedef struct Node Node;
typedef struct Param Param;
typedef struct Field Field;

struct Param {
    const char* name;
    Node*       type;   // tipo opzionale
};

struct Field {
    const char* name;
    Node*       type;
    Node*       value;  // valore default opzionale
};

struct Node {
    NodeKind    kind;
    int         line;

    // Nome/valore (per ident, literal, annotation, ecc.)
    const char* name;
    const char* value;
    const char* op;

    // Figli generici
    Node*  left;
    Node*  right;
    Node*  cond;
    Node*  body;
    Node*  else_body;
    Node*  type;        // tipo dichiarato
    Node*  annotation;  // @native("...") ecc.

    // Liste
    Vec    children;    // body di blocchi, argomenti, ecc.
    Vec    params;      // parametri funzione (Param*)
    Vec    fields;      // campi struct (Field*)
    Vec    arms;        // match arms
    Vec    style;       // style props (Field*)

    // Flags
    bool   is_exported;
    bool   is_default;
};

// ── Parser ───────────────────────────────────────────────────────

typedef struct {
    TokenList   tokens;
    int         pos;
    Arena*      arena;
    ErrorList*  errors;
} Parser;

Node* parse(Arena* a, TokenList tokens, ErrorList* errors);
