import * as AST from "../parser/ast"

export type CodegenTarget = "native" | "wasm"

export function codegen(program: AST.Program, target: CodegenTarget = "native", runtimeDir?: string): string {
    const lines: string[] = []
    let indent = 0

    const emit  = (s: string) => lines.push("  ".repeat(indent) + s)
    const push  = () => indent++
    const pop   = () => indent--

    // ── Tipi Flow → C ────────────────────────────

    function genType(t: AST.TypeExpr): string {
        switch (t.kind) {
            case "PrimitiveType":
                switch (t.name) {
                    case "str":   return "FlowStr*"
                    case "int":   return "int32_t"
                    case "float": return "double"
                    case "bool":  return "int32_t"
                    case "void":  return "void"
                    case "any":   return "FlowObject*"
                }
            case "ArrayType":    return "FlowArray*"
            case "MapType":      return "FlowMap*"
            case "OptionalType": return genType(t.inner)
            case "NamedType":    return `Flow_${t.name}*`
        }
    }

    // ── Espressioni ──────────────────────────────

    function genExpr(node: AST.Node): string {
        switch (node.kind) {

            case "Literal": {
                if (node.type === "str")   return `flow_str_new("${node.value}")`
                if (node.type === "int")   return `${node.value}`
                if (node.type === "float") return `${node.value}`
                if (node.type === "bool")  return node.value ? "1" : "0"
                if (node.type === "null")  return "NULL"
                return "NULL"
            }

            case "Identifier":
                return node.name

            case "ArrayLiteral": {
                if (node.items.length === 0) return "flow_array_new()"
                const tmp = `_arr_${Math.random().toString(36).slice(2, 7)}`
                // Genera come espressione inline non è possibile in C —
                // restituiamo un segnaposto e gestiamo nel caller
                return `flow_array_new() /* [${node.items.map(genExpr).join(", ")}] */`
            }

            case "MapLiteral":
                return "flow_map_new()"

            case "BinaryExpr": {
                const l = genExpr(node.left)
                const r = genExpr(node.right)
                // Concatenazione stringhe
                if (node.op === "+") return `flow_str_concat(${l}, ${r})`
                // Uguaglianza stringhe
                if (node.op === "==") return `flow_str_eq(${l}, ${r})`
                if (node.op === "!=") return `!flow_str_eq(${l}, ${r})`
                return `(${l} ${node.op} ${r})`
            }

            case "UnaryExpr":
                return `(${node.op}${genExpr(node.value)})`

            case "MemberExpr": {
                const obj = genExpr(node.object)
                // Metodi built-in su array
                if (node.property === "len") return `flow_array_len((FlowArray*)${obj})`
                if (node.property === "first") return `flow_array_get((FlowArray*)${obj}, 0)`
                if (node.property === "last") return `flow_array_get((FlowArray*)${obj}, flow_array_len((FlowArray*)${obj})-1)`
                // Metodi built-in su str
                if (node.property === "len" ) return `flow_str_len((FlowStr*)${obj})`
                // Accesso a campo struct
                return `${obj}->${node.property}`
            }

            case "CallExpr": {
                // Chiamata su membro: db.find(...)
                if (node.callee.kind === "MemberExpr") {
                    const obj  = node.callee.object
                    const prop = node.callee.property
                    const args = node.args.map(genExpr)

                    // Namespace built-in
                    if (obj.kind === "Identifier") {
                        const ns = obj.name
                        if (ns === "fs") {
                            if (prop === "read")   return `flow_fs_read(${args[0]}->data)`
                            if (prop === "write")  return `flow_fs_write(${args[0]}->data, ${args[1]})`
                            if (prop === "exists") return `flow_fs_exists(${args[0]}->data)`
                            if (prop === "delete") return `flow_fs_delete(${args[0]}->data)`
                        }
                        if (ns === "db") {
                            if (prop === "find")    return `flow_db_find(${args[0]}->data)`
                            if (prop === "findOne") return `flow_db_find_one(${args[0]}->data, ${args[1]}->data)`
                            if (prop === "insert")  return `flow_db_insert(${args[0]}->data, (FlowObject*)${args[1]})`
                            if (prop === "update")  return `flow_db_update(${args[0]}->data, ${args[1]}->data, (FlowObject*)${args[2]})`
                            if (prop === "delete")  return `flow_db_delete(${args[0]}->data, ${args[1]}->data)`
                        }
                        if (ns === "http") {
                            if (prop === "response") return `flow_http_response(${args[0]}, ${args[1]}->data, ${args[2]}->data)`
                            if (prop === "serve")    return `flow_http_serve(${args[0]}, ${args[1]})`
                        }
                        if (ns === "router") {
                            if (prop === "navigate") return `flow_router_navigate(${args[0]}->data)`
                            if (prop === "current")  return `flow_router_current()`
                            if (prop === "back")     return `flow_router_back()`
                        }
                    }
                    return `${genExpr(node.callee)}(${args.join(", ")})`
                }

                // Chiamata diretta
                const callee = genExpr(node.callee)
                const args   = node.args.map(genExpr)

                // Built-in print — overload per tipo
                if (callee === "print") {
                    const arg = node.args[0]
                    if (arg?.kind === "Literal") {
                        if (arg.type === "int")   return `flow_print_int(${args[0]})`
                        if (arg.type === "float") return `flow_print_float(${args[0]})`
                        if (arg.type === "bool")  return `flow_print_bool(${args[0]})`
                    }
                    return `flow_print(${args[0]})`
                }

                return `${callee}(${args.join(", ")})`
            }

            case "AssignExpr": {
                const target = genExpr(node.target)
                const value  = genExpr(node.value)
                // Prima assegnazione → dichiarazione
                return `${target} = ${value}`
            }

            default:
                return "/* expr non supportata */"
        }
    }

    // ── Statements ───────────────────────────────

    function genStmt(node: AST.Node) {
        switch (node.kind) {

            case "AssignExpr": {
                const target = genExpr((node as AST.AssignExpr).target)
                const value  = genExpr((node as AST.AssignExpr).value)
                emit(`__auto_type ${target} = ${value};`)
                break
            }

            case "IfStmt": {
                emit(`if (${genExpr(node.condition)}) {`)
                push()
                genBlock(node.then)
                pop()
                if (node.else) {
                    emit("} else {")
                    push()
                    genBlock(node.else)
                    pop()
                }
                emit("}")
                break
            }

            case "ForStmt": {
                const iter = genExpr(node.iterable)
                const tmp  = `_len_${node.item}`
                emit(`{ FlowArray* _arr_${node.item} = (FlowArray*)${iter};`)
                emit(`  int32_t ${tmp} = flow_array_len(_arr_${node.item});`)
                emit(`  for (int32_t _i_${node.item} = 0; _i_${node.item} < ${tmp}; _i_${node.item}++) {`)
                push(); push()
                emit(`FlowObject* ${node.item} = flow_array_get(_arr_${node.item}, _i_${node.item});`)
                genBlock(node.body)
                pop(); pop()
                emit("  }")
                emit("}")
                break
            }

            case "ReturnStmt":
                emit(node.value ? `return ${genExpr(node.value)};` : "return;")
                break

            case "MatchStmt": {
                const val = genExpr(node.value)
                node.cases.forEach((c, idx) => {
                    if (c.pattern === "_") {
                        emit(idx === 0 ? "{" : "else {")
                    } else {
                        const pat = genExpr(c.pattern as AST.Node)
                        emit(idx === 0
                            ? `if (flow_str_eq(${val}, ${pat})) {`
                            : `else if (flow_str_eq(${val}, ${pat})) {`)
                    }
                    push()
                    genBlock(c.body)
                    pop()
                    emit("}")
                })
                break
            }

            case "CallExpr":
                emit(`${genExpr(node)};`)
                break

            default:
                emit(`${genExpr(node)};`)
        }
    }

    function genBlock(block: AST.Block) {
        for (const stmt of block.body) genStmt(stmt)
    }

    // ── Dichiarazioni top-level ───────────────────

    function genTopLevel(node: AST.Node) {
        switch (node.kind) {

            case "FnDecl": {
                // @native — niente codice, esiste già in C
                if (node.annotation?.name === "native") break

                const params = node.params
                    .map(p => `${genType(p.type)} ${p.name}`)
                    .join(", ")

                emit(`void ${node.name}(${params}) {`)
                push()
                genBlock(node.body)
                pop()
                emit("}")
                emit("")
                break
            }

            case "StructDecl": {
                emit(`typedef struct {`)
                push()
                emit("FlowObject base;")
                for (const f of node.fields) {
                    emit(`${genType(f.type)} ${f.name};`)
                }
                pop()
                emit(`} Flow_${node.name};`)
                emit("")
                break
            }

            default:
                genStmt(node)
        }
    }

    // ── Header includes ───────────────────────────

    // Normalizza i path con slash forward (clang non accetta backslash su Windows)
    const rt = (runtimeDir ?? "../../runtime").replace(/\\/g, "/")

    if (target === "wasm") {
        emit(`#include "${rt}/wasm/runtime.c"`)
    } else {
        emit(`#include "${rt}/io/print.c"`)
        emit(`#include "${rt}/io/fs.c"`)
        emit(`#include "${rt}/net/http.c"`)
    }
    emit("")

    for (const node of program.body) {
        // WASM — aggiungi export attribute alle funzioni
        if (target === "wasm" && node.kind === "FnDecl" && node.annotation?.name !== "native") {
            emit(`__attribute__((export_name("${node.name}")))`)
        }
        genTopLevel(node)
    }

    return lines.join("\n")
}
