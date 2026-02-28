// scripts/bundle-compiler.ts
// Genera compiler-ts-unused/compiler-bundle.ts con i sorgenti C del compiler
// Va eseguito PRIMA di bun build --compile

import { join, relative } from "node:path"
import { readdirSync, readFileSync, writeFileSync, statSync } from "node:fs"

const DEVTOOLS_ROOT = join(import.meta.dir, "..")
const PROJECT_ROOT  = join(import.meta.dir, "../..")
const COMPILER_DIR  = join(PROJECT_ROOT, "compiler")
const OUT_FILE      = join(DEVTOOLS_ROOT, "compiler-ts-unused", "compiler-bundle.ts")

function collectC(dir: string, base: string = dir): Record<string, string> {
    const result: Record<string, string> = {}
    for (const entry of readdirSync(dir)) {
        const full = join(dir, entry)
        if (statSync(full).isDirectory()) {
            // Salta cartelle non rilevanti
            if (["flowc.ilk", "flowc.pdb"].includes(entry)) continue
            Object.assign(result, collectC(full, base))
        } else if (entry.endsWith(".c") || entry.endsWith(".h")) {
            const rel = relative(base, full).replace(/\\/g, "/")
            result[rel] = readFileSync(full, "utf8")
        }
    }
    return result
}

const files = collectC(COMPILER_DIR)
const count = Object.keys(files).length

let out = `// AUTO-GENERATED — non modificare manualmente
// Generato da scripts/bundle-compiler.ts
// Contiene ${count} file C del compiler Flow

export const COMPILER_BUNDLE: Record<string, string> = {\n`

for (const [path, content] of Object.entries(files)) {
    const escaped = content
        .replace(/\\/g, "\\\\")
        .replace(/`/g, "\\`")
        .replace(/\$\{/g, "\\${")
    out += `    ${JSON.stringify(path)}: \`${escaped}\`,\n\n`
}

out += `}\n`

writeFileSync(OUT_FILE, out)
console.log(`✓ compiler-bundle.ts generato — ${count} file C`)
