export type TokenType =
    // Keyword
    | "FN"          // fn
    | "TYPE"        // type
    | "COMPONENT"   // component
    | "IF"          // if
    | "ELSE"        // else
    | "FOR"         // for
    | "IN"          // in
    | "RETURN"      // return
    | "MATCH"       // match
    | "TRUE"        // true
    | "FALSE"       // false
    | "NULL"        // null
    | "AS"          // as
    | "ERR"         // err

    // Identificatori e letterali
    | "IDENT"       // nome, saluta, Utente
    | "INT"         // 42
    | "FLOAT"       // 3.14
    | "STRING"      // "ciao {nome}"

    // Tipi primitivi
    | "T_STR"       // str
    | "T_INT"       // int
    | "T_FLOAT"     // float
    | "T_BOOL"      // bool
    | "T_VOID"      // void
    | "T_ANY"       // any

    // Simboli
    | "LPAREN"      // (
    | "RPAREN"      // )
    | "LBRACE"      // {
    | "RBRACE"      // }
    | "LBRACKET"    // [
    | "RBRACKET"    // ]
    | "COLON"       // :
    | "COMMA"       // ,
    | "DOT"         // .
    | "QUESTION"    // ?
    | "PIPE"        // |
    | "BANG"        // !
    | "AT"          // @

    // Operatori
    | "ASSIGN"      // =
    | "EQ"          // ==
    | "NEQ"         // !=
    | "LT"          // <
    | "GT"          // >
    | "LTE"         // <=
    | "GTE"         // >=
    | "PLUS"        // +
    | "MINUS"       // -
    | "STAR"        // *
    | "SLASH"       // /
    | "AND"         // &&
    | "OR"          // ||
    | "ARROW"       // =>
    | "ARRAY_TYPE"  // []  dopo un tipo → str[]

    // Speciali
    | "EOF"

export type Token = {
    type: TokenType
    value: string
    line: number
    col: number
}
