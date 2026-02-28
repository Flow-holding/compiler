import { Token, TokenType } from "./token"

const KEYWORDS: Record<string, TokenType> = {
    fn:        "FN",
    type:      "TYPE",
    component: "COMPONENT",
    if:        "IF",
    else:      "ELSE",
    for:       "FOR",
    in:        "IN",
    return:    "RETURN",
    match:     "MATCH",
    true:      "TRUE",
    false:     "FALSE",
    null:      "NULL",
    as:        "AS",
    err:       "ERR",
    str:       "T_STR",
    int:       "T_INT",
    float:     "T_FLOAT",
    bool:      "T_BOOL",
    void:      "T_VOID",
    any:       "T_ANY",
}

export function lex(source: string): Token[] {
    const tokens: Token[] = []
    let i    = 0
    let line = 1
    let col  = 1

    const peek = (offset = 0): string => source[i + offset] ?? ""

    const next = (): string => {
        const c = source[i++]
        if (c === "\n") { line++; col = 1 } else { col++ }
        return c
    }

    const add = (type: TokenType, value: string) =>
        tokens.push({ type, value, line, col })

    while (i < source.length) {
        const c = peek()

        // Spazi e whitespace
        if (c === " " || c === "\r" || c === "\t" || c === "\n") {
            next()
            continue
        }

        // Commenti //
        if (c === "/" && peek(1) === "/") {
            while (i < source.length && peek() !== "\n") next()
            continue
        }

        // Stringa "..."
        if (c === '"') {
            next()
            let s = ""
            while (i < source.length && peek() !== '"') {
                s += next()
            }
            next() // chiude "
            add("STRING", s)
            continue
        }

        // Numeri
        if (c >= "0" && c <= "9") {
            let n = ""
            while (peek() >= "0" && peek() <= "9") n += next()
            if (peek() === "." && peek(1) >= "0" && peek(1) <= "9") {
                n += next()
                while (peek() >= "0" && peek() <= "9") n += next()
                add("FLOAT", n)
            } else {
                add("INT", n)
            }
            continue
        }

        // Identificatori e keyword
        if ((c >= "a" && c <= "z") || (c >= "A" && c <= "Z") || c === "_") {
            let word = ""
            while (
                (peek() >= "a" && peek() <= "z") ||
                (peek() >= "A" && peek() <= "Z") ||
                (peek() >= "0" && peek() <= "9") ||
                peek() === "_"
            ) {
                word += next()
            }
            // str[] → ARRAY_TYPE
            if (KEYWORDS[word] !== undefined && peek() === "[" && peek(1) === "]") {
                next(); next()
                add("ARRAY_TYPE", word + "[]")
                continue
            }
            add(KEYWORDS[word] ?? "IDENT", word)
            continue
        }

        // Operatori doppi — controllati prima dei singoli
        if (c === "=" && peek(1) === "=") { next(); next(); add("EQ",    "=="); continue }
        if (c === "!" && peek(1) === "=") { next(); next(); add("NEQ",   "!="); continue }
        if (c === "<" && peek(1) === "=") { next(); next(); add("LTE",   "<="); continue }
        if (c === ">" && peek(1) === "=") { next(); next(); add("GTE",   ">="); continue }
        if (c === "&" && peek(1) === "&") { next(); next(); add("AND",   "&&"); continue }
        if (c === "|" && peek(1) === "|") { next(); next(); add("OR",    "||"); continue }
        if (c === "=" && peek(1) === ">") { next(); next(); add("ARROW", "=>"); continue }
        if (c === "[" && peek(1) === "]") { next(); next(); add("ARRAY_TYPE", "[]"); continue }

        // Simboli singoli
        next()
        switch (c) {
            case "(": add("LPAREN",   c); break
            case ")": add("RPAREN",   c); break
            case "{": add("LBRACE",   c); break
            case "}": add("RBRACE",   c); break
            case "[": add("LBRACKET", c); break
            case "]": add("RBRACKET", c); break
            case ":": add("COLON",    c); break
            case ",": add("COMMA",    c); break
            case ".": add("DOT",      c); break
            case "?": add("QUESTION", c); break
            case "|": add("PIPE",     c); break
            case "!": add("BANG",     c); break
            case "@": add("AT",       c); break
            case "=": add("ASSIGN",   c); break
            case "<": add("LT",       c); break
            case ">": add("GT",       c); break
            case "+": add("PLUS",     c); break
            case "-": add("MINUS",    c); break
            case "*": add("STAR",     c); break
            case "/": add("SLASH",    c); break
        }
    }

    add("EOF", "")
    return tokens
}
