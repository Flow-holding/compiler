#include "lexer.h"
// lexer.h include già ../common.h
#include <stdarg.h>
#include <ctype.h>

static void tklist_push(Arena* a, TokenList* tl, TkKind kind, const char* val, int line, int col) {
    if (tl->len >= tl->cap) {
        tl->cap *= 2;
        tl->data = (Token*)realloc(tl->data, tl->cap * sizeof(Token));
    }
    tl->data[tl->len++] = (Token){ kind, val, line, col };
}

// Parole chiave
static TkKind keyword(const char* s, int len) {
    #define KW(str, tk) if (len == (int)strlen(str) && memcmp(s, str, len) == 0) return tk
    KW("fn",        TK_FN);
    KW("if",        TK_IF);
    KW("else",      TK_ELSE);
    KW("for",       TK_FOR);
    KW("in",        TK_IN);
    KW("return",    TK_RETURN);
    KW("match",     TK_MATCH);
    KW("struct",    TK_STRUCT);
    KW("component", TK_COMPONENT);
    KW("true",      TK_TRUE);
    KW("false",     TK_FALSE);
    KW("null",      TK_NULL);
    KW("as",        TK_AS);
    KW("type",      TK_TYPE);
    KW("default",   TK_DEFAULT);
    KW("export",    TK_EXPORT);
    KW("import",    TK_IMPORT);
    KW("str",       TK_STR_T);
    KW("int",       TK_INT_T);
    KW("float",     TK_FLOAT_T);
    KW("bool",      TK_BOOL_T);
    KW("void",      TK_VOID);
    KW("any",       TK_ANY);
    #undef KW
    return TK_IDENT;
}

TokenList lex(Arena* a, const char* src, ErrorList* errors) {
    TokenList tl;
    tl.data = (Token*)malloc(256 * sizeof(Token));
    tl.len  = 0;
    tl.cap  = 256;

    int i    = 0;
    int line = 1;
    int col  = 1;
    int n    = (int)strlen(src);

    #define PEEK()    (i < n ? src[i] : '\0')
    #define PEEK2()   (i+1 < n ? src[i+1] : '\0')
    #define ADV()     (col++, src[i++])
    #define PUSH(k,v) tklist_push(a, &tl, k, v, line, col)
    #define LIT(k)    { PUSH(k, NULL); ADV(); continue; }

    while (i < n) {
        char c = PEEK();

        // Whitespace (non newline)
        if (c == ' ' || c == '\t' || c == '\r') { ADV(); continue; }

        // Newline
        if (c == '\n') { line++; col = 1; i++; continue; }

        // Commenti // e /* */
        if (c == '/' && PEEK2() == '/') {
            while (i < n && src[i] != '\n') i++;
            continue;
        }
        if (c == '/' && PEEK2() == '*') {
            i += 2; col += 2;
            while (i < n && !(src[i] == '*' && i+1 < n && src[i+1] == '/')) {
                if (src[i] == '\n') { line++; col = 1; } else col++;
                i++;
            }
            i += 2; col += 2;
            continue;
        }

        // Annotazione @nome
        if (c == '@') {
            ADV();
            int start = i;
            while (i < n && (isalnum(src[i]) || src[i] == '_')) ADV();
            PUSH(TK_AT, str_intern(a, src + start, i - start));
            continue;
        }

        // Stringa
        if (c == '"') {
            ADV();
            int start = i;
            while (i < n && src[i] != '"') {
                if (src[i] == '\\') { ADV(); }
                ADV();
            }
            char* val = str_intern(a, src + start, i - start);
            ADV();  // chiudi "
            PUSH(TK_STRING, val);
            continue;
        }

        // Numero
        if (isdigit(c) || (c == '-' && isdigit(PEEK2()))) {
            int start = i;
            bool is_float = false;
            if (c == '-') ADV();
            while (i < n && isdigit(src[i])) ADV();
            if (i < n && src[i] == '.' && isdigit(src[i+1])) {
                is_float = true;
                ADV();
                while (i < n && isdigit(src[i])) ADV();
            }
            char* val = str_intern(a, src + start, i - start);
            PUSH(is_float ? TK_FLOAT : TK_INT, val);
            continue;
        }

        // Identificatore / keyword
        if (isalpha(c) || c == '_') {
            int start = i;
            while (i < n && (isalnum(src[i]) || src[i] == '_')) ADV();
            TkKind k   = keyword(src + start, i - start);
            char*  val = str_intern(a, src + start, i - start);
            PUSH(k, val);
            continue;
        }

        // JSX multi-char: /> e </
        if (c == '/' && PEEK2() == '>') { ADV(); ADV(); PUSH(TK_SLASH_GT, NULL); continue; }
        if (c == '<' && PEEK2() == '/') { ADV(); ADV(); PUSH(TK_LT_SLASH, NULL); continue; }

        // Operatori multi-char
        if (c == '?' && PEEK2() == '.') { ADV(); ADV(); PUSH(TK_QUEST_DOT, NULL); continue; }
        if (c == '?' && PEEK2() == '?') { ADV(); ADV(); PUSH(TK_QUEST_QUEST, NULL); continue; }
        if (c == '|' && PEEK2() == '>') { ADV(); ADV(); PUSH(TK_PIPE_GT, NULL); continue; }
        if (c == '-' && PEEK2() == '>') { ADV(); ADV(); PUSH(TK_ARROW, NULL); continue; }
        if (c == '=' && PEEK2() == '>') { ADV(); ADV(); PUSH(TK_FAT_ARROW, NULL); continue; }
        if (c == '=' && PEEK2() == '=') { ADV(); ADV(); PUSH(TK_EQ, NULL); continue; }
        if (c == '!' && PEEK2() == '=') { ADV(); ADV(); PUSH(TK_NEQ, NULL); continue; }
        if (c == '<' && PEEK2() == '=') { ADV(); ADV(); PUSH(TK_LTE, NULL); continue; }
        if (c == '>' && PEEK2() == '=') { ADV(); ADV(); PUSH(TK_GTE, NULL); continue; }
        if (c == '&' && PEEK2() == '&') { ADV(); ADV(); PUSH(TK_AND, NULL); continue; }
        if (c == '|' && PEEK2() == '|') { ADV(); ADV(); PUSH(TK_OR, NULL); continue; }
        if (c == '.' && PEEK2() == '.') { ADV(); ADV(); PUSH(TK_DOTDOT, NULL); continue; }

        // Operatori singoli
        switch (c) {
            case '+': LIT(TK_PLUS);
            case '-': LIT(TK_MINUS);
            case '*': LIT(TK_STAR);
            case '/': LIT(TK_SLASH);
            case '%': LIT(TK_PERCENT);
            case '<': LIT(TK_LT);
            case '>': LIT(TK_GT);
            case '!': LIT(TK_NOT);
            case '|': LIT(TK_PIPE);
            case '=': LIT(TK_ASSIGN);
            case ':': LIT(TK_COLON);
            case '.': LIT(TK_DOT);
            case '?': LIT(TK_QUESTION);
            case '(': LIT(TK_LPAREN);
            case ')': LIT(TK_RPAREN);
            case '{': LIT(TK_LBRACE);
            case '}': LIT(TK_RBRACE);
            case '[': LIT(TK_LBRACKET);
            case ']': LIT(TK_RBRACKET);
            case ',': LIT(TK_COMMA);
            case ';': LIT(TK_SEMICOLON);
            case '#': LIT(TK_HASH);
            default:
                errlist_add(errors, line, col, "carattere sconosciuto: '%c'", c);
                ADV();
        }
    }

    PUSH(TK_EOF, NULL);
    return tl;
}

const char* tk_name(TkKind k) {
    switch (k) {
        case TK_IDENT:     return "IDENT";
        case TK_INT:       return "INT";
        case TK_FLOAT:     return "FLOAT";
        case TK_STRING:    return "STRING";
        case TK_BOOL:      return "BOOL";
        case TK_FN:        return "fn";
        case TK_IF:        return "if";
        case TK_ELSE:      return "else";
        case TK_FOR:       return "for";
        case TK_IN:        return "in";
        case TK_RETURN:    return "return";
        case TK_MATCH:     return "match";
        case TK_STRUCT:    return "struct";
        case TK_COMPONENT: return "component";
        case TK_TRUE:      return "true";
        case TK_FALSE:     return "false";
        case TK_STR_T:     return "str";
        case TK_INT_T:     return "int";
        case TK_FLOAT_T:   return "float";
        case TK_BOOL_T:    return "bool";
        case TK_VOID:      return "void";
        case TK_ANY:       return "any";
        case TK_ASSIGN:    return "=";
        case TK_EQ:        return "==";
        case TK_NEQ:       return "!=";
        case TK_LT:        return "<";
        case TK_GT:        return ">";
        case TK_AT:        return "@";
        case TK_EOF:       return "EOF";
        default:           return "?";
    }
}
