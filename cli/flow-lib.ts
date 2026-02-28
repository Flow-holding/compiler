// Estrae @native da flow/ e flow-os/ per costruire la mappa dinamica
// Usato in dev per avere flow/ e flow-os/ sempre aggiornati

import { join } from "node:path"
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs"

function* walkFlowFiles(dir: string, base = ""): Generator<string> {
    try {
        const fullDir = base ? join(dir, base) : dir
        const entries = readdirSync(fullDir)
        for (const e of entries) {
            const rel = base ? `${base}/${e}` : e
            const full = join(dir, rel)
            if (statSync(full).isDirectory()) {
                yield* walkFlowFiles(dir, rel)
            } else if (e.endsWith(".flow")) {
                yield rel
            }
        }
    } catch { /* ignore */ }
}

// Estrae modulo dal path: flow/io/fs.flow -> fs, flow/core/print.flow -> "" (global)
function moduleFromPath(relPath: string, isFlowOs: boolean): string {
    const parts = relPath.replace(/\\/g, "/").split("/")
    const name = parts[parts.length - 1]?.replace(/\.flow$/, "") ?? ""
    if (isFlowOs) return name === "index" ? (parts[parts.length - 2] ?? "") : name
    // flow/core/* = global, flow/io/fs.flow = fs
    if (parts.includes("core")) return ""
    return name
}

// Regex per @native("X") fn name
const NATIVE_RE = /@native\s*\(\s*"([^"]+)"\s*\)\s*fn\s+(\w+)/g

export function extractNativeMap(flowDir: string, flowOsDir?: string): Map<string, string> {
    const map = new Map<string, string>()

    for (const rel of walkFlowFiles(flowDir)) {
        const full = join(flowDir, rel)
        const content = readFileSync(full, "utf8")
        const mod = moduleFromPath(rel, false)

        for (const m of content.matchAll(NATIVE_RE)) {
            const [, symbol, fnName] = m
            const key = mod ? `${mod}.${fnName}` : fnName
            map.set(key, symbol)
        }
    }

    if (flowOsDir && existsSync(flowOsDir)) {
        for (const rel of walkFlowFiles(flowOsDir)) {
            const full = join(flowOsDir, rel)
            const content = readFileSync(full, "utf8")
            const mod = moduleFromPath(rel, true)

            for (const m of content.matchAll(NATIVE_RE)) {
                const [, symbol, fnName] = m
                const key = mod ? `${mod}.${fnName}` : fnName
                map.set(key, symbol)
            }
        }
    }

    return map
}

export function nativeMapToFile(map: Map<string, string>): string {
    return [...map.entries()].map(([k, v]) => `${k}=${v}`).join("\n")
}
