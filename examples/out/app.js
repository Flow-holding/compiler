// Flow app — generato automaticamente
const Flow = (() => {
  const memory = new WebAssembly.Memory({ initial: 16 })
  const decoder = new TextDecoder()
  const encoder = new TextEncoder()

  let instance
  let heapPtr = 65536  // base heap

  function malloc(size) {
    const ptr = heapPtr
    heapPtr += (size + 7) & ~7  // allineamento 8 byte
    return ptr
  }

  function readStr(ptr) {
    const buf = new Uint8Array(memory.buffer)
    let end = ptr
    while (buf[end]) end++
    return decoder.decode(buf.slice(ptr, end))
  }

  function writeStr(str) {
    const bytes = encoder.encode(str + "\0")
    const ptr = malloc(bytes.length)
    new Uint8Array(memory.buffer).set(bytes, ptr)
    return ptr
  }

  // Bridge I/O — funzioni C richiamate dal WASM
  const imports = {
    env: {
      memory,

      // print
      flow_print_str(ptr) { console.log(readStr(ptr)) },
      flow_print_int(n)   { console.log(n) },
      flow_print_float(n) { console.log(n) },
      flow_print_bool(b)  { console.log(b ? "true" : "false") },
      flow_eprint_str(ptr){ console.error(readStr(ptr)) },

      // DOM — aggiorna un nodo HTML
      flow_dom_set_text(id, ptr)   {
        const el = document.querySelector(`[data-fl="${id}"]`)
        if (el) el.textContent = readStr(ptr)
      },
      flow_dom_set_class(id, ptr)  {
        const el = document.querySelector(`[data-fl="${id}"]`)
        if (el) el.className = readStr(ptr)
      },
      flow_dom_toggle_class(id, ptr) {
        const el = document.querySelector(`[data-fl="${id}"]`)
        if (el) el.classList.toggle(readStr(ptr))
      },

      // Event listener
      flow_dom_on(id, eventPtr, cbIdx) {
        const el = document.querySelector(`[data-fl="${id}"]`)
        if (el) el.addEventListener(readStr(eventPtr), () => {
          instance.exports.__flow_cb?.(cbIdx)
        })
      },

      // Alloca memoria dal JS (per stringhe create lato JS)
      flow_js_alloc: malloc,
    }
  }

  async function init() {
    const res  = await fetch("app.wasm")
    const wasm = await WebAssembly.instantiateStreaming(res, imports)
    instance   = wasm.instance

    if (typeof instance.exports.main === "function") {
      instance.exports.main()
    }
  }

  return {
    init,
    writeStr,
    readStr,
    saluta: (...args) => instance.exports.saluta?.(...args),
    main: (...args) => instance.exports.main?.(...args)
  }
})()

document.addEventListener("DOMContentLoaded", () => Flow.init())
