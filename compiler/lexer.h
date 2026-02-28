#pragma once
#include "common.h"

typedef enum {
    // Letterali
    TK_IDENT, TK_INT, TK_FLOAT, TK_STRING, TK_BOOL,

    // Keywords
    TK_FN, TK_IF, TK_ELSE, TK_FOR, TK_IN, TK_RETURN,
    TK_MATCH, TK_STRUCT, TK_COMPONENT, TK_TRUE, TK_FALSE,
    TK_NULL, TK_AS, TK_TYPE,

    // Tipi primitivi
    TK_STR_T, TK_INT_T, TK_FLOAT_T, TK_BOOL_T, TK_VOID, TK_ANY,

    // Operatori
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LTE, TK_GTE,
    TK_AND, TK_OR, TK_NOT, TK_PIPE,
    TK_ASSIGN,        // =
    TK_COLON,         // :
    TK_ARROW,         // ->
    TK_FAT_ARROW,     // =>
    TK_DOT,           // .
    TK_DOTDOT,        // ..
    TK_QUESTION,      // ?
    TK_QUEST_DOT,     // ?.
    TK_QUEST_QUEST,   // ??
    TK_AT,            // @
    TK_HASH,          // #
    TK_PIPE_GT,       // |>

    // Delimitatori
    TK_LPAREN, TK_RPAREN,
    TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_SEMICOLON, TK_NEWLINE,

    TK_EOF,
} TkKind;

typedef struct {
    TkKind      kind;
    const char* val;   // puntatore nell'arena — non alloca
    int         line;
    int         col;
} Token;

typedef struct {
    Token* data;
    int    len;
    int    cap;
} TokenList;

// Tokenizza source e restituisce una lista di token
TokenList lex(Arena* a, const char* source, ErrorList* errors);

// Debug
const char* tk_name(TkKind k);
