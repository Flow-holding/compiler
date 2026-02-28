// scripts/bundle-runtime.ts
// Genera compiler/runtime-bundle.ts con i file C del runtime come stringhe
// Va eseguito PRIMA di bun build --compile

import { join, relative } from "node:path"
import { readdirSync, readFileSync, writeFileSync, statSync } from "node:fs"

const ROOT         = join(import.meta.dir, "..")
const RUNTIME_DIR  = join(ROOT, "runtime")
const OUT_FILE     = join(ROOT, "compiler", "runtime-bundle.ts")

function collectFiles(dir: string, base: string = dir): Record<string, string> {
    const result: Record<string, string> = {}
    for (const entry of readdirSync(dir)) {
        const full = join(dir, entry)
        if (statSync(full).isDirectory()) {
            Object.assign(result, collectFiles(full, base))
        } else if (entry.endsWith(".c") || entry.endsWith(".h")) {
            const rel = relative(base, full).replace(/\\/g, "/")
            result[rel] = readFileSync(full, "utf8")
        }
    }
    return result
}

const files = collectFiles(RUNTIME_DIR)
const count = Object.keys(files).length

let out = `// AUTO-GENERATED — non modificare manualmente
// Generato da scripts/bundle-runtime.ts
// Contiene ${count} file C del runtime Flow

export const RUNTIME_BUNDLE: Record<string, string> = {\n`

for (const [path, content] of Object.entries(files)) {
    const escaped = content
        .replace(/\\/g, "\\\\")
        .replace(/`/g, "\\`")
        .replace(/\$\{/g, "\\${")
    out += `    ${JSON.stringify(path)}: \`${escaped}\`,\n\n`
}

out += `}\n`

writeFileSync(OUT_FILE, out)
console.log(`✓ runtime-bundle.ts generato — ${count} file C`)
