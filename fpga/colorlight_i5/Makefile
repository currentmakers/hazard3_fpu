# Colorlight i5 build for the Hazard3 example SoC.
# Reuses Hazard3's ECP5 synthesis flow without modifying the hazard3 checkout.

include ../../hazard3/project_paths.mk

CHIPNAME      = fpga_colorlight_i5
TOP           = fpga_colorlight_i5
DOTF          = fpga_colorlight_i5.f

SYNTH_OPT     = -abc9
PNR_OPT       =

DEVICE        = 25k
PACKAGE       = CABGA381
DEVICE_IDCODE = 0x41111043

include $(SCRIPTS)/synth_ecp5.mk

# Needs openFPGALoader; the i5 extension board has a CMSIS-DAP probe.
prog: bit
	openFPGALoader -c cmsisdap $(CHIPNAME).bit

flash: bit
	openFPGALoader -c cmsisdap -f $(CHIPNAME).bit

clean::
	rm -f $(CHIPNAME).config $(CHIPNAME).svf
