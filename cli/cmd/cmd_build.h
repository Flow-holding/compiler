#pragma once

/* flow build (prod=1, run=0, dev=0, fast=0)
   flow run   (prod=0, run=1, dev=0, fast=0)
   flow dev   (prod=0, run=0, dev=1, fast=0) — WASM senza .exe
   flow fast  (prod=0, run=0, dev=0, fast=1) — solo HTML/CSS/JS */
int cmd_build(int prod, int run_flag, int dev, int fast);
