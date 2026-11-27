# Colorlight i5 build for the Hazard3 example SoC (with the Zfinx FPU).
# Reuses Hazard3's ECP5 synthesis flow (hazard3/scripts/synth_ecp5.mk).

include ../../hazard3/project_paths.mk

CHIPNAME      = fpga_colorlight_i5
TOP           = fpga_colorlight_i5
DOTF          = fpga_colorlight_i5.f

SYNTH_OPT     = -abc9
PNR_OPT       =

DEVICE        = 25k
PACKAGE       = CABGA381
DEVICE_IDCODE = 0x41111043

PNR_SEED     ?= 1

include $(SCRIPTS)/synth_ecp5.mk

# Override the place-and-route rule from synth_ecp5.mk: its simulated-annealing
# placer ("--placer sa") fails to place a carry chain in this design with
# nextpnr 0.11, so use the analytic placer with a fixed seed. Timing failure
# is a build error. (Make warns about the overridden recipe; that is expected.)
$(CHIPNAME).config: $(CHIPNAME).json $(CHIPNAME).lpf
	@echo ">>> Place and Route"
	@echo
	$(NEXTPNR) --placer heap --seed $(PNR_SEED) --$(DEVICE) --package $(PACKAGE) --lpf $(CHIPNAME).lpf --json $(CHIPNAME).json --textcfg $@ $(PNR_OPT) --quiet --log pnr.log
	@grep "Info: Max frequency for clock " pnr.log | tail -n 1

# Needs openFPGALoader; the i5 extension board has a CMSIS-DAP probe.
prog: bit
	openFPGALoader -c cmsisdap $(CHIPNAME).bit

flash: bit
	openFPGALoader -c cmsisdap -f $(CHIPNAME).bit

clean::
	rm -f $(CHIPNAME).config $(CHIPNAME).svf try_*.config try_*.log
