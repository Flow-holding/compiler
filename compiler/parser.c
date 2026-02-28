#include "parser.h"
#include <stdarg.h>
#include <ctype.h>

// ── Helpers parser ───────────────────────────────────────────────

static Token* peek(Parser* p) {
    return &p->tokens.data[p->pos];
}

static Token* peek2(Parser* p) {
    int next = p->pos + 1;
    if (next >= p->tokens.len) next = p->tokens.len - 1;
    return &p->tokens.data[next];
}

static Token* advance(Parser* p) {
    Token* t = &p->tokens.data[p->pos];
    if (p->pos < p->tokens.len - 1) p->pos++;
    return t;
}

static bool check(Parser* p, TkKind k) {
    return peek(p)->kind == k;
}

static bool match(Parser* p, TkKind k) {
    if (check(p, k)) { advance(p); return true; }
    return false;
}

static Token* eat(Parser* p, TkKind k) {
    if (!check(p, k)) {
        Token* t = peek(p);
        errlist_add(p->errors, t->line, t->col,
            "atteso '%s', trovato '%s'", tk_name(k), tk_name(t->kind));
        return t;
    }
    return advance(p);
}

static Node* node_new(Parser* p, NodeKind kind) {
    Node* n  = (Node*)arena_alloc(p->arena, sizeof(Node));
    n->kind  = kind;
    n->line  = peek(p)->line;
    n->children = vec_new();
    n->params   = vec_new();
    n->fields   = vec_new();
    n->arms     = vec_new();
    n->style    = vec_new();
    return n;
}

// ── Forward declarations ─────────────────────────────────────────

static Node* parse_expr(Parser* p);
static Node* parse_stmt(Parser* p);
static Node* parse_block(Parser* p);
static Node* parse_type(Parser* p);
static Node* parse_top(Parser* p);

// ── Tipi ─────────────────────────────────────────────────────────

static Node* parse_type(Parser* p) {
    Token* t = peek(p);
    Node*  n = node_new(p, ND_TYPE_PRIM);

    // Opzionale ?T
    if (match(p, TK_QUESTION)) {
        n->kind = ND_TYPE_OPTIONAL;
        n->left = parse_type(p);
        return n;
    }

    // fn(T, T): R
    if (match(p, TK_FN)) {
        n->kind = ND_TYPE_FN;
        eat(p, TK_LPAREN);
        while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
            vec_push(&n->params, parse_type(p));
            if (!match(p, TK_COMMA)) break;
        }
        eat(p, TK_RPAREN);
        if (match(p, TK_COLON)) n->type = parse_type(p);
        return n;
    }

    // { K: V }
    if (match(p, TK_LBRACE)) {
        n->kind  = ND_TYPE_MAP;
        n->left  = parse_type(p);
        eat(p, TK_COLON);
        n->right = parse_type(p);
        eat(p, TK_RBRACE);
        return n;
    }

    // Primitivi
    switch (t->kind) {
        case TK_STR_T:   n->name = "str";   advance(p); break;
        case TK_INT_T:   n->name = "int";   advance(p); break;
        case TK_FLOAT_T: n->name = "float"; advance(p); break;
        case TK_BOOL_T:  n->name = "bool";  advance(p); break;
        case TK_VOID:    n->name = "void";  advance(p); break;
        case TK_ANY:     n->name = "any";   advance(p); break;
        case TK_IDENT:
            n->kind = ND_TYPE_NAMED;
            n->name = t->val;
            advance(p);
            break;
        default:
            errlist_add(p->errors, t->line, t->col, "tipo atteso, trovato '%s'", tk_name(t->kind));
            advance(p);
            return n;
    }

    // T[]
    if (match(p, TK_LBRACKET)) {
        eat(p, TK_RBRACKET);
        Node* arr  = node_new(p, ND_TYPE_ARRAY);
        arr->left  = n;
        return arr;
    }

    // T | err
    if (match(p, TK_PIPE)) {
        Node* u  = node_new(p, ND_TYPE_UNION);
        u->left  = n;
        u->right = parse_type(p);
        return u;
    }

    return n;
}

// ── Espressioni ──────────────────────────────────────────────────

static Node* parse_primary(Parser* p) {
    Token* t = peek(p);
    Node*  n = node_new(p, ND_LIT_INT);

    switch (t->kind) {
        case TK_INT:    n->kind = ND_LIT_INT;    n->value = t->val; advance(p); return n;
        case TK_FLOAT:  n->kind = ND_LIT_FLOAT;  n->value = t->val; advance(p); return n;
        case TK_STRING: n->kind = ND_LIT_STRING; n->value = t->val; advance(p); return n;
        case TK_TRUE:   n->kind = ND_LIT_BOOL;   n->value = "true";  advance(p); return n;
        case TK_FALSE:  n->kind = ND_LIT_BOOL;   n->value = "false"; advance(p); return n;
        case TK_NULL:   n->kind = ND_LIT_NULL;   advance(p); return n;

        case TK_LBRACKET: {
            n->kind = ND_LIT_ARRAY;
            advance(p);
            while (!check(p, TK_RBRACKET) && !check(p, TK_EOF)) {
                vec_push(&n->children, parse_expr(p));
                if (!match(p, TK_COMMA)) break;
            }
            eat(p, TK_RBRACKET);
            return n;
        }

        case TK_LBRACE: {
            n->kind = ND_LIT_MAP;
            advance(p);
            while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
                Field* f   = (Field*)arena_alloc(p->arena, sizeof(Field));
                // Accetta sia "stringa" che identificatore come chiave
                Token* key = peek(p);
                if (key->kind == TK_STRING || key->kind == TK_IDENT ||
                    key->kind == TK_STR_T  || key->kind == TK_INT_T  ||
                    key->kind == TK_BOOL_T || key->kind == TK_TYPE) {
                    f->name = key->val ? key->val : tk_name(key->kind);
                    advance(p);
                } else {
                    errlist_add(p->errors, key->line, key->col, "chiave attesa nel map");
                    advance(p); break;
                }
                eat(p, TK_COLON);
                f->value   = parse_expr(p);
                vec_push(&n->fields, f);
                match(p, TK_COMMA);
            }
            eat(p, TK_RBRACE);
            return n;
        }

        case TK_IDENT: {
            n->kind = ND_IDENT;
            n->name = t->val;
            advance(p);

            // Chiamata funzione
            if (check(p, TK_LPAREN)) {
                n->kind = ND_CALL;
                advance(p);
                while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                    vec_push(&n->children, parse_expr(p));
                    if (!match(p, TK_COMMA)) break;
                }
                eat(p, TK_RPAREN);
            }

            // UI component { ... }
            if (check(p, TK_LBRACE) && isupper((unsigned char)n->name[0])) {
                n->kind = ND_UI_NODE;
                advance(p);
                while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
                    // prop: value  o  ChildNode { }
                    if (check(p, TK_IDENT) && peek2(p)->kind == TK_COLON) {
                        Field* f = (Field*)arena_alloc(p->arena, sizeof(Field));
                        f->name  = advance(p)->val;
                        advance(p);  // :
                        f->value = parse_expr(p);
                        vec_push(&n->style, f);
                    } else {
                        vec_push(&n->children, parse_expr(p));
                    }
                    match(p, TK_COMMA);
                }
                eat(p, TK_RBRACE);
            }
            return n;
        }

        case TK_LPAREN: {
            advance(p);
            Node* inner = parse_expr(p);
            eat(p, TK_RPAREN);
            return inner;
        }

        default:
            errlist_add(p->errors, t->line, t->col, "espressione attesa, trovato '%s'", tk_name(t->kind));
            advance(p);
            return n;
    }
}

static Node* parse_postfix(Parser* p) {
    Node* left = parse_primary(p);

    while (true) {
        if (match(p, TK_DOT)) {
            Node* n  = node_new(p, ND_MEMBER);
            n->left  = left;
            n->name  = eat(p, TK_IDENT)->val;
            if (match(p, TK_LPAREN)) {
                n->kind = ND_CALL;
                while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                    vec_push(&n->children, parse_expr(p));
                    if (!match(p, TK_COMMA)) break;
                }
                eat(p, TK_RPAREN);
            }
            left = n;
        } else if (match(p, TK_QUEST_DOT)) {
            Node* n  = node_new(p, ND_OPT_MEMBER);
            n->left  = left;
            n->name  = eat(p, TK_IDENT)->val;
            left = n;
        } else if (match(p, TK_LBRACKET)) {
            Node* n  = node_new(p, ND_INDEX);
            n->left  = left;
            n->right = parse_expr(p);
            eat(p, TK_RBRACKET);
            left = n;
        } else if (match(p, TK_AS)) {
            Node* n  = node_new(p, ND_CAST);
            n->left  = left;
            n->type  = parse_type(p);
            left = n;
        } else {
            break;
        }
    }
    return left;
}

static Node* parse_unary(Parser* p) {
    if (check(p, TK_NOT) || check(p, TK_MINUS)) {
        Node* n  = node_new(p, ND_UNARY);
        n->op    = tk_name(advance(p)->kind);
        n->right = parse_unary(p);
        return n;
    }
    return parse_postfix(p);
}

// Precedenza operatori binari
static int op_prec(TkKind k) {
    switch (k) {
        case TK_OR:       return 1;
        case TK_AND:      return 2;
        case TK_EQ: case TK_NEQ: return 3;
        case TK_LT: case TK_GT: case TK_LTE: case TK_GTE: return 4;
        case TK_PIPE:     return 5;
        case TK_PLUS: case TK_MINUS: return 6;
        case TK_STAR: case TK_SLASH: case TK_PERCENT: return 7;
        case TK_PIPE_GT:  return 8;
        case TK_QUEST_QUEST: return 0;
        default:          return -1;
    }
}

static Node* parse_binary(Parser* p, int min_prec) {
    Node* left = parse_unary(p);

    while (true) {
        int prec = op_prec(peek(p)->kind);
        if (prec < min_prec) break;
        Token* op_tok = advance(p);
        Node* right   = parse_binary(p, prec + 1);
        Node* n       = node_new(p, ND_BINARY);
        n->left  = left;
        n->right = right;
        n->op    = op_tok->val ? op_tok->val : tk_name(op_tok->kind);
        left = n;
    }
    return left;
}

static Node* parse_expr(Parser* p) {
    Node* left = parse_binary(p, 0);

    // Assegnazione
    if (match(p, TK_ASSIGN)) {
        Node* n  = node_new(p, ND_ASSIGN);
        n->left  = left;
        n->right = parse_expr(p);
        return n;
    }
    return left;
}

// ── Statement ────────────────────────────────────────────────────

static Node* parse_block(Parser* p) {
    Node* n = node_new(p, ND_BLOCK);
    eat(p, TK_LBRACE);
    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        Node* s = parse_stmt(p);
        if (s) vec_push(&n->children, s);
    }
    eat(p, TK_RBRACE);
    return n;
}

static Node* parse_stmt(Parser* p) {
    Token* t = peek(p);

    // return
    if (match(p, TK_RETURN)) {
        Node* n = node_new(p, ND_RETURN);
        if (!check(p, TK_RBRACE) && !check(p, TK_EOF))
            n->left = parse_expr(p);
        return n;
    }

    // if
    if (match(p, TK_IF)) {
        Node* n = node_new(p, ND_IF);
        eat(p, TK_LPAREN);
        n->cond = parse_expr(p);
        eat(p, TK_RPAREN);
        n->body = parse_block(p);
        if (match(p, TK_ELSE))
            n->else_body = check(p, TK_IF) ? parse_stmt(p) : parse_block(p);
        return n;
    }

    // for x in list
    if (match(p, TK_FOR)) {
        Node* n  = node_new(p, ND_FOR);
        n->name  = eat(p, TK_IDENT)->val;
        eat(p, TK_IN);
        n->left  = parse_expr(p);
        n->body  = parse_block(p);
        return n;
    }

    // match
    if (match(p, TK_MATCH)) {
        Node* n = node_new(p, ND_MATCH);
        n->left = parse_expr(p);
        eat(p, TK_LBRACE);
        while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
            Node* arm   = node_new(p, ND_MATCH_ARM);
            arm->name   = eat(p, TK_IDENT)->val;  // pattern
            if (match(p, TK_LPAREN)) {
                arm->value = eat(p, TK_IDENT)->val;
                eat(p, TK_RPAREN);
            }
            arm->body   = parse_block(p);
            vec_push(&n->arms, arm);
        }
        eat(p, TK_RBRACE);
        return n;
    }

    // Variabile: nome = expr  o  nome: tipo = expr
    if (t->kind == TK_IDENT && (peek2(p)->kind == TK_ASSIGN || peek2(p)->kind == TK_COLON)) {
        Node* n = node_new(p, ND_VAR_DECL);
        n->name = advance(p)->val;
        if (match(p, TK_COLON)) n->type = parse_type(p);
        if (match(p, TK_ASSIGN)) n->left = parse_expr(p);
        return n;
    }

    // Espressione-statement
    Node* expr = parse_expr(p);
    Node* s    = node_new(p, ND_EXPR_STMT);
    s->left    = expr;
    return s;
}

// ── Top-level ────────────────────────────────────────────────────

static Node* parse_annotation(Parser* p) {
    Node* n   = node_new(p, ND_ANNOTATION);
    n->name   = peek(p)->val;  // già consumato il @ nel lexer, val è il nome
    advance(p);
    if (match(p, TK_LPAREN)) {
        if (check(p, TK_STRING)) n->value = advance(p)->val;
        eat(p, TK_RPAREN);
    }
    return n;
}

static Node* parse_top(Parser* p) {
    Node* ann = NULL;

    // Annotazione @nome(...)
    if (check(p, TK_AT)) {
        ann = parse_annotation(p);
    }

    Token* t = peek(p);

    // fn nome(params): tipo { body }
    if (match(p, TK_FN)) {
        Node* n      = node_new(p, ND_FN_DECL);
        n->name      = eat(p, TK_IDENT)->val;
        n->annotation = ann;
        eat(p, TK_LPAREN);
        while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
            Param* param  = (Param*)arena_alloc(p->arena, sizeof(Param));
            param->name   = eat(p, TK_IDENT)->val;
            if (match(p, TK_COLON)) param->type = parse_type(p);
            vec_push(&n->params, param);
            if (!match(p, TK_COMMA)) break;
        }
        eat(p, TK_RPAREN);
        if (match(p, TK_COLON)) n->type = parse_type(p);
        if (check(p, TK_LBRACE)) n->body = parse_block(p);
        return n;
    }

    // struct Nome { fields }
    if (match(p, TK_STRUCT)) {
        Node* n  = node_new(p, ND_STRUCT_DECL);
        n->name  = eat(p, TK_IDENT)->val;
        n->annotation = ann;
        eat(p, TK_LBRACE);
        while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
            Field* f = (Field*)arena_alloc(p->arena, sizeof(Field));
            f->name  = eat(p, TK_IDENT)->val;
            eat(p, TK_COLON);
            f->type  = parse_type(p);
            if (match(p, TK_ASSIGN)) f->value = parse_expr(p);
            vec_push(&n->fields, f);
            match(p, TK_COMMA);
        }
        eat(p, TK_RBRACE);
        return n;
    }

    // component Nome(props) { body }
    if (match(p, TK_COMPONENT)) {
        Node* n  = node_new(p, ND_COMPONENT_DECL);
        n->name  = eat(p, TK_IDENT)->val;
        n->annotation = ann;
        if (match(p, TK_LPAREN)) {
            while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                Param* param  = (Param*)arena_alloc(p->arena, sizeof(Param));
                param->name   = eat(p, TK_IDENT)->val;
                if (match(p, TK_COLON)) param->type = parse_type(p);
                vec_push(&n->params, param);
                if (!match(p, TK_COMMA)) break;
            }
            eat(p, TK_RPAREN);
        }
        n->body = parse_block(p);
        return n;
    }

    // Variabile globale
    if (t->kind == TK_IDENT) {
        Node* n = node_new(p, ND_VAR_DECL);
        n->name = advance(p)->val;
        n->annotation = ann;
        if (match(p, TK_COLON)) n->type = parse_type(p);
        if (match(p, TK_ASSIGN)) n->left = parse_expr(p);
        return n;
    }

    errlist_add(p->errors, t->line, t->col, "dichiarazione attesa, trovato '%s'", tk_name(t->kind));
    advance(p);
    return NULL;
}

// ── Entry point ──────────────────────────────────────────────────

Node* parse(Arena* a, TokenList tokens, ErrorList* errors) {
    Parser p = { tokens, 0, a, errors };
    Node*  prog = node_new(&p, ND_PROGRAM);

    while (!check(&p, TK_EOF)) {
        Node* n = parse_top(&p);
        if (n) vec_push(&prog->children, n);
    }
    return prog;
}
