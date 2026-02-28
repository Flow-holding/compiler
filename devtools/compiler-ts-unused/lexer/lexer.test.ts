import { lex } from "./lexer"

// Test 1 — variabile semplice
{
    const tokens = lex(`x = 5`)
    console.assert(tokens[0].type === "IDENT",  "IDENT x")
    console.assert(tokens[1].type === "ASSIGN",  "ASSIGN =")
    console.assert(tokens[2].type === "INT",     "INT 5")
    console.log("✓ variabile:  x = 5")
}

// Test 2 — funzione
{
    const tokens = lex(`fn saluta(nome: str) { }`)
    console.assert(tokens[0].type === "FN",      "FN")
    console.assert(tokens[1].type === "IDENT",   "IDENT saluta")
    console.assert(tokens[2].type === "LPAREN",  "LPAREN")
    console.assert(tokens[3].type === "IDENT",   "IDENT nome")
    console.assert(tokens[4].type === "COLON",   "COLON")
    console.assert(tokens[5].type === "T_STR",   "T_STR")
    console.assert(tokens[6].type === "RPAREN",  "RPAREN")
    console.log("✓ funzione:   fn saluta(nome: str) { }")
}

// Test 3 — stringa
{
    const tokens = lex(`"ciao mondo"`)
    console.assert(tokens[0].type === "STRING",  "STRING")
    console.assert(tokens[0].value === "ciao mondo", "valore stringa")
    console.log("✓ stringa:    \"ciao mondo\"")
}

// Test 4 — array type
{
    const tokens = lex(`str[]`)
    console.assert(tokens[0].type === "ARRAY_TYPE", "ARRAY_TYPE str[]")
    console.log("✓ array type: str[]")
}

// Test 5 — operatori
{
    const tokens = lex(`x == 5 && y != 0`)
    console.assert(tokens[1].type === "EQ",  "EQ ==")
    console.assert(tokens[3].type === "AND", "AND &&")
    console.assert(tokens[5].type === "NEQ", "NEQ !=")
    console.log("✓ operatori:  == && !=")
}

// Test 6 — commento ignorato
{
    const tokens = lex(`// questo è un commento\nx = 1`)
    console.assert(tokens[0].type === "IDENT", "commento ignorato")
    console.log("✓ commento:   // ignorato")
}

// Test 7 — @native
{
    const tokens = lex(`@native("flow_print")`)
    console.assert(tokens[0].type === "AT",     "AT @")
    console.assert(tokens[1].type === "IDENT",  "IDENT native")
    console.assert(tokens[2].type === "LPAREN", "LPAREN")
    console.assert(tokens[3].type === "STRING", "STRING flow_print")
    console.log("✓ annotazione: @native(\"flow_print\")")
}

console.log("\nTutti i test passati")
