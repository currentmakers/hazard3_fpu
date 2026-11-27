# Colorlight i5 (LFE5U-25F) top level for Hazard3 example SoC.
# All Hazard3 sources are referenced in place; nothing in ../../hazard3 is modified.

file fpga_colorlight_i5.v
file pll_25_35.v
file ../../hazard3/example_soc/libfpga/common/reset_sync.v
file ../../hazard3/example_soc/libfpga/common/fpga_reset.v

list ../../hazard3/example_soc/soc/soc.f
list ../../hazard3/hdl/debug/dtm/hazard3_ecp5_jtag_dtm.f
