// flow update — scarica l'ultima versione e sostituisce il binario

import { join }                     from "node:path"
import { existsSync, readFileSync,
         writeFileSync }            from "node:fs"

const REPO         = "Flow-holding/compiler"
const RELEASES_URL = `https://api.github.com/repos/${REPO}/releases/latest`
const VERSION_URL  = `https://raw.githubusercontent.com/${REPO}/main/VERSION`
const DOWNLOAD_URL = `https://github.com/${REPO}/releases/download`

// Versione corrente (aggiornata dal build script)
export const CURRENT_VERSION = "0.2.5"

// File cache: salva l'ultima versione vista per non fare fetch ad ogni lancio
const CACHE_DIR  = join(process.env.LOCALAPPDATA ?? process.env.HOME ?? ".", ".flow")
const CACHE_FILE = join(CACHE_DIR, "update-check.json")
const CHECK_INTERVAL_MS = 1000 * 60 * 60 * 4  // ogni 4 ore

type UpdateCache = { checkedAt: number; latestVersion: string }

function readCache(): UpdateCache | null {
    try {
        if (!existsSync(CACHE_FILE)) return null
        return JSON.parse(readFileSync(CACHE_FILE, "utf8")) as UpdateCache
    } catch { return null }
}

function writeCache(latest: string) {
    try {
        if (!existsSync(CACHE_DIR)) require("node:fs").mkdirSync(CACHE_DIR, { recursive: true })
        writeFileSync(CACHE_FILE, JSON.stringify({ checkedAt: Date.now(), latestVersion: latest }))
    } catch {}
}

// Controlla in background senza bloccare — chiamato all'avvio del CLI
// Se c'è una versione nuova, stampa un avviso DOPO che il comando finisce
export function checkUpdateBackground(): () => void {
    const cache = readCache()
    const now   = Date.now()

    // Non ri-controllare se abbiamo già verificato di recente
    if (cache && (now - cache.checkedAt) < CHECK_INTERVAL_MS) {
        if (cache.latestVersion !== CURRENT_VERSION) {
            // Già sappiamo che c'è un aggiornamento — avvisa subito
            return () => printUpdateHint(cache.latestVersion)
        }
        return () => {}
    }

    // Fetch asincrono, non blocca l'esecuzione del comando corrente
    let pendingLatest: string | null = null
    const promise = fetch(RELEASES_URL, {
        headers: { "User-Agent": "flow-cli" },
        signal: AbortSignal.timeout(3000),  // timeout 3s
    })
        .then(r => r.json() as Promise<{ tag_name: string }>)
        .then(d => {
            const v = d.tag_name.replace(/^v/, "")
            writeCache(v)
            pendingLatest = v
        })
        .catch(() => {})  // silenzioso se offline

    // Restituisce una funzione da chiamare DOPO il comando
    // a quel punto il fetch è quasi certamente già completato
    return async () => {
        await promise
        if (pendingLatest && pendingLatest !== CURRENT_VERSION) {
            printUpdateHint(pendingLatest)
        }
    }
}

function printUpdateHint(latest: string) {
    console.log(`\n\x1b[33m╔══════════════════════════════════════════╗`)
    console.log(`║  Aggiornamento disponibile: v${latest.padEnd(12)} ║`)
    console.log(`║  Esegui:  flow update                    ║`)
    console.log(`╚══════════════════════════════════════════╝\x1b[0m`)
}

export async function update() {
    const isWindows = process.platform === "win32"

    console.log(`\x1b[36mflow update\x1b[0m — versione attuale: \x1b[33m${CURRENT_VERSION}\x1b[0m`)
    process.stdout.write("Controllo aggiornamenti...")

    // 1. Controlla l'ultima versione disponibile
    let latestVersion: string | null = null

    try {
        const res = await fetch(RELEASES_URL, {
            headers: { "User-Agent": "flow-cli" },
            signal: AbortSignal.timeout(5000),
        })
        if (res.ok) {
            const data = await res.json() as { tag_name: string }
            latestVersion = data.tag_name.replace(/^v/, "")
        }
    } catch {}

    if (!latestVersion) {
        try {
            const res = await fetch(VERSION_URL, {
                headers: { "User-Agent": "flow-cli" },
                signal: AbortSignal.timeout(5000),
            })
            if (res.ok) latestVersion = (await res.text()).trim()
        } catch {}
    }

    if (!latestVersion) {
        console.log(` \x1b[33mImpossibile raggiungere il server — riprova più tardi\x1b[0m`)
        return
    }

    if (latestVersion === CURRENT_VERSION) {
        console.log(` \x1b[32m✓ Già all'ultima versione (${CURRENT_VERSION})\x1b[0m`)
        return
    }

    console.log(` \x1b[32m${latestVersion} disponibile!\x1b[0m`)

    // 2. URL del binario per questa piattaforma
    const platform  = isWindows ? "windows" : process.platform === "darwin" ? "macos" : "linux"
    const arch      = process.arch === "arm64" ? "arm64" : "x64"
    const ext       = isWindows ? ".exe" : ""
    const assetName = `flow-${platform}-${arch}${ext}`
    const downloadURL = `${DOWNLOAD_URL}/v${latestVersion}/${assetName}`

    console.log(`Download \x1b[36m${assetName}\x1b[0m...`)

    // 3. Scarica il nuovo binario
    let binaryData: ArrayBuffer
    try {
        const res = await fetch(downloadURL)
        if (!res.ok) throw new Error(`HTTP ${res.status}`)
        binaryData = await res.arrayBuffer()
    } catch (e) {
        console.log(`\x1b[31mErrore download: ${e}\x1b[0m`)
        console.log(`\x1b[33mPuoi aggiornare manualmente:\x1b[0m`)
        console.log(`  ${downloadURL}`)
        process.exit(1)
    }

    // 4. Sostituisce il binario attuale
    const currentExe = process.execPath

    // Su Windows non puoi sovrascrivere un .exe in esecuzione
    // Usa un file .tmp e uno script .bat per sostituirlo al prossimo avvio
    if (isWindows) {
        const tmpExe  = currentExe + ".new"
        const batFile = currentExe.replace(/\.exe$/, "-update.bat")

        await Bun.write(tmpExe, new Uint8Array(binaryData))

        const bat = `@echo off
timeout /t 1 /nobreak >nul
move /y "${tmpExe}" "${currentExe}"
echo flow aggiornato a v${latestVersion}
del "%~f0"
`
        await Bun.write(batFile, bat)

        console.log(`\x1b[32m✓ Aggiornamento preparato — chiudi questo terminale e riaprilo\x1b[0m`)
        console.log(`  Il file \x1b[33m${batFile}\x1b[0m verrà eseguito automaticamente`)

        // Avvia il bat in background
        Bun.spawn(["cmd", "/c", "start", "/min", batFile], { stdio: ["ignore", "ignore", "ignore"] })
    } else {
        // POSIX: sostituzione diretta + chmod
        const { writeFileSync, chmodSync } = await import("node:fs")
        const tmpPath = currentExe + ".new"
        writeFileSync(tmpPath, Buffer.from(binaryData))
        chmodSync(tmpPath, 0o755)
        const { renameSync } = await import("node:fs")
        renameSync(tmpPath, currentExe)
        console.log(`\x1b[32m✓ flow aggiornato a v${latestVersion}\x1b[0m`)
    }
}
