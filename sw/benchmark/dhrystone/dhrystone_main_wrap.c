// Compile Hazard3's (riscv-tests') dhrystone_main.c unmodified, with its
// main() renamed so that main.c can wrap it.
#define main dhrystone_main
#include "dhrystone_main.c"
