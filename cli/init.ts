import { join } from "node:path"
import { existsSync, mkdirSync, writeFileSync } from "node:fs"

export async function cmdInit(nome?: string) {
    const projectName = nome ?? "my-flow-app"
    const projectDir  = join(process.cwd(), projectName)

    if (existsSync(projectDir)) {
        console.error(`✗ cartella "${projectName}" esiste già`)
        process.exit(1)
    }

    // Crea struttura
    mkdirSync(join(projectDir, "src"),    { recursive: true })
    mkdirSync(join(projectDir, "assets"), { recursive: true })

    // flow.config.json
    writeFileSync(join(projectDir, "flow.config.json"), JSON.stringify({
        name:    projectName,
        version: "0.1.0",
        entry:   "src/main.flow",
        target:  ["web"],
        out:     "out"
    }, null, 4))

    // src/main.flow — template base
    writeFileSync(join(projectDir, "src", "main.flow"), `// ${projectName}

fn main() {
    print("Ciao da ${projectName}")
}

@client
component App() {
    return Column {
        style: { padding: 24, gap: 16 }
        Text {
            value: "Benvenuto in ${projectName}"
            style: { size: 28, weight: "bold", color: "#111" }
        }
        Button {
            text: "Inizia"
            style: {
                bg: "#3b82f6"
                color: "#fff"
                radius: 8
                padding: 12
                hover: { bg: "#2563eb" }
            }
        }
    }
}
`)

    // .gitignore
    writeFileSync(join(projectDir, ".gitignore"), `out/
node_modules/
*.exe
*.wasm
*.o
`)

    console.log(`\n✓ Progetto "${projectName}" creato\n`)
    console.log(`  cd ${projectName}`)
    console.log(`  flow dev\n`)
}
