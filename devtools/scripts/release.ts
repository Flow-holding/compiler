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

// 1. Aggiorna cli/VERSION
writeFileSync(join(root, "cli/VERSION"), version)
console.log(`✓ cli/VERSION → ${version}`)

// 2. Aggiorna FLOW_VERSION in cli/cli.h
const cliHPath = join(root, "cli/cli.h")
const cliHSrc  = await Bun.file(cliHPath).text()
writeFileSync(cliHPath, cliHSrc.replace(
    /#define FLOW_VERSION "[^"]+"/,
    `#define FLOW_VERSION "${version}"`
))
console.log(`✓ cli/cli.h → ${version}`)

// 3. Commit + tag + push
function run(cmd: string[]) {
    const proc = Bun.spawnSync(cmd, { cwd: root, stdio: ["inherit", "inherit", "inherit"] })
    if (proc.exitCode !== 0) {
        console.error(`✗ Fallito: ${cmd.join(" ")}`)
        process.exit(1)
    }
}

run(["git", "add", "cli/VERSION", "cli/cli.h"])
run(["git", "commit", "-m", `v${version}`])
run(["git", "tag", `v${version}`])
run(["git", "push"])
run(["git", "push", "--tags"])

console.log(`\n✓ Tag v${version} pushato — GitHub Actions avvia il build`)
