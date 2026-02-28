import { StyleProps, BaseStyle, AnimProps } from "../parser/ast"
import { BASE_PROPS, BREAKPOINTS, KEYFRAMES, PSEUDO_KEYS } from "../style-map"

let classCounter = 0
export function resetClassCounter() { classCounter = 0 }
export function genClassName(tag: string): string {
    return `fl-${tag.toLowerCase()}-${++classCounter}`
}

// ── BaseStyle → regole CSS ─────────────────────────────

function baseToRules(s: BaseStyle): string[] {
    const rules: string[] = []

    for (const [key, val] of Object.entries(s)) {
        if (val === undefined || val === null) continue
        const def = BASE_PROPS[key]
        if (!def) continue

        switch (def.type) {
            case "string":
                rules.push(`${def.css}: ${val}`)
                break
            case "px":
                rules.push(`${def.css}: ${val}px`)
                break
            case "number":
                rules.push(`${def.css}: ${val}`)
                break
            case "bool":
                if (val) rules.push(`${def.css}: 0 2px 8px rgba(0,0,0,0.15)`)
                break
            case "px-array":
                rules.push(Array.isArray(val)
                    ? `${def.css}: ${(val as number[]).map(v => v + "px").join(" ")}`
                    : `${def.css}: ${val}px`)
                break
        }
    }

    return rules
}

function block(selector: string, rules: string[]): string {
    if (!rules.length) return ""
    return `${selector} {\n${rules.map(r => `  ${r};`).join("\n")}\n}`
}

// ── StyleProps → CSS completo ──────────────────────────

export function styleToCSS(className: string, style: StyleProps): string {
    const blocks: string[] = []
    const sel = `.${className}`

    // Shorthand + base
    const directRules = baseToRules(style as BaseStyle)
    if (style.base) directRules.push(...baseToRules(style.base))
    if (directRules.length) blocks.push(block(sel, directRules))

    // Pseudo-classi: hover, focus, active...
    for (const pseudo of PSEUDO_KEYS) {
        const val = (style as any)[pseudo]
        if (val) {
            const r = baseToRules(val)
            if (r.length) blocks.push(block(`${sel}:${pseudo}`, r))
        }
    }

    // Breakpoint: mob, tab, desk, wide
    for (const [key, query] of Object.entries(BREAKPOINTS)) {
        const val = (style as any)[key]
        if (val) {
            const r = baseToRules(val)
            if (r.length) blocks.push(`@media ${query} {\n${block(`  ${sel}`, r)}\n}`)
        }
    }

    // Animazione
    if (style.anim) {
        const a = style.anim
        const kf = KEYFRAMES[a.type]
        if (kf) blocks.push(kf)
        const dur    = a.duration ?? 300
        const delay  = a.delay    ?? 0
        const ease   = a.easing   ?? "ease"
        const repeat = a.repeat === "loop" ? "infinite" : String(a.repeat ?? 1)
        blocks.push(block(sel, [`animation: ${a.type} ${dur}ms ${ease} ${delay}ms ${repeat}`]))
    }

    return blocks.filter(Boolean).join("\n\n")
}

// ── Stili base per tag layout ──────────────────────────

export function tagBaseCSS(tag: string, className: string): string {
    const sel = `.${className}`
    switch (tag) {
        case "Column":     return block(sel, ["display: flex", "flex-direction: column"])
        case "Row":        return block(sel, ["display: flex", "flex-direction: row"])
        case "Stack":      return block(sel, ["position: relative"])
        case "Grid":       return block(sel, ["display: grid"])
        case "ScrollView": return block(sel, ["overflow: auto"])
        case "Button":     return block(sel, ["cursor: pointer", "display: inline-flex", "align-items: center", "justify-content: center", "border: none", "background: none"])
        case "Input":      return block(sel, ["outline: none", "border: 1px solid #ccc", "padding: 8px 12px", "border-radius: 4px"])
        default:           return ""
    }
}
