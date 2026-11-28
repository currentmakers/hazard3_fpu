# Benchmarks: Hazard3 + Zfinx FPU on the Colorlight i5

| Directory                  | Benchmark                         | Measures                    |
|----------------------------|-----------------------------------|-----------------------------|
| [whetstone](whetstone/)    | Whetstone, single precision       | floating point, with and without the FPU |
| [dhrystone](dhrystone/)    | Dhrystone 2.x                     | integer (DMIPS)             |
| [coremark](coremark/)      | EEMBC CoreMark 1.0                | integer (CoreMark)          |

    make          # build all
    make run      # run all on the board, one after the other (about 1.5 min)
    make -C coremark run      # or one of them

`run` loads the program into SRAM over JTAG (DAPLink CMSIS-DAP), starts it
and prints the UART output (`/dev/ttyACM0`, override with `TTY=`) until the
program says "Done". The FPGA must be configured first:
`make -C ../../fpga/colorlight_i5 prog` (or the bitstream in flash).

## Results (2026-11-28, 35 MHz)

Core configuration as in `fpga/colorlight_i5`: RV32IMAC + Zicsr + Zifencei +
Zfinx, fast multiplier (`MUL_FAST`), branch predictor, 64 kB SRAM on the
example SoC's single AHB port. GCC 15.3 (Debian riscv64-unknown-elf).

| Benchmark                    | Result          | Per MHz            |
|------------------------------|-----------------|--------------------|
| Whetstone, with FPU          | 15.67 MWIPS     | 0.448 MWIPS/MHz    |
| Whetstone, soft-float        | 1.99 MWIPS      | 0.057 MWIPS/MHz    |
| Dhrystone                    | 34.94 DMIPS     | 0.998 DMIPS/MHz    |
| CoreMark                     | 112.29          | 3.208 CoreMark/MHz |

Dhrystone and CoreMark are integer benchmarks; the FPU does not affect them.
For comparison, Hazard3's README quotes 4.15 CoreMark/MHz for the RP2350
configuration, which additionally has Zba/Zbb/Zbs/Zbkb and faster multiply
options and was measured in simulation. Building CoreMark here without the C
extension gives 3.08 CoreMark/MHz (more code, 62 of 64 kB), so compressed code
is not the cause of the difference.

## Notes

- **Sources.** CoreMark and Dhrystone are used unmodified from the Hazard3
  submodule (`hazard3/test/sim/coremark`, `hazard3/test/sim/dhrystone`); only
  the board ports live here. CoreMark: Apache-2.0 (EEMBC), port adapted from
  Hazard3's `barebones` port. Dhrystone: the riscv-tests copy (BSD).
- **CoreMark** runs a fixed 2000 iterations (`make ITERATIONS=n`), about
  18 s; a valid CoreMark needs at least 10 s. The CRCs are checked by CoreMark
  itself ("Correct operation validated").
- **Dhrystone** follows Hazard3's flags (`-O3 -fno-inline`); `strcpy`/`strcmp`
  are picolibc's. It times 500 runs with `mcycle` (cycle-exact, so no longer
  run is needed). Its own output prints cycles as "Microseconds" and runs per
  million cycles as "per Second" (that is how the riscv-tests port uses
  `mcycle`); the summary at the end converts to real units and checks the
  final values. 1 DMIPS = 1757 Dhrystones/s.
- **Whetstone**: see [whetstone/README.md](whetstone/README.md).
- Each program waits 0.3 s before printing: the DAPLink sometimes drops UART
  data while OpenOCD is still talking to it after starting the program.
- Shared helpers in `sw/common`: `tiny_printf.c` (printf subset with `%f`) and
  the `run` target in `app.mk`.
