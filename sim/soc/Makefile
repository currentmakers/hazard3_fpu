# Verilator simulation of the Hazard3 example SoC (Colorlight i5 config).
#   make                      build the simulator
#   make run HEX=file.hex     run a program (default: ../../sw/hello)

HAZARD3 := $(abspath ../../hazard3)
SOC_F   := $(HAZARD3)/example_soc/soc/soc.f
LISTF   := PROJ_ROOT=$(HAZARD3) HDL=$(HAZARD3)/hdl $(HAZARD3)/scripts/listfiles

SRCS    := $(shell $(LISTF) -f flat $(SOC_F))
INCDIRS := $(shell $(LISTF) -f flati $(SOC_F))

# Verilator 5.048 hits an internal error on `$display($time, "... %m ...")` in
# libfpga's sync_fifo.v. Build from a patched copy (in our build dir) so the
# hazard3 checkout stays untouched.
FIFO_ORIG := $(filter %/sync_fifo.v,$(SRCS))
FIFO_COPY := patched/sync_fifo.v
SRCS      := $(filter-out %/sync_fifo.v,$(SRCS)) $(FIFO_COPY)

HEX     ?= ../../sw/hello/build/hello.hex
CYCLES  ?= 2000000

VFLAGS  := --binary --timing -j 0 -O2 --top-module tb -Wno-fatal -Wno-lint -Wno-style \
           $(addprefix -I,$(INCDIRS)) -I$(HAZARD3)/hdl

.PHONY: all run clean
all: obj_dir/Vtb

$(FIFO_COPY): $(FIFO_ORIG)
	@mkdir -p $(dir $@)
	sed 's/%m/sync_fifo/' $< > $@

obj_dir/Vtb: tb.v $(SRCS)
	verilator $(VFLAGS) tb.v $(SRCS) > build.log 2>&1 || (tail -40 build.log; false)

run: obj_dir/Vtb
	./obj_dir/Vtb +hex=$(HEX) +cycles=$(CYCLES)

clean:
	rm -rf obj_dir build.log patched
