# Hazard3 example SoC on Colorlight i5 (LFE5U-25F-6BG381C)

Builds the unmodified Hazard3 example SoC (in `../../hazard3`) for the
Colorlight i5. Hazard3 sources are referenced in place; nothing in the
hazard3 checkout is changed or written to.

    make bit      # synth + place&route + bitstream  (~3 min)
    make prog     # load to SRAM via the extension board's CMSIS-DAP (openFPGALoader)
    make flash    # write to SPI flash
    make clean

Configuration: RV32IMAC + Zicsr + Zifencei, counters, branch predictor,
fast multiply, 64 kB SRAM, UART, RISC-V timer, JTAG debug over the ECP5
chip TAP (JTAGG). LED on U16 blinks ~1 Hz when the PLL is locked.

Clock: 35 MHz (pll_25_35.v, generated with `ecppll -i 25 -o 35`).
The core reaches ~37-42 MHz on the -6 speed grade depending on placement
seed, so 40/50 MHz (as used on ULX3S upstream) does not close timing here.
Timing failure is a build error (no `--timing-allow-fail`).

## Baseline utilisation (before FPU)

| Resource          | Used  | Available | %   |
|-------------------|-------|-----------|-----|
| Logic (LUT4/comb) | 6402  | 24288     | 26% |
| Flip-flops        | 1751  | 24288     | 7%  |
| EBR (DP16KD)      | 32    | 56        | 57% |
| MULT18X18D        | 3     | 28        | 10% |

UART pins (J17 tx / H18 rx) are from the LiteX colorlight_i5 definition;
verify against your extension board revision.
