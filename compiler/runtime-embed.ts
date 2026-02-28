// Runtime embedded — i file C del runtime sono inclusi come stringhe
// Bun li risolve staticamente al momento del build --compile

import { join } from "node:path"
import { existsSync, mkdirSync, writeFileSync } from "node:fs"

// Bun include questi file nel binario compilato tramite import statico
// I path sono relativi a questo file e vengono risolti a compile-time
const RUNTIME_FILES: Record<string, string> = {
    "memory/core.c":   await Bun.file(new URL("../runtime/memory/core.c",   import.meta.url)).text().catch(() => ""),
    "types/strings.c": await Bun.file(new URL("../runtime/types/strings.c", import.meta.url)).text().catch(() => ""),
    "types/arrays.c":  await Bun.file(new URL("../runtime/types/arrays.c",  import.meta.url)).text().catch(() => ""),
    "types/maps.c":    await Bun.file(new URL("../runtime/types/maps.c",    import.meta.url)).text().catch(() => ""),
    "io/print.c":      await Bun.file(new URL("../runtime/io/print.c",      import.meta.url)).text().catch(() => ""),
    "io/fs.c":         await Bun.file(new URL("../runtime/io/fs.c",         import.meta.url)).text().catch(() => ""),
    "io/dom.c":        await Bun.file(new URL("../runtime/io/dom.c",        import.meta.url)).text().catch(() => ""),
    "net/http.c":      await Bun.file(new URL("../runtime/net/http.c",      import.meta.url)).text().catch(() => ""),
    "net/websocket.c": await Bun.file(new URL("../runtime/net/websocket.c", import.meta.url)).text().catch(() => ""),
    "wasm/stdlib.c":   await Bun.file(new URL("../runtime/wasm/stdlib.c",   import.meta.url)).text().catch(() => ""),
    "wasm/runtime.c":  await Bun.file(new URL("../runtime/wasm/runtime.c",  import.meta.url)).text().catch(() => ""),
}

// Cartella dove vengono estratti i file C — ~/.flow/runtime/
const FLOW_HOME = join(process.env.USERPROFILE ?? process.env.HOME ?? ".", ".flow")
const RUNTIME_CACHE = join(FLOW_HOME, "runtime")

// Estrae i file runtime nella home dell'utente
async function extractRuntime(): Promise<string> {
    const stamp = join(RUNTIME_CACHE, ".version")

    // Se già estratti e aggiornati, non fare niente
    if (existsSync(stamp)) return RUNTIME_CACHE

    mkdirSync(join(RUNTIME_CACHE, "memory"), { recursive: true })
    mkdirSync(join(RUNTIME_CACHE, "types"),  { recursive: true })
    mkdirSync(join(RUNTIME_CACHE, "io"),     { recursive: true })
    mkdirSync(join(RUNTIME_CACHE, "net"),    { recursive: true })
    mkdirSync(join(RUNTIME_CACHE, "wasm"),   { recursive: true })

    for (const [relPath, content] of Object.entries(RUNTIME_FILES)) {
        if (content) writeFileSync(join(RUNTIME_CACHE, relPath), content)
    }

    writeFileSync(stamp, "1")
    return RUNTIME_CACHE
}

// Ritorna il path al runtime — locale in dev, estratto in produzione
export async function getRuntimeDir(): Promise<string> {
    // In sviluppo la cartella runtime è accanto al sorgente
    const devPath = join(new URL("../runtime", import.meta.url).pathname.replace(/^\/([A-Z]:)/, "$1"))
    if (existsSync(devPath)) return devPath

    // In produzione (binario compilato) — estrai i file embedded
    return await extractRuntime()
}
