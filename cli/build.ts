import { join, dirname, basename } from "node:path"
import { existsSync, mkdirSync, copyFileSync } from "node:fs"
import { lex }          from "../compiler/lexer/lexer"
import { parse }        from "../compiler/parser/parser"
import { typecheck }    from "../compiler/typechecker/typechecker"
import { codegen }      from "../compiler/codegen/codegen"
import { generateHTML } from "../compiler/codegen/htmlgen"
import { generateJS }   from "../compiler/codegen/jsgen"
import { getRuntimeDir } from "../compiler/runtime-embed"

export type BuildOptions = {
    watch?:   boolean
    run?:     boolean
    prod?:    boolean
}

export async function build(options: BuildOptions = {}) {
    const dec = new TextDecoder()

    // ── Leggi config ─────────────────────────────────────
    const configPath = join(process.cwd(), "flow.config.json")
    if (!existsSync(configPath)) {
        console.error("✗ flow.config.json non trovato — sei nella cartella del progetto?")
        process.exit(1)
    }

    const config   = await Bun.file(configPath).json()
    const entry    = join(process.cwd(), config.entry ?? "src/main.flow")
    const outDir   = join(process.cwd(), config.out   ?? "out")
    const name     = basename(entry, ".flow")

    if (!existsSync(entry)) {
        console.error(`✗ Entry point non trovato: ${entry}`)
        process.exit(1)
    }

    if (!existsSync(outDir)) mkdirSync(outDir, { recursive: true })

    const start      = Date.now()
    const source     = await Bun.file(entry).text()
    const runtimeDir = await getRuntimeDir()

    // ── Pipeline ─────────────────────────────────────────
    const tokens = lex(source)
    const ast    = parse(tokens)
    const errors = typecheck(ast)

    if (errors.length > 0) {
        console.error(`\n✗ ${errors.length} errore/i:\n`)
        for (const e of errors) console.error(`  • ${e.message}`)
        process.exit(1)
    }

    const hasClient = ast.body.some(n =>
        n.kind === "ComponentDecl" && n.annotation?.name === "client")

    // ── C nativo ─────────────────────────────────────────
    const cPath   = join(outDir, name + ".c")
    const exePath = join(outDir, name + ".exe")
    await Bun.write(cPath, codegen(ast, "native"))

    const native = Bun.spawnSync(["clang", cPath, "-o", exePath,
        ...(options.prod ? ["-O2"] : ["-g"])])

    if (native.exitCode !== 0) {
        console.error("✗ Errore compilazione:")
        console.error(dec.decode(native.stderr).split("\n").slice(0, 8).join("\n"))
        process.exit(1)
    }

    // ── WASM ─────────────────────────────────────────────
    const cWasmPath = join(outDir, name + ".wasm.c")
    const wasmPath  = join(outDir, "app.wasm")
    await Bun.write(cWasmPath, codegen(ast, "wasm"))

    const wasm = Bun.spawnSync([
        "clang", "--target=wasm32-unknown-unknown",
        "-nostdlib", "-Wl,--no-entry", "-Wl,--allow-undefined",
        "-Wl,--export-dynamic",
        ...(options.prod ? ["-O2", "-flto"] : ["-O1"]),
        cWasmPath, "-o", wasmPath
    ])

    const wasmOk = wasm.exitCode === 0

    // ── HTML / CSS / JS (client) ──────────────────────────
    if (hasClient) {
        const { html, css, assets } = generateHTML(ast)
        const js = generateJS(ast)

        await Bun.write(join(outDir, "index.html"), html)
        await Bun.write(join(outDir, "style.css"),  css)
        await Bun.write(join(outDir, "app.js"),      js)

        for (const asset of assets) {
            const src  = join(dirname(entry), asset)
            const dest = join(outDir, basename(asset))
            if (existsSync(src)) copyFileSync(src, dest)
        }
    }

    const elapsed = Date.now() - start
    console.log(`\n✓ Build completata in ${elapsed}ms\n`)
    console.log(`  .exe    ${exePath}`)
    if (wasmOk) console.log(`  .wasm   ${wasmPath}`)
    if (hasClient) {
        console.log(`  html    ${join(outDir, "index.html")}`)
        console.log(`  css     ${join(outDir, "style.css")}`)
        console.log(`  js      ${join(outDir, "app.js")}`)
    }
    console.log()

    // ── Esegui se --run ───────────────────────────────────
    if (options.run && existsSync(exePath)) {
        console.log("── output ──────────────────────────")
        const run = Bun.spawnSync([exePath])
        process.stdout.write(dec.decode(run.stdout))
        if (run.stderr.length) process.stderr.write(dec.decode(run.stderr))
        console.log("────────────────────────────────────\n")
    }

    return { exePath, wasmPath, outDir, hasClient }
}
