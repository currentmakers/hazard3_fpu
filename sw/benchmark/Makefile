# Benchmarks for Hazard3 + Zfinx on the Colorlight i5.
#
#   make          build all
#   make run      run all on the board (JTAG load, UART output); needs the
#                 FPGA configured, see README.md
#   make clean
#
# Or use the Makefile in each directory (make -C coremark run, ...).

BENCHMARKS := whetstone dhrystone coremark

.PHONY: all run clean $(BENCHMARKS)

all: $(BENCHMARKS)

$(BENCHMARKS):
	$(MAKE) -C $@

run:
	@for b in $(BENCHMARKS); do $(MAKE) --no-print-directory -s -C $$b run || exit 1; done

clean:
	@for b in $(BENCHMARKS); do $(MAKE) -C $$b clean; done
