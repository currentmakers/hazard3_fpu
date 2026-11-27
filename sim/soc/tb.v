// Simulation testbench: Hazard3 example SoC as configured for Colorlight i5,
// SRAM preloaded from a hex file, UART TX decoded and printed to stdout.
`timescale 1ns/1ps
`default_nettype none

module tb;

parameter CLK_MHZ = 35;
parameter BAUD    = 115200;

reg clk = 1'b0;
reg rst_n = 1'b0;
always #(500.0 / CLK_MHZ) clk = !clk;

wire uart_tx;
reg  uart_rx = 1'b1;

example_soc #(
	.DTM_TYPE            ("JTAG"),     // ECP5 JTAGG has no sim model
	.SRAM_DEPTH          (1 << 14),
	.CLK_MHZ             (CLK_MHZ),
	.EXTENSION_M         (1),
	.EXTENSION_A         (1),
	.EXTENSION_C         (1),
	.EXTENSION_ZIFENCEI  (1),
	.CSR_COUNTER         (1),
	.MUL_FAST            (1),
	.MULDIV_UNROLL       (1),
	.FAST_BRANCHCMP      (1),
	.BRANCH_PREDICTOR    (1)
) soc_u (
	.clk     (clk),
	.rst_n   (rst_n),
	.tck     (1'b0),
	.trst_n  (1'b0),
	.tms     (1'b0),
	.tdi     (1'b0),
	.tdo     (),
	.uart_tx (uart_tx),
	.uart_rx (uart_rx)
);

// ---------------------------------------------------------------------------
// Preload, reset, timeout

string hexfile;
longint max_cycles;
longint cycle = 0;

initial begin
	if (!$value$plusargs("hex=%s", hexfile)) begin
		$display("usage: +hex=<file> [+cycles=N]");
		$finish;
	end
	if (!$value$plusargs("cycles=%d", max_cycles))
		max_cycles = 2000000;
	$readmemh(hexfile, soc_u.sram0.sram.behav_mem.mem);
	repeat (10) @(posedge clk);
	rst_n = 1'b1;
end

always @ (posedge clk) begin
	cycle <= cycle + 1;
	if (cycle >= max_cycles) begin
		$display("\n[tb] stopped after %0d cycles", cycle);
		$finish;
	end
end

// ---------------------------------------------------------------------------
// UART TX decoder (8N1). Sample mid-bit using real time.

localparam real BIT_NS = 1.0e9 / BAUD;

reg [7:0] rx_byte;
integer   b;
initial begin
	forever begin
		@(negedge uart_tx);
		#(BIT_NS * 1.5);
		for (b = 0; b < 8; b = b + 1) begin
			rx_byte[b] = uart_tx;
			#(BIT_NS);
		end
		if (!uart_tx)
			$display("\n[tb] UART framing error");
		else if (rx_byte != 8'h0d)
			$write("%c", rx_byte);
		$fflush;
	end
end

endmodule
