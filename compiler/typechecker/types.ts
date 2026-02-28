// Tipi interni del typechecker

export type FlowType =
    | { kind: "str" }
    | { kind: "int" }
    | { kind: "float" }
    | { kind: "bool" }
    | { kind: "void" }
    | { kind: "any" }
    | { kind: "null" }
    | { kind: "array";    item: FlowType }
    | { kind: "map";      key: FlowType; value: FlowType }
    | { kind: "optional"; inner: FlowType }
    | { kind: "fn";       params: FlowType[]; ret: FlowType }
    | { kind: "struct";   name: string; fields: Record<string, FlowType> }
    | { kind: "unknown" }

export const T = {
    str:     (): FlowType => ({ kind: "str" }),
    int:     (): FlowType => ({ kind: "int" }),
    float:   (): FlowType => ({ kind: "float" }),
    bool:    (): FlowType => ({ kind: "bool" }),
    void:    (): FlowType => ({ kind: "void" }),
    any:     (): FlowType => ({ kind: "any" }),
    null:    (): FlowType => ({ kind: "null" }),
    array:   (item: FlowType): FlowType => ({ kind: "array", item }),
    map:     (key: FlowType, value: FlowType): FlowType => ({ kind: "map", key, value }),
    optional:(inner: FlowType): FlowType => ({ kind: "optional", inner }),
    fn:      (params: FlowType[], ret: FlowType): FlowType => ({ kind: "fn", params, ret }),
    unknown: (): FlowType => ({ kind: "unknown" }),
}

// Due tipi sono compatibili?
export function compatible(a: FlowType, b: FlowType): boolean {
    if (a.kind === "any" || b.kind === "any") return true
    if (a.kind === "unknown" || b.kind === "unknown") return true
    if (a.kind !== b.kind) {
        // null è compatibile con optional
        if (b.kind === "optional" && a.kind === "null") return true
        if (a.kind === "optional" && b.kind === "null") return true
        // int è compatibile con float
        if (a.kind === "int" && b.kind === "float") return true
        if (a.kind === "float" && b.kind === "int") return true
        return false
    }
    if (a.kind === "array"    && b.kind === "array")    return compatible(a.item, b.item)
    if (a.kind === "optional" && b.kind === "optional") return compatible(a.inner, b.inner)
    return true
}

export function typeToStr(t: FlowType): string {
    switch (t.kind) {
        case "array":    return `${typeToStr(t.item)}[]`
        case "map":      return `{${typeToStr(t.key)}: ${typeToStr(t.value)}}`
        case "optional": return `?${typeToStr(t.inner)}`
        case "fn":       return `fn(${t.params.map(typeToStr).join(", ")}): ${typeToStr(t.ret)}`
        case "struct":   return t.name
        default:         return t.kind
    }
}
