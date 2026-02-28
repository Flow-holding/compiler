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

    // Watcher — ricompila quando cambiano i file .flow
    const watcher = Bun.file(entry)
    let   lastMod = (await watcher.stat?.()).mtime ?? 0

    console.log(`  watching ${entry}\n`)

    setInterval(async () => {
        try {
            const stat = await Bun.file(entry).stat?.()
            const mod  = stat?.mtime ?? 0
            if (mod !== lastMod) {
                lastMod = mod
                process.stdout.write("  rebuild... ")
                const t = Date.now()
                await build({ run: false })
                console.log(`  ✓ rebuild in ${Date.now() - t}ms`)
            }
        } catch {}
    }, 300)
}
