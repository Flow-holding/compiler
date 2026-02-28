import { join, basename }   from "node:path"
import { existsSync, mkdirSync } from "node:fs"
import { runFlowc }         from "./flowc"

export type BuildOptions = {
    watch?:   boolean
    run?:     boolean
    prod?:    boolean
}

export async function build(options: BuildOptions = {}) {
    // ── Leggi config ─────────────────────────────────────────────
    const configPath = join(process.cwd(), "flow.config.json")
    if (!existsSync(configPath)) {
        console.error("✗ flow.config.json non trovato — sei nella cartella del progetto?")
        process.exit(1)
    }

    const config   = await Bun.file(configPath).json()
    const entry    = join(process.cwd(), config.entry ?? "src/main.flow")
    const outDir   = join(process.cwd(), config.out   ?? "out")

    if (!existsSync(entry)) {
        console.error(`✗ Entry point non trovato: ${entry}`)
        process.exit(1)
    }

    if (!existsSync(outDir)) mkdirSync(outDir, { recursive: true })

    // ── Chiama flowc (compiler C) ─────────────────────────────────
    const ok = await runFlowc(entry, outDir, {
        run:  options.run,
        prod: options.prod,
    })

    if (!ok) process.exit(1)

    return { outDir }
}
