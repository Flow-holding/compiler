// scripts/release.ts — compila flow.exe per tutte le piattaforme
// Uso: bun run scripts/release.ts [versione]
// Es:  bun run scripts/release.ts 0.2.0

import { join } from "node:path"
import { writeFileSync, existsSync, mkdirSync } from "node:fs"

const version = Bun.argv[2] ?? "0.1.0"
const root    = join(import.meta.dir, "../..")
const distDir = join(root, "dist")

if (!existsSync(distDir)) mkdirSync(distDir, { recursive: true })

// Aggiorna la versione nel sorgente
const updatePath = join(root, "cli/update.ts")
let updateSrc = await Bun.file(updatePath).text()
updateSrc = updateSrc.replace(
    /export const CURRENT_VERSION = "[^"]+"/,
    `export const CURRENT_VERSION = "${version}"`
)
writeFileSync(updatePath, updateSrc)
console.log(`\x1b[32m✓\x1b[0m Versione aggiornata a ${version}`)

// Aggiorna VERSION file
writeFileSync(join(root, "VERSION"), version)

type Target = {
    target: string
    outFile: string
    display: string
}

const TARGETS: Target[] = [
    { target: "bun-windows-x64",  outFile: "flow-windows-x64.exe", display: "Windows x64"  },
    { target: "bun-linux-x64",    outFile: "flow-linux-x64",        display: "Linux x64"    },
    { target: "bun-linux-arm64",  outFile: "flow-linux-arm64",      display: "Linux arm64"  },
    { target: "bun-darwin-x64",   outFile: "flow-macos-x64",        display: "macOS x64"    },
    { target: "bun-darwin-arm64", outFile: "flow-macos-arm64",      display: "macOS arm64"  },
]

console.log(`\nBuild \x1b[36mflow v${version}\x1b[0m per ${TARGETS.length} piattaforme...\n`)

let errors = 0
for (const t of TARGETS) {
    const outPath = join(distDir, t.outFile)
    process.stdout.write(`  ${t.display.padEnd(16)} → `)

    const proc = Bun.spawnSync([
        "bun", "build",
        join(root, "cli/main.ts"),
        "--compile",
        "--target", t.target,
        "--outfile", outPath,
    ], { cwd: root })

    if (proc.exitCode === 0) {
        console.log(`\x1b[32m${t.outFile}\x1b[0m`)
    } else {
        console.log(`\x1b[31mFAILED\x1b[0m`)
        console.error(proc.stderr?.toString())
        errors++
    }
}

console.log("")
if (errors === 0) {
    console.log(`\x1b[32m✓ Build completato\x1b[0m — file in dist/`)
    console.log(`\nPer pubblicare su GitHub:`)
    console.log(`  git tag v${version} && git push --tags`)
    console.log(`  gh release create v${version} dist/* --title "flow v${version}"`)
} else {
    console.log(`\x1b[31m✗ ${errors} build falliti\x1b[0m`)
    process.exit(1)
}
