import { lex } from "../lexer/lexer"
import { parse } from "./parser"

// Test 1 — variabile
{
    const ast = parse(lex(`x = 5`))
    const node = ast.body[0] as any
    console.assert(node.kind === "AssignExpr", "AssignExpr")
    console.assert(node.target.name === "x",  "target x")
    console.assert(node.value.value === 5,     "value 5")
    console.log("✓ variabile:   x = 5")
}

// Test 2 — funzione
{
    const ast = parse(lex(`fn saluta(nome: str) { }`))
    const node = ast.body[0] as any
    console.assert(node.kind === "FnDecl",               "FnDecl")
    console.assert(node.name === "saluta",               "nome saluta")
    console.assert(node.params[0].name === "nome",       "param nome")
    console.assert(node.params[0].type.name === "str",   "tipo str")
    console.log("✓ funzione:    fn saluta(nome: str) { }")
}

// Test 3 — struct
{
    const ast = parse(lex(`Utente = { nome: str, età: int }`))
    const node = ast.body[0] as any
    console.assert(node.kind === "StructDecl",           "StructDecl")
    console.assert(node.name === "Utente",               "nome Utente")
    console.assert(node.fields.length === 2,             "2 campi")
    console.log("✓ struct:      Utente = { nome: str, età: int }")
}

// Test 4 — if
{
    const ast = parse(lex(`if (x > 0) { print(x) }`))
    const node = ast.body[0] as any
    console.assert(node.kind === "IfStmt",               "IfStmt")
    console.assert(node.condition.op === ">",            "op >")
    console.log("✓ if:          if (x > 0) { ... }")
}

// Test 5 — for
{
    const ast = parse(lex(`for item in lista { print(item) }`))
    const node = ast.body[0] as any
    console.assert(node.kind === "ForStmt",              "ForStmt")
    console.assert(node.item === "item",                 "item")
    console.assert(node.iterable.name === "lista",       "iterable lista")
    console.log("✓ for:         for item in lista { ... }")
}

// Test 6 — annotazione @native
{
    const ast = parse(lex(`@native("flow_print")\nfn print(s: str) { }`))
    const node = ast.body[0] as any
    console.assert(node.kind === "FnDecl",               "FnDecl")
    console.assert(node.annotation.name === "native",    "annotation native")
    console.assert(node.annotation.arg === "flow_print", "arg flow_print")
    console.log("✓ annotation:  @native(\"flow_print\")")
}

// Test 7 — espressione binaria
{
    const ast = parse(lex(`x + y * 2`))
    const node = ast.body[0] as any
    console.assert(node.kind === "BinaryExpr",           "BinaryExpr")
    console.assert(node.op === "+",                      "op +")
    console.assert(node.right.op === "*",                "destra *")
    console.log("✓ espressione: x + y * 2")
}

console.log("\nTutti i test passati")
