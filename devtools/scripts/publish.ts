// scripts/publish.ts
// Bumpa la versione patch, committa tutto, taga e pusha per triggerare la GitHub Action
// Uso: bun run publish

import { join } from "node:path"
import { readFileSync, writeFileSync } from "node:fs"

const ROOT = join(import.meta.dir, "../..")

// Legge versione attuale
const versionFile = join(ROOT, "VERSION")
const current = readFileSync(versionFile, "utf8").trim()
const parts = current.split(".").map(Number)
parts[2]++
const next = parts.join(".")

// Aggiorna VERSION
writeFileSync(versionFile, next)
console.log(`\x1b[32m✓\x1b[0m Versione: ${current} → ${next}`)

// Aggiorna cli/update.ts
const updatePath = join(ROOT, "cli/update.ts")
let updateSrc = readFileSync(updatePath, "utf8")
updateSrc = updateSrc.replace(
    /export const CURRENT_VERSION = "[^"]+"/,
    `export const CURRENT_VERSION = "${next}"`
)
writeFileSync(updatePath, updateSrc)
console.log(`\x1b[32m✓\x1b[0m cli/update.ts aggiornato`)

// Aggiorna devtools/package.json
const pkgPath = join(ROOT, "devtools/package.json")
let pkg = JSON.parse(readFileSync(pkgPath, "utf8"))
pkg.version = next
writeFileSync(pkgPath, JSON.stringify(pkg, null, 4) + "\n")
console.log(`\x1b[32m✓\x1b[0m devtools/package.json aggiornato`)

// Git: add, commit, tag, push
function run(cmd: string, args: string[]) {
    const proc = Bun.spawnSync([cmd, ...args], { cwd: ROOT, stdout: "inherit", stderr: "inherit" })
    if (proc.exitCode !== 0) {
        console.error(`\x1b[31m✗\x1b[0m Comando fallito: ${cmd} ${args.join(" ")}`)
        process.exit(1)
    }
}

run("git", ["add", "-A"])
run("git", ["commit", "-m", `chore: release v${next}`])
run("git", ["tag", `v${next}`])
run("git", ["push", "origin", "main"])
run("git", ["push", "origin", `v${next}`])

console.log(`\n\x1b[32m✓ Pubblicato v${next}\x1b[0m — GitHub Action avviata`)
console.log(`  https://github.com/Flow-holding/compiler/actions`)
