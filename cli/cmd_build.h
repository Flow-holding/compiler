#pragma once

/* flow build (prod=1, run_flag=0)
   flow run   (prod=0, run_flag=1)
   Ritorna exit code. */
int cmd_build(int prod, int run_flag);
