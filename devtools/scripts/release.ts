// scripts/release.ts — aggiorna versione, committa e pusha il tag
// Uso: bun run scripts/release.ts 0.2.0
// GitHub Actions fa il build e crea la release automaticamente

import { join }            from "node:path"
import { writeFileSync }   from "node:fs"

const version = Bun.argv[2]
if (!version || !/^\d+\.\d+\.\d+$/.test(version)) {
    console.error("✗ Versione mancante o non valida — es: bun run scripts/release.ts 0.2.0")
    process.exit(1)
}

const root = join(import.meta.dir, "../..")

// 1. Aggiorna VERSION
writeFileSync(join(root, "VERSION"), version)
console.log(`✓ VERSION → ${version}`)

// 2. Aggiorna CURRENT_VERSION in cli/update.ts
const updatePath = join(root, "cli/update.ts")
const updateSrc  = await Bun.file(updatePath).text()
writeFileSync(updatePath, updateSrc.replace(
    /export const CURRENT_VERSION = "[^"]+"/,
    `export const CURRENT_VERSION = "${version}"`
))
console.log(`✓ cli/update.ts → ${version}`)

// 3. Commit + tag + push
function run(cmd: string[]) {
    const proc = Bun.spawnSync(cmd, { cwd: root, stdio: ["inherit", "inherit", "inherit"] })
    if (proc.exitCode !== 0) {
        console.error(`✗ Fallito: ${cmd.join(" ")}`)
        process.exit(1)
    }
}

run(["git", "add", "VERSION", "cli/update.ts"])
run(["git", "commit", "-m", `v${version}`])
run(["git", "tag", `v${version}`])
run(["git", "push"])
run(["git", "push", "--tags"])

console.log(`\n✓ Tag v${version} pushato — GitHub Actions avvia il build`)
