import { lex }          from "./lexer/lexer"
import { parse }        from "./parser/parser"
import { typecheck }    from "./typechecker/typechecker"
import { codegen, CodegenTarget } from "./codegen/codegen"
import { generateHTML } from "./codegen/htmlgen"
import { generateJS }   from "./codegen/jsgen"
import { join, dirname, basename } from "node:path"
import { existsSync, mkdirSync, copyFileSync } from "node:fs"

// ── Leggi argomenti ──────────────────────────────────────

const args      = process.argv.slice(2)
const shouldRun = args.includes("--run")

// Trova il file .flow da compilare
// Priorità: argomento diretto > flow.config.json > src/main.flow
let inputPath: string
let outDir:    string

const configPath = join(process.cwd(), "flow.config.json")

if (args[0] && !args[0].startsWith("--")) {
    inputPath = args[0]
    outDir    = join(dirname(inputPath), "out")
} else if (existsSync(configPath)) {
    const config = await Bun.file(configPath).json()
    inputPath = join(process.cwd(), config.entry ?? "src/main.flow")
    outDir    = join(process.cwd(), config.out  ?? "out")
} else {
    const fallback = join(process.cwd(), "src", "main.flow")
    if (existsSync(fallback)) {
        inputPath = fallback
        outDir    = join(process.cwd(), "out")
    } else {
        console.error("uso: flowc <file.flow>  oppure crea flow.config.json")
        process.exit(1)
    }
}

if (!existsSync(inputPath)) {
    console.error(`flowc: file non trovato "${inputPath}"`)
    process.exit(1)
}

const source  = await Bun.file(inputPath).text()
const name    = basename(inputPath, ".flow")

if (!existsSync(outDir)) mkdirSync(outDir, { recursive: true })

// ── Pipeline ──────────────────────────────────────────────

const tokens = lex(source)
const ast    = parse(tokens)
const errors = typecheck(ast)

if (errors.length > 0) {
    console.error(`\nflowc: ${errors.length} errore/i in "${inputPath}":\n`)
    for (const e of errors) console.error(`  ✗ ${e.message}`)
    process.exit(1)
}

const hasClient = ast.body.some(n =>
    n.kind === "ComponentDecl" && n.annotation?.name === "client")
const hasServer = ast.body.some(n =>
    n.kind === "FnDecl" && n.annotation?.name !== "native")

const dec = new TextDecoder()

// ── 1. Genera C nativo + compila .exe ────────────────────

const cPath   = join(outDir, name + ".c")
const exePath = join(outDir, name + ".exe")

const cCode = codegen(ast, "native")
await Bun.write(cPath, cCode)
console.log(`✓ C nativo        → ${cPath}`)

const clangNative = Bun.spawnSync(["clang", cPath, "-o", exePath])
if (clangNative.exitCode !== 0) {
    console.error("flowc: errore compilazione nativa:")
    console.error(dec.decode(clangNative.stderr))
} else {
    console.log(`✓ .exe compilato  → ${exePath}`)
}

// ── 2. Genera C WASM + compila .wasm ─────────────────────

const cWasmPath = join(outDir, name + ".wasm.c")
const wasmPath  = join(outDir, "app.wasm")

const cWasmCode = codegen(ast, "wasm")
await Bun.write(cWasmPath, cWasmCode)

const clangWasm = Bun.spawnSync([
    "clang",
    "--target=wasm32-unknown-unknown",
    "-nostdlib",
    "-Wl,--no-entry",
    "-Wl,--allow-undefined",
    "-Wl,--export-dynamic",
    "-O2",
    cWasmPath,
    "-o", wasmPath
])

if (clangWasm.exitCode !== 0) {
    console.warn("⚠ WASM:")
    console.warn(dec.decode(clangWasm.stderr).split("\n").slice(0, 5).join("\n"))
} else {
    console.log(`✓ .wasm compilato → ${wasmPath}`)
}

// ── 4. Genera HTML + CSS (client) ─────────────────────────

if (hasClient) {
    const { html, css, assets } = generateHTML(ast)
    const jsCode = generateJS(ast)

    await Bun.write(join(outDir, "index.html"), html)
    await Bun.write(join(outDir, "style.css"),  css)
    await Bun.write(join(outDir, "app.js"),     jsCode)

    console.log(`✓ HTML generato   → ${join(outDir, "index.html")}`)
    console.log(`✓ CSS generato    → ${join(outDir, "style.css")}`)
    console.log(`✓ JS generato     → ${join(outDir, "app.js")}`)

    for (const asset of assets) {
        const src  = join(dirname(inputPath), asset)
        const dest = join(outDir, basename(asset))
        if (existsSync(src)) {
            copyFileSync(src, dest)
            console.log(`✓ asset copiato   → ${dest}`)
        } else {
            console.warn(`⚠ asset non trovato: ${asset}`)
        }
    }
}

// ── 5. Esegui se --run ────────────────────────────────────

if (shouldRun && existsSync(exePath)) {
    console.log(`\n── output ──────────────────────────`)
    const run = Bun.spawnSync([exePath])
    process.stdout.write(new TextDecoder().decode(run.stdout))
    if (run.stderr.length) process.stderr.write(new TextDecoder().decode(run.stderr))
}
