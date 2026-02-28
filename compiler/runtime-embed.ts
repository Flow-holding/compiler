// Runtime embed — usa il bundle pre-generato (compiler/runtime-bundle.ts)
// I file C sono inclusi come stringhe nel binario compilato

import { join }                          from "node:path"
import { existsSync, mkdirSync,
         writeFileSync, readFileSync }   from "node:fs"
import { RUNTIME_BUNDLE }               from "./runtime-bundle"

const FLOW_HOME      = join(process.env.USERPROFILE ?? process.env.HOME ?? ".", ".flow")
const RUNTIME_CACHE  = join(FLOW_HOME, "runtime")
const STAMP_FILE     = join(RUNTIME_CACHE, ".version")

// Estrae i file C nella home dell'utente (~/.flow/runtime/)
async function extractRuntime(): Promise<string> {
    if (existsSync(STAMP_FILE)) return RUNTIME_CACHE  // già estratti

    for (const [relPath, content] of Object.entries(RUNTIME_BUNDLE)) {
        const dest = join(RUNTIME_CACHE, relPath)
        mkdirSync(join(dest, ".."), { recursive: true })
        writeFileSync(dest, content)
    }

    writeFileSync(STAMP_FILE, "1")
    return RUNTIME_CACHE
}

// Forza la ri-estrazione (usato da flow update)
export async function reExtractRuntime(): Promise<string> {
    try { require("node:fs").rmSync(STAMP_FILE) } catch {}
    return await extractRuntime()
}

// Ritorna il path al runtime corretto
// - In sviluppo: usa la cartella /runtime/ locale
// - In produzione (binario): estrae in ~/.flow/runtime/
export async function getRuntimeDir(): Promise<string> {
    // Sviluppo locale — la cartella runtime è accanto al compiler
    const devPath = join(import.meta.dir, "../runtime")
    if (existsSync(devPath) && existsSync(join(devPath, "io/print.c"))) {
        return devPath
    }

    // Produzione — estrai il bundle embedded
    return await extractRuntime()
}
