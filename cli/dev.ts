import { join } from "node:path"
import { existsSync } from "node:fs"
import { build } from "./build"

const HMR_CLIENT = `
(function() {
  const ws = new WebSocket('ws://' + location.host + '/__hmr');
  ws.onmessage = async (e) => {
    if (e.data !== 'update') return;
    const t = Date.now();
    try {
      const [htmlRes, cssRes] = await Promise.all([
        fetch('/index.html?t=' + t),
        fetch('/style.css?t=' + t)
      ]);
      const html = await htmlRes.text();
      const parser = new DOMParser();
      const doc = parser.parseFromString(html, 'text/html');
      const newRoot = doc.getElementById('root');
      const root = document.getElementById('root');
      if (newRoot && root) root.innerHTML = newRoot.innerHTML;
      const link = document.querySelector('link[href*="style.css"]');
      if (link) link.href = '/style.css?t=' + t;
      let wasmMem;
      const imports = {
        env: {
          flow_print_str: (ptr) => {
            const view = new Uint8Array(wasmMem.buffer);
            let end = ptr;
            while (view[end]) end++;
            console.log(new TextDecoder().decode(view.subarray(ptr, end)));
          },
          flow_eprint_str: (ptr) => {
            const view = new Uint8Array(wasmMem.buffer);
            let end = ptr;
            while (view[end]) end++;
            console.error(new TextDecoder().decode(view.subarray(ptr, end)));
          },
        }
      };
      const { instance } = await WebAssembly.instantiateStreaming(fetch('app.wasm?t=' + t), imports);
      wasmMem = instance.exports.memory;
      window._flow = instance.exports;
      if (instance.exports.main) instance.exports.main();
    } catch (err) { console.error('[HMR]', err); }
  };
})();
`

export async function cmdDev() {
    const configPath = join(process.cwd(), "flow.config.json")
    if (!existsSync(configPath)) {
        console.error("✗ flow.config.json non trovato")
        process.exit(1)
    }

    const config = await Bun.file(configPath).json()
    const entry  = join(process.cwd(), config.entry ?? "src/main.flow")
    const srcDir = join(process.cwd(), "src")
    const flowDir = config.flow ? join(process.cwd(), config.flow) : null
    const flowOsDir = config.flowOs ? join(process.cwd(), config.flowOs) : null

    console.log("┌─────────────────────────────────┐")
    console.log("│  Flow dev server                │")
    console.log("│  http://localhost:3000          │")
    console.log("│  HMR via WebSocket (no refresh) │")
    console.log("└─────────────────────────────────┘\n")

    // Prima build
    await build({ run: false })

    const outDir = join(process.cwd(), config.out ?? "out")
    const hmrClients = new Set<import("bun").ServerWebSocket<unknown>>()

    const server = Bun.serve({
        port: 3000,
        async fetch(req, srv) {
            const url  = new URL(req.url)
            let   path = url.pathname === "/" ? "/index.html" : url.pathname

            if (path === "/__hmr") {
                if (srv.upgrade(req)) return undefined
                return new Response("Upgrade failed", { status: 500 })
            }

            if (path === "/__hmr-client.js") {
                return new Response(HMR_CLIENT, {
                    headers: { "Content-Type": "application/javascript" }
                })
            }

            const filePath = join(outDir, path.replace(/\?.*/, "").replace(/^\//, ""))
            const file = Bun.file(filePath)

            if (await file.exists()) {
                let body: string | Blob = file
                const ext = path.split(".").pop()?.split("?")[0] ?? ""
                const mime: Record<string, string> = {
                    html: "text/html",
                    css:  "text/css",
                    js:   "application/javascript",
                    wasm: "application/wasm",
                    png:  "image/png",
                    jpg:  "image/jpeg",
                    svg:  "image/svg+xml",
                }
                if (ext === "html") {
                    body = await file.text()
                    body = body.replace(
                        '<script src="app.js"></script>',
                        '<script src="/__hmr-client.js"></script>\n  <script src="app.js"></script>'
                    )
                }
                return new Response(body, {
                    headers: { "Content-Type": mime[ext] ?? "text/plain" }
                })
            }
            return new Response("404", { status: 404 })
        },
        websocket: {
            open(ws) { hmrClients.add(ws) },
            close(ws) { hmrClients.delete(ws) },
            message() {},
        },
    })

    const broadcastHmr = () => {
        for (const ws of hmrClients) ws.send("update")
    }

    console.log(`✓ Server avviato su http://localhost:${server.port}\n`)

    // Watcher — src/, flow/, flow-os/
    const watchDirs = [srcDir, flowDir, flowOsDir].filter(Boolean) as string[]
    console.log(`  watching ${watchDirs.join(", ")}\n`)

    let rebuilding = false
    let lastMods: Record<string, number> = {}

    async function getMtime(path: string): Promise<number> {
        try {
            const stat = await Bun.file(path).stat?.()
            return stat?.mtime instanceof Date ? stat.mtime.getTime() : Number(stat?.mtime ?? 0)
        } catch { return 0 }
    }

    const initGlob = new Bun.Glob("**/*.flow")
    for (const dir of watchDirs) {
        if (!existsSync(dir)) continue
        for await (const file of initGlob.scan(dir)) {
            const full = join(dir, file)
            lastMods[full] = await getMtime(full)
        }
    }

    setInterval(async () => {
        if (rebuilding) return
        try {
            let changed = false
            for (const dir of watchDirs) {
                if (!existsSync(dir)) continue
                const glob = new Bun.Glob("**/*.flow")
                for await (const file of glob.scan(dir)) {
                    const full = join(dir, file)
                    const mod = await getMtime(full)
                    if (mod !== (lastMods[full] ?? 0)) {
                        lastMods[full] = mod
                        changed = true
                    }
                }
            }
            if (changed) {
                rebuilding = true
                process.stdout.write("  rebuild... ")
                const t = Date.now()
                await build({ run: false })
                broadcastHmr()
                console.log(`  ✓ rebuild in ${Date.now() - t}ms (HMR pushed)`)
                rebuilding = false
            }
        } catch { rebuilding = false }
    }, 400)
}
