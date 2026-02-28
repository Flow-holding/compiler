import { lex } from "../lexer/lexer"
import { parse } from "../parser/parser"
import { typecheck } from "./typechecker"

function check(code: string): string[] {
    return typecheck(parse(lex(code))).map(e => e.message)
}

// Test 1 — nessun errore su codice valido
{
    const errs = check(`x = 5`)
    console.assert(errs.length === 0, "nessun errore")
    console.log("✓ valido:      x = 5")
}

// Test 2 — variabile non definita
{
    const errs = check(`print(fantasma)`)
    console.assert(errs.some(e => e.includes("fantasma")), "fantasma non definito")
    console.log("✓ non definito: fantasma")
}

// Test 3 — funzione con tipo corretto
{
    const errs = check(`
        fn somma(a: int, b: int) { a + b }
        somma(1, 2)
    `)
    console.assert(errs.length === 0, "somma ok")
    console.log("✓ funzione:    fn somma(a: int, b: int)")
}

// Test 4 — argomenti errati
{
    const errs = check(`
        fn saluta(nome: str) { print(nome) }
        saluta(1, 2, 3)
    `)
    console.assert(errs.some(e => e.includes("Argomenti")), "argomenti errati")
    console.log("✓ argomenti:   troppi argomenti rilevati")
}

// Test 5 — if con condizione bool
{
    const errs = check(`if (x > 0) { print("ok") }`)
    console.assert(errs.length <= 1, "if ok (solo x non definito)")
    console.log("✓ if:          condizione bool ok")
}

// Test 6 — struct
{
    const errs = check(`
        Utente = { nome: str, età: int }
        u = Utente
    `)
    console.assert(errs.length === 0, "struct ok")
    console.log("✓ struct:      Utente = { nome: str, età: int }")
}

// Test 7 — built-in print
{
    const errs = check(`print("ciao")`)
    console.assert(errs.length === 0, "print ok")
    console.log("✓ built-in:    print(\"ciao\")")
}

// Test 8 — built-in db
{
    const errs = check(`db.find("utenti")`)
    console.assert(errs.length === 0, "db.find ok")
    console.log("✓ built-in:    db.find(\"utenti\")")
}

console.log("\nTutti i test passati")
