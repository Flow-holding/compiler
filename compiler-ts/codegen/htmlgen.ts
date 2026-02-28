import * as AST from "../parser/ast"
import { genClassName, styleToCSS, tagBaseCSS, resetClassCounter } from "./cssgen"

// Mappa tag Flow → tag HTML
const TAG_MAP: Record<string, string> = {
    View:       "div",
    Column:     "div",
    Row:        "div",
    Stack:      "div",
    Grid:       "div",
    ScrollView: "div",
    Text:       "p",
    Button:     "button",
    Input:      "input",
    Image:      "img",
    Modal:      "dialog",
}

export type HTMLOutput = {
    html:   string
    css:    string
    assets: string[]   // path degli asset trovati
}

export function generateHTML(program: AST.Program): HTMLOutput {
    resetClassCounter()

    const cssBlocks:  string[] = []
    const components: string[] = []
    const assets:     string[] = []

    // Stili reset base
    cssBlocks.push(`*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }`)
    cssBlocks.push(`body { font-family: system-ui, sans-serif; }`)

    for (const node of program.body) {
        if (node.kind !== "ComponentDecl") continue
        const annotation = node.annotation?.name
        if (annotation === "server") continue   // @server → skip

        const { html, css, assets: a } = renderComponent(node)
        components.push(html)
        cssBlocks.push(...css)
        assets.push(...a)
    }

    const fullHTML = buildHTML(components)
    return { html: fullHTML, css: cssBlocks.join("\n\n"), assets }

    // ── Render un component ─────────────────────

    function renderComponent(comp: AST.ComponentDecl): { html: string; css: string[]; assets: string[] } {
        const css:    string[] = []
        const assets: string[] = []

        function renderNode(node: AST.UINode, indent = 0): string {
            const className = genClassName(node.tag)
            const htmlTag   = TAG_MAP[node.tag] ?? "div"
            const pad       = "  ".repeat(indent)

            // CSS
            const base = tagBaseCSS(node.tag, className)
            if (base) css.push(base)
            if (node.style) {
                const styleCSS = styleToCSS(className, node.style)
                if (styleCSS) css.push(styleCSS)
            }

            // Props speciali
            const attrs: string[] = [`class="${className}"`]
            let innerText = ""
            let selfClose = false

            for (const prop of node.props) {
                const val = resolveStaticValue(prop.value)

                if (prop.key === "value" || prop.key === "text") {
                    innerText = val
                }
                if (prop.key === "src") {
                    attrs.push(`src="${val}"`)
                    if (isAsset(val)) assets.push(val)
                }
                if (prop.key === "alt")         attrs.push(`alt="${val}"`)
                if (prop.key === "placeholder")  attrs.push(`placeholder="${val}"`)
                if (prop.key === "onClick")      attrs.push(`onclick="${val}"`)
            }

            // Self-closing tags
            if (node.tag === "Input" || node.tag === "Image") {
                selfClose = true
            }

            if (selfClose) {
                return `${pad}<${htmlTag} ${attrs.join(" ")} />`
            }

            const childrenHTML = node.children
                .map(child => renderNode(child, indent + 1))
                .join("\n")

            const body = innerText
                ? innerText
                : childrenHTML
                    ? `\n${childrenHTML}\n${pad}`
                    : ""

            return `${pad}<${htmlTag} ${attrs.join(" ")}>${body}</${htmlTag}>`
        }

        return {
            html:   renderNode(comp.body),
            css,
            assets,
        }
    }
}

// ── Helpers ─────────────────────────────────────

function resolveStaticValue(node: AST.Node): string {
    if (node.kind === "Literal") return String(node.value)
    if (node.kind === "Identifier") return `\${${node.name}}`
    return ""
}

function isAsset(path: string): boolean {
    return /\.(png|jpg|jpeg|svg|gif|webp|ico|ttf|woff|woff2|mp4|mp3)$/i.test(path)
}

function buildHTML(components: string[]): string {
    return `<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Flow App</title>
  <link rel="stylesheet" href="style.css" />
</head>
<body>
  <div id="app">
${components.map(c => "    " + c).join("\n")}
  </div>
  <script src="app.js"></script>
</body>
</html>`
}
