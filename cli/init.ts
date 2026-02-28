import { join } from "node:path"
import {
    existsSync, mkdirSync, writeFileSync,
    readFileSync, readdirSync, statSync
} from "node:fs"

export async function cmdInit(nome?: string) {
    const projectName = nome ?? "my-flow-app"
    const projectDir = join(process.cwd(), projectName)

    if (existsSync(projectDir)) {
        console.error(`✗ cartella "${projectName}" esiste già`)
        process.exit(1)
    }

    const templateDir = join(import.meta.dir, "../template")
    if (!existsSync(templateDir)) {
        console.error(`✗ template non trovato: ${templateDir}`)
        process.exit(1)
    }

    copyTemplate(templateDir, projectDir, projectName)

    console.log(`\n✓ Progetto "${projectName}" creato\n`)
    console.log(`  cd ${projectName}`)
    console.log(`  flow dev\n`)
}

function copyTemplate(src: string, dest: string, name: string) {
    mkdirSync(dest, { recursive: true })

    for (const entry of readdirSync(src)) {
        const srcPath = join(src, entry)
        const destPath = join(dest, entry)

        if (statSync(srcPath).isDirectory()) {
            copyTemplate(srcPath, destPath, name)
        } else if (entry !== ".gitkeep") {
            const content = readFileSync(srcPath, "utf8").replaceAll("{{name}}", name)
            writeFileSync(destPath, content)
        }
    }
}
