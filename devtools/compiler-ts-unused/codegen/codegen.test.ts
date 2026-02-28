import { lex } from "../lexer/lexer"
import { parse } from "../parser/parser"
import { codegen } from "./codegen"

function gen(code: string): string {
    return codegen(parse(lex(code)))
}

// Test 1 — stringa
{
    const c = gen(`print("ciao")`)
    console.assert(c.includes("flow_print"), "flow_print")
    console.assert(c.includes('flow_str_new("ciao")'), "str_new")
    console.log("✓ print str:   flow_print(flow_str_new(...))")
}

// Test 2 — numero
{
    const c = gen(`print(42)`)
    console.assert(c.includes("flow_print_int(42)"), "print_int")
    console.log("✓ print int:   flow_print_int(42)")
}

// Test 3 — funzione
{
    const c = gen(`fn saluta(nome: str) { print(nome) }`)
    console.assert(c.includes("void saluta(FlowStr* nome)"), "firma fn")
    console.assert(c.includes("flow_print(nome)"), "body fn")
    console.log("✓ funzione:    void saluta(FlowStr* nome)")
}

// Test 4 — struct
{
    const c = gen(`Utente = { nome: str, età: int }`)
    console.assert(c.includes("typedef struct"), "typedef struct")
    console.assert(c.includes("Flow_Utente"),    "Flow_Utente")
    console.assert(c.includes("FlowStr* nome"),  "campo nome")
    console.log("✓ struct:      typedef struct Flow_Utente")
}

// Test 5 — if
{
    const c = gen(`if (x > 0) { print("ok") }`)
    console.assert(c.includes("if ((x > 0))"), "if generato")
    console.log("✓ if:          if ((x > 0))")
}

// Test 6 — for
{
    const c = gen(`for item in lista { print(item) }`)
    console.assert(c.includes("flow_array_len"), "for usa array_len")
    console.assert(c.includes("flow_array_get"), "for usa array_get")
    console.log("✓ for:         for item in lista")
}

// Test 7 — @native ignorato
{
    const c = gen(`@native("flow_print")\nfn print(s: str) { }`)
    console.assert(!c.includes("void print("), "@native non genera codice")
    console.log("✓ @native:     non genera codice C")
}

// Test 8 — fs built-in
{
    const c = gen(`fs.read("config.json")`)
    console.assert(c.includes("flow_fs_read"), "flow_fs_read")
    console.log("✓ fs.read:     flow_fs_read(...)")
}

console.log("\nTutti i test passati")
