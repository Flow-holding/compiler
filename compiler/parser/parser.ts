import { Token, TokenType } from "../lexer/token"
import * as AST from "./ast"

export function parse(tokens: Token[]): AST.Program {
    let i = 0

    const peek  = (): Token => tokens[i] ?? { type: "EOF", value: "", line: 0, col: 0 }
    const next  = (): Token => tokens[i++]
    const check = (type: TokenType): boolean => peek().type === type
    const eat   = (type: TokenType): Token => {
        if (!check(type)) throw new Error(`Riga ${peek().line}: atteso ${type}, trovato ${peek().type} ("${peek().value}")`)
        return next()
    }

    // ── Tipi ──────────────────────────────────

    function parseType(): AST.TypeExpr {
        // ?str — opzionale
        if (check("QUESTION")) {
            next()
            return { kind: "OptionalType", inner: parseType() }
        }

        // Tipi primitivi
        const primitives: Partial<Record<TokenType, AST.TypeExpr["kind"]>> = {
            T_STR: "PrimitiveType", T_INT: "PrimitiveType", T_FLOAT: "PrimitiveType",
            T_BOOL: "PrimitiveType", T_VOID: "PrimitiveType", T_ANY: "PrimitiveType"
        }
        if (primitives[peek().type]) {
            const t = next()
            const name = t.value as "str" | "int" | "float" | "bool" | "void" | "any"
            if (check("ARRAY_TYPE") || (check("LBRACKET") && tokens[i+1]?.type === "RBRACKET")) {
                if (check("LBRACKET")) { next(); next() }
                return { kind: "ArrayType", item: { kind: "PrimitiveType", name } }
            }
            return { kind: "PrimitiveType", name }
        }

        // str[] — array type diretto
        if (check("ARRAY_TYPE")) {
            const t = next()
            const base = t.value.replace("[]", "") as "str" | "int" | "float" | "bool" | "void" | "any"
            return { kind: "ArrayType", item: { kind: "PrimitiveType", name: base } }
        }

        // {str: int} — mappa
        if (check("LBRACE")) {
            next()
            const key = parseType()
            eat("COLON")
            const val = parseType()
            eat("RBRACE")
            return { kind: "MapType", key, value: val }
        }

        // Tipo custom (struct)
        if (check("IDENT")) {
            return { kind: "NamedType", name: next().value }
        }

        throw new Error(`Riga ${peek().line}: tipo non riconosciuto "${peek().value}"`)
    }

    // ── Espressioni ───────────────────────────

    function parseExpr(): AST.Node {
        return parseAssign()
    }

    function parseAssign(): AST.Node {
        const left = parseOr()
        if (check("ASSIGN")) {
            next()
            const value = parseExpr()
            return { kind: "AssignExpr", target: left, value }
        }
        return left
    }

    function parseOr(): AST.Node {
        let left = parseAnd()
        while (check("OR")) {
            const op = next().value
            left = { kind: "BinaryExpr", op, left, right: parseAnd() }
        }
        return left
    }

    function parseAnd(): AST.Node {
        let left = parseEq()
        while (check("AND")) {
            const op = next().value
            left = { kind: "BinaryExpr", op, left, right: parseEq() }
        }
        return left
    }

    function parseEq(): AST.Node {
        let left = parseComp()
        while (check("EQ") || check("NEQ")) {
            const op = next().value
            left = { kind: "BinaryExpr", op, left, right: parseComp() }
        }
        return left
    }

    function parseComp(): AST.Node {
        let left = parseAdd()
        while (check("LT") || check("GT") || check("LTE") || check("GTE")) {
            const op = next().value
            left = { kind: "BinaryExpr", op, left, right: parseAdd() }
        }
        return left
    }

    function parseAdd(): AST.Node {
        let left = parseMul()
        while (check("PLUS") || check("MINUS")) {
            const op = next().value
            left = { kind: "BinaryExpr", op, left, right: parseMul() }
        }
        return left
    }

    function parseMul(): AST.Node {
        let left = parseUnary()
        while (check("STAR") || check("SLASH")) {
            const op = next().value
            left = { kind: "BinaryExpr", op, left, right: parseUnary() }
        }
        return left
    }

    function parseUnary(): AST.Node {
        if (check("BANG")) { next(); return { kind: "UnaryExpr", op: "!", value: parseUnary() } }
        if (check("MINUS")) { next(); return { kind: "UnaryExpr", op: "-", value: parseUnary() } }
        return parseCall()
    }

    function parseCall(): AST.Node {
        let expr = parsePrimary()

        while (true) {
            if (check("LPAREN")) {
                next()
                const args: AST.Node[] = []
                while (!check("RPAREN") && !check("EOF")) {
                    args.push(parseExpr())
                    if (check("COMMA")) next()
                }
                eat("RPAREN")
                expr = { kind: "CallExpr", callee: expr, args }
            } else if (check("DOT")) {
                next()
                const prop = eat("IDENT").value
                expr = { kind: "MemberExpr", object: expr, property: prop }
            } else {
                break
            }
        }
        return expr
    }

    function parsePrimary(): AST.Node {
        const t = peek()

        if (t.type === "INT")    { next(); return { kind: "Literal", type: "int",   value: parseInt(t.value) } }
        if (t.type === "FLOAT")  { next(); return { kind: "Literal", type: "float", value: parseFloat(t.value) } }
        if (t.type === "STRING") { next(); return { kind: "Literal", type: "str",   value: t.value } }
        if (t.type === "TRUE")   { next(); return { kind: "Literal", type: "bool",  value: true } }
        if (t.type === "FALSE")  { next(); return { kind: "Literal", type: "bool",  value: false } }
        if (t.type === "NULL")   { next(); return { kind: "Literal", type: "null",  value: null } }

        if (t.type === "IDENT")  { next(); return { kind: "Identifier", name: t.value } }

        // Array letterale [1, 2, 3]
        if (t.type === "LBRACKET") {
            next()
            const items: AST.Node[] = []
            while (!check("RBRACKET") && !check("EOF")) {
                items.push(parseExpr())
                if (check("COMMA")) next()
            }
            eat("RBRACKET")
            return { kind: "ArrayLiteral", items }
        }

        // Map letterale { "k": v }  o  Block { }
        if (t.type === "LBRACE") {
            next()
            const entries: { key: AST.Node; value: AST.Node }[] = []
            while (!check("RBRACE") && !check("EOF")) {
                const key = parseExpr()
                eat("COLON")
                const val = parseExpr()
                entries.push({ key, value: val })
                if (check("COMMA")) next()
            }
            eat("RBRACE")
            return { kind: "MapLiteral", entries }
        }

        // Espressione tra parentesi
        if (t.type === "LPAREN") {
            next()
            const expr = parseExpr()
            eat("RPAREN")
            return expr
        }

        throw new Error(`Riga ${t.line}: espressione non attesa "${t.value}" (${t.type})`)
    }

    // ── Statements ────────────────────────────

    function parseBlock(): AST.Block {
        eat("LBRACE")
        const body: AST.Node[] = []
        while (!check("RBRACE") && !check("EOF")) {
            body.push(parseStatement())
        }
        eat("RBRACE")
        return { kind: "Block", body }
    }

    function parseStatement(): AST.Node {
        // if
        if (check("IF")) {
            next()
            eat("LPAREN")
            const condition = parseExpr()
            eat("RPAREN")
            const then = parseBlock()
            const elseBranch = check("ELSE") ? (next(), parseBlock()) : null
            return { kind: "IfStmt", condition, then, else: elseBranch }
        }

        // for item in lista
        if (check("FOR")) {
            next()
            const item = eat("IDENT").value
            eat("IN")
            const iterable = parseExpr()
            const body = parseBlock()
            return { kind: "ForStmt", item, iterable, body }
        }

        // return
        if (check("RETURN")) {
            next()
            const value = !check("RBRACE") && !check("EOF") ? parseExpr() : null
            return { kind: "ReturnStmt", value }
        }

        // match
        if (check("MATCH")) {
            next()
            const value = parseExpr()
            eat("LBRACE")
            const cases: AST.MatchCase[] = []
            while (!check("RBRACE") && !check("EOF")) {
                const pattern = check("IDENT") && peek().value === "_"
                    ? (next(), "_" as const)
                    : parseExpr()
                eat("ARROW")
                const body = parseBlock()
                cases.push({ pattern, body })
            }
            eat("RBRACE")
            return { kind: "MatchStmt", value, cases }
        }

        return parseExpr()
    }

    // ── Top level ─────────────────────────────

    function parseAnnotation(): AST.Annotation {
        eat("AT")
        const name = eat("IDENT").value
        let arg: string | null = null
        if (check("LPAREN")) {
            next()
            arg = eat("STRING").value
            eat("RPAREN")
        }
        return { kind: "Annotation", name, arg }
    }

    function parseTopLevel(): AST.Node {
        // Annotazione @native / @client / @server
        let annotation: AST.Annotation | null = null
        if (check("AT")) {
            annotation = parseAnnotation()
        }

        // fn
        if (check("FN")) {
            next()
            const name = eat("IDENT").value
            eat("LPAREN")
            const params: AST.Param[] = []
            while (!check("RPAREN") && !check("EOF")) {
                const pname = eat("IDENT").value
                eat("COLON")
                const ptype = parseType()
                params.push({ name: pname, type: ptype })
                if (check("COMMA")) next()
            }
            eat("RPAREN")
            const body = check("LBRACE") ? parseBlock() : { kind: "Block" as const, body: [] }
            return { kind: "FnDecl", name, params, body, annotation }
        }

        // Struct  Utente = { nome: str }
        if (check("IDENT") && tokens[i+1]?.type === "ASSIGN" && tokens[i+2]?.type === "LBRACE") {
            const name = next().value
            next() // =
            next() // {
            const fields: AST.StructField[] = []
            while (!check("RBRACE") && !check("EOF")) {
                const fname = eat("IDENT").value
                eat("COLON")
                const ftype = parseType()
                fields.push({ name: fname, type: ftype })
                if (check("COMMA")) next()
            }
            eat("RBRACE")
            return { kind: "StructDecl", name, fields }
        }

        return parseStatement()
    }

    // ── Programma ─────────────────────────────

    const body: AST.Node[] = []
    while (!check("EOF")) {
        body.push(parseTopLevel())
    }
    return { kind: "Program", body }
}
