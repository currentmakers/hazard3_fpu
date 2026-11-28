# Dhrystone on Hazard3 (Colorlight i5).
#
#   make          build
#   make run      load over JTAG, run, show the UART output (FPGA must be configured)
#
# The Dhrystone sources are used unmodified from the Hazard3 submodule (its
# copy of riscv-tests' Dhrystone); main.c is the board glue. Integer-only, so
# the FPU makes no difference. Flags follow Hazard3's own build (-O3, no
# inlining, as the Dhrystone rules require); strcpy/strcmp come from picolibc.

DHRY      := ../../../hazard3/test/sim/dhrystone/src
COMMON    := ../../common

APP        = dhrystone
SRCS       = main.c dhrystone_main_wrap.c $(DHRY)/dhrystone.c $(COMMON)/tiny_printf.c
OPT        = -O3 -fno-inline
EXTRA_CFLAGS = -I$(DHRY) -std=gnu99 \
               -Wno-implicit-int -Wno-implicit-function-declaration -Wno-return-type \
               -Wno-missing-parameter-type -Wno-old-style-definition -Wno-unused-parameter
EXTRA_DEPS = $(DHRY)/dhrystone.h $(DHRY)/dhrystone_main.c
LIBS       = -lc

include $(COMMON)/app.mk
