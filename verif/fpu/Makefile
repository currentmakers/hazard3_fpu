# FPU verification: Verilator testbench vs Berkeley SoftFloat (RISC-V
# specialisation), with TestFloat-generated corner-case input vectors.
#
#   make                       build everything
#   make test                  random regression (all ops, all rounding modes)
#   make test N=10000000       longer run
#   make testfloat             TestFloat corner-case inputs (~200M vectors, ~2 min)
#
# SoftFloat/TestFloat are built out-of-tree in build/, so third_party/ stays clean.

SHELL  := /bin/bash
ROOT   := $(abspath ../..)
SF     := $(ROOT)/third_party/berkeley-softfloat-3
TF     := $(ROOT)/third_party/berkeley-testfloat-3
RTL    := $(ROOT)/hazard3/hdl/fpu
B      := $(abspath build)

N      ?= 1000000
SEED   ?= 1

RTL_SRCS := $(RTL)/hazard3_fpu.v $(RTL)/hazard3_fpu_unpack.v \
            $(RTL)/hazard3_fpu_norm.v $(RTL)/hazard3_fpu_round.v

SF_LIB := $(B)/softfloat/softfloat.a
TF_GEN := $(B)/testfloat/testfloat_gen
VTB    := $(B)/obj_dir/Vhazard3_fpu

.PHONY: all test testfloat clean
all: $(VTB) $(TF_GEN)

$(SF_LIB):
	mkdir -p $(B)/softfloat
	cp $(SF)/build/Linux-x86_64-GCC/platform.h $(B)/softfloat/
	$(MAKE) -C $(B)/softfloat -f $(SF)/build/Linux-x86_64-GCC/Makefile \
		SOURCE_DIR=$(SF)/source SPECIALIZE_TYPE=RISCV softfloat.a >/dev/null

$(TF_GEN): $(SF_LIB)
	mkdir -p $(B)/testfloat
	cp $(TF)/build/Linux-x86_64-GCC/platform.h $(B)/testfloat/
	$(MAKE) -C $(B)/testfloat -f $(TF)/build/Linux-x86_64-GCC/Makefile \
		SOURCE_DIR=$(TF)/source SOFTFLOAT_DIR=$(SF) SOFTFLOAT_LIB=$(SF_LIB) \
		testfloat_gen >/dev/null

$(VTB): tb_fpu.cpp $(RTL_SRCS) $(RTL)/hazard3_fpu_ops.vh $(SF_LIB)
	mkdir -p $(B)
	verilator --cc --exe --build -j 0 -O3 -Wno-fatal \
		--top-module hazard3_fpu -I$(RTL) --Mdir $(B)/obj_dir \
		-CFLAGS "-O2 -I$(SF)/source/include" \
		$(RTL_SRCS) $(abspath tb_fpu.cpp) $(SF_LIB) >$(B)/verilator.log 2>&1 \
		|| (tail -30 $(B)/verilator.log; false)

test: $(VTB)
	$(VTB) random all $(N) $(SEED)

# TestFloat generator inputs; expected results come from our own SoftFloat
# calls in the testbench, so only the input columns are used.
# Entries are generator:op:level:modes. mulAdd level 2 would be billions of
# vectors, so FMA runs level 1; everything else level 2. Compares do not
# depend on the rounding mode.
TF_RUNS := f32_add:add:2:01234 f32_sub:sub:2:01234 f32_mul:mul:2:01234 \
           f32_mulAdd:madd:1:01234 f32_div:div:2:01234 f32_sqrt:sqrt:2:01234 \
           f32_to_i32:cvt_w_s:2:01234 f32_to_ui32:cvt_wu_s:2:01234 \
           i32_to_f32:cvt_s_w:2:01234 ui32_to_f32:cvt_s_wu:2:01234 \
           f32_eq:eq:2:0 f32_lt:lt:2:0 f32_le:le:2:0

testfloat: $(VTB) $(TF_GEN)
	@set -o pipefail; fail=0; for run in $(TF_RUNS); do \
		IFS=: read gen op lvl modes <<< "$$run"; \
		for mode in $$(echo $$modes | fold -w1); do \
			printf "%-12s level %s rm=%s  " $$gen $$lvl $$mode; \
			$(TF_GEN) -level $$lvl $$gen | $(VTB) stdin $$op $$mode | tail -1 || fail=1; \
		done; \
	done; exit $$fail

clean:
	rm -rf build
