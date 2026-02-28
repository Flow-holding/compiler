import { join }             from "node:path"
import { existsSync, mkdirSync, writeFileSync } from "node:fs"
import { runFlowc }         from "./flowc"
import { extractNativeMap, nativeMapToFile } from "./flow-lib"

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
    const runtime  = config.runtime ? join(process.cwd(), config.runtime) : undefined
    const flowDir  = config.flow ? join(process.cwd(), config.flow) : undefined
    const flowOsDir = config.flowOs ? join(process.cwd(), config.flowOs) : undefined

    if (!existsSync(entry)) {
        console.error(`✗ Entry point non trovato: ${entry}`)
        process.exit(1)
    }

    if (!existsSync(outDir)) mkdirSync(outDir, { recursive: true })

    // ── Native map da flow/ e flow-os/ (sempre aggiornato) ─────────
    let nativeMapPath: string | undefined
    if (flowDir && existsSync(flowDir)) {
        const map = extractNativeMap(flowDir, flowOsDir)
        nativeMapPath = join(outDir, ".flow-native-map")
        writeFileSync(nativeMapPath, nativeMapToFile(map))
    }

    // ── Chiama flowc (compiler C) ─────────────────────────────────
    const ok = await runFlowc(entry, outDir, {
        run:         options.run,
        prod:        options.prod,
        runtime:     runtime,
        nativeMap:   nativeMapPath,
    })

    if (!ok) process.exit(1)

    return { outDir }
}
