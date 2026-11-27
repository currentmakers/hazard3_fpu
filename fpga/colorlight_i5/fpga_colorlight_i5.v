// Colorlight i5 (Lattice LFE5U-25F-6BG381C) top level for the Hazard3
// example SoC. Adapted from hazard3/example_soc/fpga/fpga_ulx3s.v
// (Copyright (C) 2021 Luke Wren, Apache-2.0).
// SPDX-License-Identifier: Apache-2.0

`default_nettype none

module fpga_colorlight_i5 (
	input  wire clk_osc,   // 25 MHz
	output wire led_n,

	output wire uart_tx,
	input  wire uart_rx
);

wire clk_sys;
wire pll_sys_locked;
wire rst_n_sys;

pll_25_35 pll_sys (
	.clkin   (clk_osc),
	.clkout0 (clk_sys),
	.locked  (pll_sys_locked)
);

fpga_reset #(
	.SHIFT (3)
) rstgen (
	.clk         (clk_sys),
	.force_rst_n (pll_sys_locked),
	.rst_n       (rst_n_sys)
);

example_soc #(
	.DTM_TYPE            ("ECP5"),
	// LFE5U-25F has 56 x 18 kbit EBRs (126 kB). Use 64 kB for the SRAM,
	// leaving EBRs for the register file, caches, FPU tables etc.
	.SRAM_DEPTH          (1 << 14),
	.CLK_MHZ             (35),

	// Baseline: RV32IMAC + Zicsr + Zifencei, the "before FPU" reference.
	.EXTENSION_M         (1),
	.EXTENSION_A         (1),
	.EXTENSION_C         (1),
	.EXTENSION_ZBA       (0),
	.EXTENSION_ZBB       (0),
	.EXTENSION_ZBC       (0),
	.EXTENSION_ZBS       (0),
	.EXTENSION_ZBKB      (0),
	.EXTENSION_ZIFENCEI  (1),
	.EXTENSION_XH3BEXTM  (0),
	.EXTENSION_XH3PMPM   (0),
	.EXTENSION_XH3POWER  (0),
	.CSR_COUNTER         (1),
	.MUL_FAST            (1),
	.MUL_FASTER          (0),
	.MULH_FAST           (0),
	.MULDIV_UNROLL       (1),
	.FAST_BRANCHCMP      (1),
	.BRANCH_PREDICTOR    (1)
) soc_u (
	.clk     (clk_sys),
	.rst_n   (rst_n_sys),

	// JTAG is provided internally through the ECP5 JTAGG primitive
	.tck     (1'b0),
	.trst_n  (1'b0),
	.tms     (1'b0),
	.tdi     (1'b0),
	.tdo     (/* unused */),

	.uart_tx (uart_tx),
	.uart_rx (uart_rx)
);

// Heartbeat: ~1.5 Hz blink proves the bitstream loaded and the PLL locked.
reg [24:0] heartbeat;
always @ (posedge clk_sys or negedge rst_n_sys)
	if (!rst_n_sys)
		heartbeat <= 25'd0;
	else
		heartbeat <= heartbeat + 25'd1;

assign led_n = !heartbeat[24];

endmodule
