import { cmdInit } from "./init"
import { build }   from "./build"
import { cmdDev }  from "./dev"
import { update, checkUpdateBackground, CURRENT_VERSION } from "./update"

const args    = process.argv.slice(2)
const command = args[0]

// Avvia il check in background — non blocca, notifica solo alla fine
const notifyUpdate = command !== "update" ? checkUpdateBackground() : () => {}

const HELP = `
Flow v${CURRENT_VERSION} — linguaggio e framework universale

COMANDI:
  flow init [nome]    Crea un nuovo progetto
  flow run            Compila ed esegue
  flow build          Build produzione
  flow dev            Server dev con watch
  flow update         Aggiorna alla versione più recente
  flow help           Mostra questo messaggio

ESEMPI:
  flow init mia-app
  cd mia-app
  flow dev
`

switch (command) {

    case "init": {
        const nome = args[1]
        await cmdInit(nome)
        break
    }

    case "run": {
        await build({ run: true })
        break
    }

    case "build": {
        await build({ prod: true })
        break
    }

    case "dev": {
        await cmdDev()
        break
    }

    case "update": {
        await update()
        break
    }

    case "help":
    case "--help":
    case "-h":
    case undefined: {
        console.log(HELP)
        break
    }

    default: {
        console.error(`✗ Comando sconosciuto: "${command}"`)
        console.log(`  Usa "flow help" per vedere i comandi disponibili`)
        process.exit(1)
    }
}

// Stampa eventuale avviso aggiornamento dopo che il comando è finito
await notifyUpdate()
