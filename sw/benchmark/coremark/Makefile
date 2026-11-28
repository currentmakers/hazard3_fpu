# EEMBC CoreMark on Hazard3 (Colorlight i5).
#
#   make          build
#   make run      load over JTAG, run, show the UART output (FPGA must be configured)
#
# The CoreMark sources are used in place from the Hazard3 submodule; only the
# port (core_portme.*) lives here. CoreMark is integer-only, so the FPU makes no
# difference. A valid result needs at least 10 s of run time.

CM        := ../../../hazard3/test/sim/coremark/dist
COMMON    := ../../common

APP        = coremark
ITERATIONS ?= 2000
SRCS       = $(CM)/core_list_join.c $(CM)/core_main.c $(CM)/core_matrix.c \
             $(CM)/core_state.c $(CM)/core_util.c core_portme.c $(COMMON)/tiny_printf.c

# Optimisation flags as in Hazard3's own CoreMark build (core_portme.mak)
PORT_CFLAGS := -O3 -mbranch-cost=1 -funroll-all-loops --param max-inline-insns-auto=200 \
               -finline-limit=10000 -fno-code-hoisting -fno-if-conversion2 \
               -falign-functions=4 -falign-jumps=4 -falign-loops=4
OPT          = $(PORT_CFLAGS)
EXTRA_CFLAGS = -I. -I$(CM) -DITERATIONS=$(ITERATIONS) \
               -DFLAGS_STR='"$(PORT_CFLAGS) -march=$(ARCH)"' -Wno-unused-parameter
EXTRA_DEPS   = core_portme.h $(CM)/coremark.h

include $(COMMON)/app.mk
