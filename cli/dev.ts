import { join } from "node:path"
import { existsSync } from "node:fs"
import { build } from "./build"

export async function cmdDev() {
    const configPath = join(process.cwd(), "flow.config.json")
    if (!existsSync(configPath)) {
        console.error("✗ flow.config.json non trovato")
        process.exit(1)
    }

    const config = await Bun.file(configPath).json()
    const entry  = join(process.cwd(), config.entry ?? "src/main.flow")
    const srcDir = join(process.cwd(), "src")

    console.log("┌─────────────────────────────────┐")
    console.log("│  Flow dev server                │")
    console.log("│  http://localhost:3000          │")
    console.log("└─────────────────────────────────┘\n")

    // Prima build
    await build({ run: false })

    // Avvia server HTTP minimale per servire out/
    const outDir = join(process.cwd(), config.out ?? "out")
    const server = Bun.serve({
        port: 3000,
        async fetch(req) {
            const url  = new URL(req.url)
            let   path = url.pathname === "/" ? "/index.html" : url.pathname
            const file = Bun.file(join(outDir, path))

            if (await file.exists()) {
                const ext  = path.split(".").pop() ?? ""
                const mime: Record<string, string> = {
                    html: "text/html",
                    css:  "text/css",
                    js:   "application/javascript",
                    wasm: "application/wasm",
                    png:  "image/png",
                    jpg:  "image/jpeg",
                    svg:  "image/svg+xml",
                }
                return new Response(file, {
                    headers: { "Content-Type": mime[ext] ?? "text/plain" }
                })
            }
            return new Response("404", { status: 404 })
        }
    })

    console.log(`✓ Server avviato su http://localhost:${server.port}\n`)

    // Watcher — guarda solo i file .flow nella cartella src/
    const srcDir2 = join(process.cwd(), "src")
    console.log(`  watching ${srcDir2}\n`)

    let rebuilding = false
    let lastMods: Record<string, number> = {}

    // Inizializza lastMods con i mtime attuali così il primo check non triggera rebuild
    const initGlob = new Bun.Glob("**/*.flow")
    for await (const file of initGlob.scan(srcDir2)) {
        const full = join(srcDir2, file)
        const stat = await Bun.file(full).stat?.()
        const mod  = stat?.mtime instanceof Date ? stat.mtime.getTime() : Number(stat?.mtime ?? 0)
        lastMods[full] = mod
    }

    setInterval(async () => {
        if (rebuilding) return
        try {
            const glob = new Bun.Glob("**/*.flow")
            let changed = false
            for await (const file of glob.scan(srcDir2)) {
                const full  = join(srcDir2, file)
                const stat  = await Bun.file(full).stat?.()
                // Converti mtime in numero (ms) per confronto stabile
                const mod   = stat?.mtime instanceof Date
                    ? stat.mtime.getTime()
                    : Number(stat?.mtime ?? 0)
                if (mod !== (lastMods[full] ?? 0)) {
                    lastMods[full] = mod
                    changed = true
                }
            }
            if (changed) {
                rebuilding = true
                process.stdout.write("  rebuild... ")
                const t = Date.now()
                await build({ run: false })
                console.log(`  ✓ rebuild in ${Date.now() - t}ms`)
                rebuilding = false
            }
        } catch { rebuilding = false }
    }, 400)
}
