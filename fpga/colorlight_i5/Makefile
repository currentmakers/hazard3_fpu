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

# Program baked into the SRAM of the bitstream (runs from reset): the name of
# a directory under ../../sw, or "none" for zero-filled SRAM.
BOOT_APP     ?= fpu_selftest
SRAM_WORDS   := 16384
SW_DIR       := ../../sw

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

# SRAM initialisation. Synthesis always uses the same random placeholder
# contents; the bitstream step swaps in the boot program with ecpbram, so a
# software change only reruns ecpbram + ecppack (seconds), not synth/PnR.
sram_random.hex:
	ecpbram -g $@ -w 32 -d $(SRAM_WORDS) -s 1

$(CHIPNAME).json: sram_random.hex

.PHONY: boot_app
boot_app:
ifneq ($(BOOT_APP),none)
	$(MAKE) -C $(SW_DIR)/$(BOOT_APP)
endif

# Padded to the full SRAM depth; only touched when the contents change
boot.hex: boot_app
ifeq ($(BOOT_APP),none)
	@yes 00000000 | head -n $(SRAM_WORDS) > $@.tmp
else
	@n=$$(wc -l < $(SW_DIR)/$(BOOT_APP)/build/$(BOOT_APP).hex); \
	if [ $$n -gt $(SRAM_WORDS) ]; then echo "$(BOOT_APP) too large for SRAM"; exit 1; fi; \
	{ cat $(SW_DIR)/$(BOOT_APP)/build/$(BOOT_APP).hex; yes 00000000 | head -n $$(($(SRAM_WORDS) - n)); } > $@.tmp
endif
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; echo "boot.hex: $(BOOT_APP)"; fi

# Override the bitstream rule from synth_ecp5.mk to insert the boot program
$(CHIPNAME).bit: $(CHIPNAME).config boot.hex sram_random.hex
	@echo ">>> Insert boot program ($(BOOT_APP)) and generate bitstream"
	ecpbram -i $(CHIPNAME).config -o boot.config -f sram_random.hex -t boot.hex
	ecppack --compress --svf $(CHIPNAME).svf --idcode $(DEVICE_IDCODE) boot.config $@

# Needs openFPGALoader; the i5 extension board has a CMSIS-DAP probe.
prog: bit
	openFPGALoader -c cmsisdap $(CHIPNAME).bit

flash: bit
	openFPGALoader -c cmsisdap -f $(CHIPNAME).bit

clean::
	rm -f $(CHIPNAME).config $(CHIPNAME).svf try_*.config try_*.log
	rm -f sram_random.hex boot.hex boot.hex.tmp
