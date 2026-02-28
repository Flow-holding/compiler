#include "../../runtime/io/print.c"
#include "../../runtime/io/fs.c"
#include "../../runtime/net/http.c"

void saluta(FlowStr* nome) {
  flow_print(flow_str_concat(flow_str_new("Ciao "), nome));
}

void main() {
  saluta(flow_str_new("Mario"));
  flow_print(flow_str_new("Flow funziona"));
}

/* expr non supportata */;