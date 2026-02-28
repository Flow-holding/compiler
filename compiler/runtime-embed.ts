// Runtime embedded — legge i file C e li espone come stringhe
// Il build process li compila dentro il binario con Bun.file()

import { join } from "node:path"
import { existsSync, mkdirSync, writeFileSync } from "node:fs"

// Percorso base del runtime — relativo a questo file
const RUNTIME_BASE = join(import.meta.dir, "../runtime")

// Legge un file runtime e lo restituisce come stringa
function readRuntime(relPath: string): string {
    const fullPath = join(RUNTIME_BASE, relPath)
    if (!existsSync(fullPath)) return `/* runtime non trovato: ${relPath} */`
    return Bun.file(fullPath).text() as unknown as string
}

// Tutti i file runtime necessari per la compilazione
const RUNTIME_FILES = {
    "memory/core.c":      readRuntime("memory/core.c"),
    "types/strings.c":    readRuntime("types/strings.c"),
    "types/arrays.c":     readRuntime("types/arrays.c"),
    "types/maps.c":       readRuntime("types/maps.c"),
    "io/print.c":         readRuntime("io/print.c"),
    "io/fs.c":            readRuntime("io/fs.c"),
    "io/dom.c":           readRuntime("io/dom.c"),
    "net/http.c":         readRuntime("net/http.c"),
    "net/websocket.c":    readRuntime("net/websocket.c"),
    "wasm/stdlib.c":      readRuntime("wasm/stdlib.c"),
    "wasm/runtime.c":     readRuntime("wasm/runtime.c"),
} as const

// Estrae il runtime nella cartella di lavoro
// Chiamato da flow init e flow run prima di compilare
export async function extractRuntime(destDir: string) {
    const runtimeDir = join(destDir, ".flow-runtime")
    if (existsSync(runtimeDir)) return runtimeDir  // già estratto

    mkdirSync(join(runtimeDir, "memory"),  { recursive: true })
    mkdirSync(join(runtimeDir, "types"),   { recursive: true })
    mkdirSync(join(runtimeDir, "io"),      { recursive: true })
    mkdirSync(join(runtimeDir, "net"),     { recursive: true })
    mkdirSync(join(runtimeDir, "wasm"),    { recursive: true })

    for (const [relPath, content] of Object.entries(RUNTIME_FILES)) {
        const resolved = await Promise.resolve(content)
        writeFileSync(join(runtimeDir, relPath), resolved)
    }

    return runtimeDir
}

// Percorso runtime — usa embedded se distribuito, locale se in dev
export async function getRuntimeDir(): Promise<string> {
    const devRuntime = join(import.meta.dir, "../runtime")
    if (existsSync(devRuntime)) return devRuntime  // sviluppo locale

    // Produzione — estrai nella home dell'utente
    const homeDir = process.env.HOME ?? process.env.USERPROFILE ?? "."
    const flowDir = join(homeDir, ".flow")
    return await extractRuntime(flowDir)
}
