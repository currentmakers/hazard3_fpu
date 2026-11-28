# Whetstone (single precision) on Hazard3, with and without the FPU.
#
#   make             build both target variants (and the host reference)
#   make run-fpu     load whetstone_fpu over JTAG, run it, show the UART output
#   make run-soft    same for whetstone_soft
#   make run         both, one after the other
#   make host        run the benchmark on the build machine
#
# whetstone_fpu   -march=rv32imac_zicsr_zifencei_zfinx: float ops on the FPU
# whetstone_soft  -march=rv32imac_zicsr_zifencei: float ops via libgcc soft-float
#
# run-* need the FPGA configured (make -C ../../../fpga/colorlight_i5 prog) and
# the DAPLink UART (TTY=/dev/ttyACM0 by default, see ../../common/app.mk).

VARIANTS  := fpu soft
ARCH_fpu  := rv32imac_zicsr_zifencei_zfinx
ARCH_soft := rv32imac_zicsr_zifencei
SRCS      := whetstone.c fmath.c
FLAGS     := -O2 -ffp-contract=off

HOST_CC   ?= gcc
REF       := build/whetstone_ref.h

APP_MAKE = $(MAKE) --no-print-directory -f ../../common/app.mk APP=whetstone_$(1) \
	BUILD=build/$(1) ARCH=$(ARCH_$(1)) SRCS="$(SRCS)" OPT=-O2 \
	EXTRA_CFLAGS="-ffp-contract=off -Ibuild" EXTRA_DEPS="fmath.h $(REF)"

.PHONY: all $(VARIANTS) $(addprefix run-,$(VARIANTS)) run host clean

all: $(VARIANTS)

$(VARIANTS): $(REF)
	@$(call APP_MAKE,$@)

# Bit patterns of every module's results, from the host build. Correctly
# rounded IEEE single precision gives the same bits everywhere.
build/whetstone_host: $(SRCS) fmath.h Makefile
	@mkdir -p build
	$(HOST_CC) $(FLAGS) -std=gnu11 -Wall -Wextra -DHOST $(SRCS) -o $@

$(REF): build/whetstone_host
	$< --ref > $@

host: build/whetstone_host
	$<

$(addprefix run-,$(VARIANTS)): run-%: %
	@$(call APP_MAKE,$*) run

run: run-fpu run-soft

clean:
	rm -rf build
