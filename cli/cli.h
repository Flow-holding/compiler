#pragma once

#define FLOW_VERSION "0.2.4"
#define FLOW_REPO    "Flow-holding/compiler"

#define C_RED     "\x1b[31m"
#define C_GREEN   "\x1b[32m"
#define C_YELLOW  "\x1b[33m"
#define C_CYAN    "\x1b[36m"
#define C_MAGENTA "\x1b[95m"
#define C_FLOW    "\x1b[38;2;255;0;135m"  /* #ff0087 */
#define C_RESET   "\x1b[0m"

#ifdef _WIN32
  #define EXE_EXT ".exe"
#else
  #define EXE_EXT ""
#endif
