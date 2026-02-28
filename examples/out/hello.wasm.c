#include "../../runtime/wasm/runtime.c"

__attribute__((export_name("saluta")))
void saluta(FlowStr* nome) {
  flow_print(flow_str_concat(flow_str_new("Ciao "), nome));
}

__attribute__((export_name("main")))
void main() {
  saluta(flow_str_new("Mario"));
  flow_print(flow_str_new("Flow funziona"));
}

/* expr non supportata */;