// ── UNICA fonte di verità per il sistema stile ──────────────────
// Aggiungere una proprietà qui la rende disponibile automaticamente
// nel parser e nel cssgen — zero modifiche altrove

export type PropType = "string" | "px" | "number" | "bool" | "px-array"

export type StyleProp = {
    css: string       // proprietà CSS generata
    type: PropType    // come viene parsata e generata
    unit?: string     // unità aggiunta automaticamente (es. "px")
}

// ── Proprietà base ───────────────────────────────────────────────

export const BASE_PROPS: Record<string, StyleProp> = {
    // Colori
    bg:          { css: "background",        type: "string" },
    color:       { css: "color",             type: "string" },

    // Testo
    size:        { css: "font-size",         type: "px" },
    weight:      { css: "font-weight",       type: "string" },
    italic:      { css: "font-style",        type: "string" },
    tracking:    { css: "letter-spacing",    type: "px" },
    leading:     { css: "line-height",       type: "number" },
    align:       { css: "text-align",        type: "string" },
    decoration:  { css: "text-decoration",   type: "string" },

    // Spaziatura
    padding:     { css: "padding",           type: "px-array" },
    margin:      { css: "margin",            type: "px-array" },
    gap:         { css: "gap",               type: "px" },

    // Dimensioni
    width:       { css: "width",             type: "string" },
    height:      { css: "height",            type: "string" },
    minW:        { css: "min-width",         type: "string" },
    maxW:        { css: "max-width",         type: "string" },
    minH:        { css: "min-height",        type: "string" },
    maxH:        { css: "max-height",        type: "string" },

    // Forma
    radius:      { css: "border-radius",     type: "px" },
    border:      { css: "border",            type: "string" },
    borderTop:   { css: "border-top",        type: "string" },
    borderBot:   { css: "border-bottom",     type: "string" },
    outline:     { css: "outline",           type: "string" },

    // Layout
    display:     { css: "display",           type: "string" },
    flex:        { css: "flex",              type: "number" },
    flexDir:     { css: "flex-direction",    type: "string" },
    wrap:        { css: "flex-wrap",         type: "string" },
    items:       { css: "align-items",       type: "string" },
    justify:     { css: "justify-content",   type: "string" },
    self:        { css: "align-self",        type: "string" },
    grow:        { css: "flex-grow",         type: "number" },
    shrink:      { css: "flex-shrink",       type: "number" },
    cols:        { css: "grid-template-columns", type: "string" },
    rows:        { css: "grid-template-rows",    type: "string" },

    // Posizione
    position:    { css: "position",          type: "string" },
    top:         { css: "top",               type: "px" },
    right:       { css: "right",             type: "px" },
    bottom:      { css: "bottom",            type: "px" },
    left:        { css: "left",              type: "px" },
    z:           { css: "z-index",           type: "number" },

    // Visibilità
    opacity:     { css: "opacity",           type: "number" },
    overflow:    { css: "overflow",          type: "string" },
    overflowX:   { css: "overflow-x",        type: "string" },
    overflowY:   { css: "overflow-y",        type: "string" },
    visible:     { css: "visibility",        type: "string" },
    clip:        { css: "clip-path",         type: "string" },

    // Effetti
    shadow:      { css: "box-shadow",        type: "bool" },
    blur:        { css: "filter",            type: "string" },
    brightness:  { css: "filter",            type: "string" },
    transform:   { css: "transform",         type: "string" },
    transition:  { css: "transition",        type: "string" },
    cursor:      { css: "cursor",            type: "string" },
    select:      { css: "user-select",       type: "string" },
    pointer:     { css: "pointer-events",    type: "string" },
}

// ── Breakpoint ───────────────────────────────────────────────────

export const BREAKPOINTS: Record<string, string> = {
    mob:  "(max-width: 767px)",
    tab:  "(min-width: 768px) and (max-width: 1023px)",
    desk: "(min-width: 1024px)",
    wide: "(min-width: 1440px)",
}

// ── Animazioni predefinite ───────────────────────────────────────

export const KEYFRAMES: Record<string, string> = {
    fadeIn:    `@keyframes fadeIn    { from { opacity: 0 } to { opacity: 1 } }`,
    fadeOut:   `@keyframes fadeOut   { from { opacity: 1 } to { opacity: 0 } }`,
    slideUp:   `@keyframes slideUp   { from { transform: translateY(20px); opacity: 0 } to { transform: translateY(0); opacity: 1 } }`,
    slideDown: `@keyframes slideDown { from { transform: translateY(-20px); opacity: 0 } to { transform: translateY(0); opacity: 1 } }`,
    slideLeft: `@keyframes slideLeft { from { transform: translateX(20px); opacity: 0 } to { transform: translateX(0); opacity: 1 } }`,
    bounce:    `@keyframes bounce    { 0%,100% { transform: translateY(0) } 50% { transform: translateY(-8px) } }`,
    spin:      `@keyframes spin      { from { transform: rotate(0deg) } to { transform: rotate(360deg) } }`,
    pulse:     `@keyframes pulse     { 0%,100% { opacity: 1 } 50% { opacity: 0.5 } }`,
    shake:     `@keyframes shake     { 0%,100% { transform: translateX(0) } 25% { transform: translateX(-4px) } 75% { transform: translateX(4px) } }`,
    pop:       `@keyframes pop       { 0% { transform: scale(0.9) } 50% { transform: scale(1.05) } 100% { transform: scale(1) } }`,
}

// ── Pseudo-classi ────────────────────────────────────────────────

export const PSEUDO_KEYS = new Set(["hover", "focus", "active", "disabled", "placeholder"])

// ── Chiavi speciali (non sono proprietà CSS dirette) ─────────────

export const SPECIAL_KEYS = new Set([
    "base", "anim",
    ...Object.keys(BREAKPOINTS),
    ...PSEUDO_KEYS,
])
