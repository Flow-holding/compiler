import * as AST from "../parser/ast"
import { FlowType, T, compatible, typeToStr } from "./types"

export type TypeError = {
    message: string
    line?: number
}

type Env = Map<string, FlowType>

export function typecheck(program: AST.Program): TypeError[] {
    const errors: TypeError[] = []
    const globalEnv: Env = new Map()

    // Carica i built-in della stdlib
    loadBuiltins(globalEnv)

    const err = (msg: string) => errors.push({ message: msg })

    // ── Converti TypeExpr AST → FlowType ──────

    function resolveTypeExpr(t: AST.TypeExpr): FlowType {
        switch (t.kind) {
            case "PrimitiveType": return T[t.name]()
            case "ArrayType":     return T.array(resolveTypeExpr(t.item))
            case "MapType":       return T.map(resolveTypeExpr(t.key), resolveTypeExpr(t.value))
            case "OptionalType":  return T.optional(resolveTypeExpr(t.inner))
            case "NamedType": {
                const found = globalEnv.get(t.name)
                return found ?? T.unknown()
            }
        }
    }

    // ── Inferisci tipo di un nodo ──────────────

    function infer(node: AST.Node, env: Env): FlowType {
        switch (node.kind) {

            case "Literal": {
                if (node.type === "str")   return T.str()
                if (node.type === "int")   return T.int()
                if (node.type === "float") return T.float()
                if (node.type === "bool")  return T.bool()
                if (node.type === "null")  return T.null()
                return T.unknown()
            }

            case "Identifier": {
                const t = env.get(node.name) ?? globalEnv.get(node.name)
                if (!t) err(`"${node.name}" non definito`)
                return t ?? T.unknown()
            }

            case "ArrayLiteral": {
                if (node.items.length === 0) return T.array(T.any())
                const itemType = infer(node.items[0], env)
                for (const item of node.items.slice(1)) {
                    const t = infer(item, env)
                    if (!compatible(t, itemType)) err(`Array misto: atteso ${typeToStr(itemType)}, trovato ${typeToStr(t)}`)
                }
                return T.array(itemType)
            }

            case "MapLiteral": {
                if (node.entries.length === 0) return T.map(T.any(), T.any())
                const keyType = infer(node.entries[0].key, env)
                const valType = infer(node.entries[0].value, env)
                return T.map(keyType, valType)
            }

            case "BinaryExpr": {
                const l = infer(node.left, env)
                const r = infer(node.right, env)
                if (["+", "-", "*", "/"].includes(node.op)) {
                    if (!compatible(l, T.int()) && !compatible(l, T.float()) && !compatible(l, T.str()))
                        err(`Operatore "${node.op}" non applicabile a ${typeToStr(l)}`)
                    return l
                }
                if (["==", "!=", "<", ">", "<=", ">="].includes(node.op)) return T.bool()
                if (["&&", "||"].includes(node.op)) {
                    if (!compatible(l, T.bool())) err(`"${node.op}" richiede bool, trovato ${typeToStr(l)}`)
                    return T.bool()
                }
                return T.unknown()
            }

            case "UnaryExpr": {
                const t = infer(node.value, env)
                if (node.op === "!" && !compatible(t, T.bool())) err(`"!" richiede bool, trovato ${typeToStr(t)}`)
                if (node.op === "-" && !compatible(t, T.int()) && !compatible(t, T.float()))
                    err(`"-" richiede int o float, trovato ${typeToStr(t)}`)
                return t
            }

            case "MemberExpr": {
                const obj = infer(node.object, env)
                // accesso a campo di struct
                if (obj.kind === "struct") {
                    const field = obj.fields[node.property]
                    if (!field) err(`"${obj.name}" non ha il campo "${node.property}"`)
                    return field ?? T.unknown()
                }
                // metodi built-in su array
                if (obj.kind === "array") {
                    if (node.property === "len") return T.int()
                    if (node.property === "first" || node.property === "last") return T.optional(obj.item)
                }
                // metodi built-in su str
                if (obj.kind === "str") {
                    if (node.property === "len") return T.int()
                    if (["upper","lower","trim"].includes(node.property)) return T.str()
                }
                return T.unknown()
            }

            case "CallExpr": {
                const callee = infer(node.callee, env)
                if (callee.kind === "fn") {
                    if (node.args.length !== callee.params.length)
                        err(`Argomenti errati: attesi ${callee.params.length}, trovati ${node.args.length}`)
                    node.args.forEach((arg, idx) => {
                        const argType = infer(arg, env)
                        const expected = callee.params[idx]
                        if (expected && !compatible(argType, expected))
                            err(`Argomento ${idx+1}: atteso ${typeToStr(expected)}, trovato ${typeToStr(argType)}`)
                    })
                    return callee.ret
                }
                if (callee.kind !== "unknown" && callee.kind !== "any")
                    err(`"${(node.callee as any).name ?? "expr"}" non è una funzione`)
                return T.unknown()
            }

            case "AssignExpr": {
                const valType = infer(node.value, env)
                if (node.target.kind === "Identifier") {
                    const existing = env.get(node.target.name) ?? globalEnv.get(node.target.name)
                    if (existing && !compatible(valType, existing))
                        err(`"${node.target.name}": atteso ${typeToStr(existing)}, assegnato ${typeToStr(valType)}`)
                    env.set(node.target.name, valType)
                }
                return valType
            }

            case "IfStmt": {
                const condType = infer(node.condition, env)
                if (!compatible(condType, T.bool()))
                    err(`Condizione if deve essere bool, trovato ${typeToStr(condType)}`)
                checkBlock(node.then, new Map(env))
                if (node.else) checkBlock(node.else, new Map(env))
                return T.void()
            }

            case "ForStmt": {
                const listType = infer(node.iterable, env)
                const loopEnv = new Map(env)
                if (listType.kind === "array") loopEnv.set(node.item, listType.item)
                else loopEnv.set(node.item, T.unknown())
                checkBlock(node.body, loopEnv)
                return T.void()
            }

            case "ReturnStmt": {
                return node.value ? infer(node.value, env) : T.void()
            }

            case "FnDecl":
            case "StructDecl":
            case "Block":
            case "MatchStmt":
            case "Annotation":
                return T.void()

            default:
                return T.unknown()
        }
    }

    function checkBlock(block: AST.Block, env: Env) {
        for (const node of block.body) infer(node, env)
    }

    // ── Prima passata — registra tutte le fn e struct ──

    for (const node of program.body) {
        if (node.kind === "FnDecl") {
            const params = node.params.map(p => resolveTypeExpr(p.type))
            globalEnv.set(node.name, T.fn(params, T.unknown()))
        }
        if (node.kind === "StructDecl") {
            const fields: Record<string, FlowType> = {}
            for (const f of node.fields) fields[f.name] = resolveTypeExpr(f.type)
            globalEnv.set(node.name, { kind: "struct", name: node.name, fields })
        }
    }

    // ── Seconda passata — controlla i corpi ───────────

    for (const node of program.body) {
        if (node.kind === "FnDecl") {
            const fnEnv = new Map(globalEnv)
            for (const p of node.params) fnEnv.set(p.name, resolveTypeExpr(p.type))
            checkBlock(node.body, fnEnv)
        } else {
            infer(node, globalEnv)
        }
    }

    return errors
}

// ── Built-in della stdlib ──────────────────────────

function loadBuiltins(env: Env) {
    // print overloads
    env.set("print", T.fn([T.any()], T.void()))
    env.set("eprint", T.fn([T.str()], T.void()))

    // fs
    env.set("fs", { kind: "struct", name: "fs", fields: {
        read:   T.fn([T.str()], T.optional(T.str())),
        write:  T.fn([T.str(), T.str()], T.bool()),
        exists: T.fn([T.str()], T.bool()),
        delete: T.fn([T.str()], T.bool()),
    }})

    // http
    env.set("http", { kind: "struct", name: "http", fields: {
        serve:    T.fn([T.int(), T.fn([T.any()], T.any())], T.void()),
        response: T.fn([T.int(), T.str(), T.str()], T.any()),
    }})

    // db
    env.set("db", { kind: "struct", name: "db", fields: {
        find:    T.fn([T.str()], T.array(T.any())),
        findOne: T.fn([T.str(), T.str()], T.optional(T.any())),
        insert:  T.fn([T.str(), T.any()], T.str()),
        update:  T.fn([T.str(), T.str(), T.any()], T.bool()),
        delete:  T.fn([T.str(), T.str()], T.bool()),
    }})

    // router
    env.set("router", { kind: "struct", name: "router", fields: {
        navigate: T.fn([T.str()], T.void()),
        current:  T.fn([], T.str()),
        back:     T.fn([], T.void()),
    }})
}
