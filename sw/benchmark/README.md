# Whetstone benchmark: Hazard3 with and without the Zfinx FPU

The classic Curnow & Wichmann Whetstone (1976), in **single precision** (the
FPU is FP32), built from one source in two variants:

| Build            | `-march`                         | float arithmetic              |
|------------------|----------------------------------|-------------------------------|
| `whetstone_fpu`  | `rv32imac_zicsr_zifencei_zfinx`  | FPU instructions              |
| `whetstone_soft` | `rv32imac_zicsr_zifencei`        | libgcc soft-float calls       |

Both run on the same bitstream (the soft build simply uses no FP instructions),
so the difference is purely the FPU.

    make             # build both (and the host reference)
    make run         # load + run both over JTAG, print the UART output
    make run-fpu     # or one of them
    make host        # run on the build machine

`make run*` needs the FPGA configured (`make -C ../../fpga/colorlight_i5 prog`,
or the bitstream in flash) and the DAPLink UART on `/dev/ttyACM0` (`TTY=`).

## Results (Colorlight i5, 35 MHz, 2026-11-28)

| Build            | MWIPS  | MWIPS/MHz |
|------------------|--------|-----------|
| `whetstone_fpu`  | 15.67  | 0.448     |
| `whetstone_soft` | 1.99   | 0.057     |

The FPU makes Whetstone **7.9x** faster. Time share per module (FPU / soft):
trig functions 32% / 35%, procedure calls 32% / 29%, standard functions
23% / 27%, array as parameter 6% / 7%, array references 7% / 1%.

## Notes

- **Own math functions.** The toolchain's libm is soft-float only; linking it
  would run sin/cos/atan/exp/log on soft-float even in the FPU build. So
  `fmath.c` implements them in plain C and gets compiled with each variant's
  `-march` (accuracy within 2 ulp; `f_sqrt` is `fsqrt.s` with the FPU and a
  correctly rounded integer routine without). Published Whetstone figures for
  other systems use their libm, so compare those with care.
- **Single precision.** Standard Whetstone is double precision; published
  MWIPS figures are usually DP and not directly comparable.
- **Result check.** Built with `-ffp-contract=off` (no fused multiply-add), all
  operations are correctly rounded IEEE single precision in a fixed order, so
  the FPU build, the soft build and the host build give bit-identical module
  results. The host build generates `build/whetstone_ref.h` and both target
  builds check against it (the "PASS" line). Allowing contraction would let GCC
  use `fmadd.s` and change the low bits of the results.
- Modules 1, 5 and 10 have zero iterations in the original and are left out.
  The procedure-call module's functions are `noinline`, as intended.
- Timing uses `mcycle`; the run length adapts to at least 2 seconds.
