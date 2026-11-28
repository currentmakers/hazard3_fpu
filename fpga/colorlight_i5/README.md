# Hazard3 + Zfinx FPU on Colorlight i5 (LFE5U-25F-6BG381C)

Builds the Hazard3 example SoC from the `hazard3` submodule (our fork,
branch `fpu`) for the Colorlight i5, with the Zfinx single-precision FPU.

    make bit      # synth + place&route + bitstream  (~4 min)
    make prog     # load to SRAM via the extension board's CMSIS-DAP (openFPGALoader)
    make flash    # write to SPI flash
    make clean

Configuration: RV32IMAC + Zicsr + Zifencei + Zfinx, counters, branch
predictor, fast multiply, 64 kB SRAM, UART, RISC-V timer, JTAG debug over
the ECP5 chip TAP (JTAGG). The LED on U16 blinks at about 1 Hz once the PLL
is locked.

Clock: 35 MHz (pll_25_35.v, generated with `ecppll -i 25 -o 35`). The core
was already limited to about 37-42 MHz on the -6 speed grade before the FPU
was added, so the 40/50 MHz used on ULX3S upstream does not close timing
here. A timing failure is a build error.

Place and route uses nextpnr's heap placer (see the Makefile): with
nextpnr 0.11 the SA placer hard-coded in Hazard3's synth_ecp5.mk fails to
place a carry chain in this design.

## Utilisation

| Resource          | Without FPU | With Zfinx FPU | Available |
|-------------------|-------------|----------------|-----------|
| Logic (LUT4/comb) | 6402 (26%)  | 9961 (41%)     | 24288     |
| Flip-flops        | 1751 (7%)   | 2436 (10%)     | 24288     |
| EBR (DP16KD)      | 32          | 32             | 56        |
| MULT18X18D        | 3           | 7              | 28        |
| Fmax (35 MHz target) | 37-42 MHz | 37.5 MHz (seed 1) |        |

## Tested hardware

Verified on an i5 module on the extension board with the STM32 DAPLink
probe (USB 0d28:0204 "DAPLink CMSIS-DAP"). The one USB cable provides:

- JTAG to the ECP5: `make prog` (openFPGALoader) and Hazard3 debug through
  OpenOCD (`openocd-colorlight-i5.cfg`, or `make load` in `sw/<app>`)
- the UART as `/dev/ttyACM0`, 115200 baud, both directions (J17 tx, H18 rx)

The user needs access to the probe's hidraw and ttyACM devices (on Debian,
membership of `plugdev` was enough).

Bring-up sequence:

    make -C fpga/colorlight_i5 prog        # bitstream to SRAM (lost at power-off)
    make -C sw/fpu_selftest load           # program to SRAM over JTAG, run from 0x40
    picocom -b 115200 /dev/ttyACM0         # or start this first to see all output

On hardware `fpu_selftest` reports `1000 vectors, 0 mismatches: PASS` with
the same latencies as in simulation (fadd/fmul 8, fmadd 9, fdiv/fsqrt 29,
fmin/feq 3 cycles). OpenOCD's "No working memory available" message during
`verify_image` is harmless (it falls back to reading the memory back).
