// Gestisce il compiler C (flowc) — lo compila al primo avvio e lo cachea
// Il sorgente C è embedded nel binario tramite compiler-bundle.ts

import { join }                      from "node:path"
import { existsSync, mkdirSync,
         writeFileSync }             from "node:fs"
import { COMPILER_BUNDLE }          from "../devtools/compiler-ts-unused/compiler-bundle"

const FLOW_HOME     = join(process.env.USERPROFILE ?? process.env.HOME ?? ".", ".flow")
const COMPILER_DIR  = join(FLOW_HOME, "compiler")
const FLOWC_EXE     = join(COMPILER_DIR, process.platform === "win32" ? "flowc.exe" : "flowc")
const STAMP_FILE    = join(COMPILER_DIR, ".built")

// Estrae i sorgenti C del compiler e compila flowc
async function buildFlowc(): Promise<boolean> {
    mkdirSync(join(COMPILER_DIR, "lexer"),   { recursive: true })
    mkdirSync(join(COMPILER_DIR, "parser"),  { recursive: true })
    mkdirSync(join(COMPILER_DIR, "codegen"), { recursive: true })

    for (const [relPath, content] of Object.entries(COMPILER_BUNDLE)) {
        writeFileSync(join(COMPILER_DIR, relPath), content as string)
    }

    process.stdout.write("  Compilazione flowc (prima volta)...")
    const proc = Bun.spawnSync([
        "clang",
        join(COMPILER_DIR, "main.c"),
        join(COMPILER_DIR, "lexer/lexer.c"),
        join(COMPILER_DIR, "parser/parser.c"),
        join(COMPILER_DIR, "codegen/codegen.c"),
        "-o", FLOWC_EXE,
        "-O2", "-w"
    ])

    if (proc.exitCode === 0) {
        writeFileSync(STAMP_FILE, "1")
        console.log(" ✓")
        return true
    } else {
        console.log(" ✗")
        console.error(new TextDecoder().decode(proc.stderr).split("\n").slice(0, 5).join("\n"))
        return false
    }
}

// Ritorna il path a flowc, compilandolo se necessario
export async function getFlowc(): Promise<string | null> {
    // In sviluppo: usa flowc.exe dalla cartella compiler/
    const devFlowc = join(import.meta.dir, "../compiler/flowc.exe")
    if (existsSync(devFlowc)) return devFlowc

    // Produzione: usa quello cached in ~/.flow/compiler/
    if (existsSync(FLOWC_EXE) && existsSync(STAMP_FILE)) return FLOWC_EXE

    // Prima volta: compila
    const ok = await buildFlowc()
    return ok ? FLOWC_EXE : null
}

// Esegui flowc su un file .flow
export async function runFlowc(
    inputPath: string,
    outDir:    string,
    opts: { run?: boolean; prod?: boolean } = {}
): Promise<boolean> {
    const flowc = await getFlowc()
    if (!flowc) return false

    const args = [flowc, inputPath, "--outdir", outDir]
    if (opts.run)  args.push("--run")
    if (opts.prod) args.push("--prod")

    const proc = Bun.spawnSync(args, { stdio: ["inherit", "inherit", "inherit"] })
    return proc.exitCode === 0
}
