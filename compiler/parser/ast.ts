// AST — struttura ad albero del codice Flow

export type Node =
    | Program
    | FnDecl
    | VarDecl
    | StructDecl
    | Annotation
    | Block
    | IfStmt
    | ForStmt
    | ReturnStmt
    | MatchStmt
    | CallExpr
    | BinaryExpr
    | UnaryExpr
    | MemberExpr
    | AssignExpr
    | Identifier
    | Literal
    | ArrayLiteral
    | MapLiteral

// ── Top level ───────────────────────────────

export type Program = {
    kind: "Program"
    body: Node[]
}

// fn saluta(nome: str) { ... }
export type FnDecl = {
    kind: "FnDecl"
    name: string
    params: Param[]
    body: Block
    annotation: Annotation | null
}

export type Param = {
    name: string
    type: TypeExpr
}

// x = 5  oppure  x: int = 5
export type VarDecl = {
    kind: "VarDecl"
    name: string
    type: TypeExpr | null   // null = inferito
    value: Node
}

// Utente = { nome: str, età: int }
export type StructDecl = {
    kind: "StructDecl"
    name: string
    fields: StructField[]
}

export type StructField = {
    name: string
    type: TypeExpr
}

// @native("flow_print")
export type Annotation = {
    kind: "Annotation"
    name: string            // "native", "client", "server"
    arg: string | null      // "flow_print" oppure null
}

// ── Statements ──────────────────────────────

export type Block = {
    kind: "Block"
    body: Node[]
}

// if (x > 0) { } else { }
export type IfStmt = {
    kind: "IfStmt"
    condition: Node
    then: Block
    else: Block | null
}

// for item in lista { }
export type ForStmt = {
    kind: "ForStmt"
    item: string
    iterable: Node
    body: Block
}

// return x  (esplicito — di solito non serve)
export type ReturnStmt = {
    kind: "ReturnStmt"
    value: Node | null
}

// match x { "a" => { } _ => { } }
export type MatchStmt = {
    kind: "MatchStmt"
    value: Node
    cases: MatchCase[]
}

export type MatchCase = {
    pattern: Node | "_"
    body: Block
}

// ── Espressioni ─────────────────────────────

// saluta("Mario")
export type CallExpr = {
    kind: "CallExpr"
    callee: Node
    args: Node[]
}

// x + y  /  x == y  /  x && y
export type BinaryExpr = {
    kind: "BinaryExpr"
    op: string
    left: Node
    right: Node
}

// !x  /  -x
export type UnaryExpr = {
    kind: "UnaryExpr"
    op: string
    value: Node
}

// utente.nome  /  lista.len
export type MemberExpr = {
    kind: "MemberExpr"
    object: Node
    property: string
}

// x = 5
export type AssignExpr = {
    kind: "AssignExpr"
    target: Node
    value: Node
}

// ── Letterali ───────────────────────────────

export type Identifier = {
    kind: "Identifier"
    name: string
}

export type Literal = {
    kind: "Literal"
    type: "str" | "int" | "float" | "bool" | "null"
    value: string | number | boolean | null
}

// ["a", "b", "c"]
export type ArrayLiteral = {
    kind: "ArrayLiteral"
    items: Node[]
}

// { "nome": "Mario", "età": 25 }
export type MapLiteral = {
    kind: "MapLiteral"
    entries: { key: Node; value: Node }[]
}

// ── Tipi ────────────────────────────────────

export type TypeExpr =
    | { kind: "PrimitiveType"; name: "str" | "int" | "float" | "bool" | "void" | "any" }
    | { kind: "ArrayType";     item: TypeExpr }
    | { kind: "MapType";       key: TypeExpr; value: TypeExpr }
    | { kind: "OptionalType";  inner: TypeExpr }
    | { kind: "NamedType";     name: string }
