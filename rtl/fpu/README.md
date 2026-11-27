# hazard3_fpu: RV32F / Zfinx single-precision FPU

Standalone FP32 unit for Hazard3, written from scratch. It is IEEE 754 and
RISC-V compliant: all five rounding modes, subnormals in and out, canonical
NaN, all five exception flags, and tininess detected after rounding.

It computes one operation at a time over several cycles, behind the same
valid/ready/kill handshake as `hazard3_muldiv_seq`, so it can sit next to
the multiply/divide unit in Hazard3's stage X.

| File                    | Purpose                                                |
|-------------------------|--------------------------------------------------------|
| `hazard3_fpu.v`         | Top: state machine, FMA datapath, div/sqrt, converts, compares |
| `hazard3_fpu_unpack.v`  | Classify operands, normalise subnormals                |
| `hazard3_fpu_norm.v`    | Leading-zero count + normalising shift                 |
| `hazard3_fpu_round.v`   | Shared round-and-pack (overflow, gradual underflow)    |
| `hazard3_fpu_ops.vh`    | Operation, rounding-mode and flag encodings            |

## Operations and latency

Latency is counted in cycles after the cycle in which the operation is accepted.

| Operations                                         | Latency |
|----------------------------------------------------|---------|
| FSGNJ[N,X], FMIN/FMAX, FEQ/FLT/FLE, FCLASS         | 1       |
| FCVT.W.S, FCVT.WU.S                                | 2       |
| FCVT.S.W, FCVT.S.WU                                | 3       |
| FADD, FSUB, FMUL, FMADD, FMSUB, FNMSUB, FNMADD     | 6       |
| FDIV, FSQRT (radix-2, 1 bit per cycle)             | 27      |
| any op with NaN/inf/zero operands that decide the result | 1 |

FADD and FMUL run on the FMA datapath: FADD computes `a*1.0 + b`, and FMUL
computes `a*b + (±0)`.

## Interface

```
op_vld/op_rdy   start an operation (operands latched on accept)
op_kill         abandon the current operation; op_vld in the same cycle
                starts a new one at once (same as hazard3_muldiv_seq)
op[4:0]         see hazard3_fpu_ops.vh
rm[2:0]         resolved rounding mode (the core substitutes frm for DYN)
op_a/b/c[31:0]  operands (Zfinx: straight from integer registers)
result, fflags  {NV,DZ,OF,UF,NX}; valid while result_vld is high
```

## Size and speed (LFE5U-25F-6, Yosys + nextpnr, standalone)

3,855 logic cells (15%), 626 FFs, 4 MULT18X18D blocks, Fmax 38.8 MHz
(target 35 MHz). The slowest path is the special-case logic in the unpack stage.

## Verification

See `verif/fpu/`. Every result and every flag bit is checked against
Berkeley SoftFloat (RISC-V specialisation), using:

- TestFloat's corner-case generator: level 2 for every operation except
  FMA, which runs level 1 because level 2 would be billions of vectors.
  All five rounding modes, about 200M vectors in total; FMSUB, FNMSUB
  and FNMADD (sign variants of FMADD) are covered by the random tests.
- Random vectors biased towards hard cases: subnormals, NaN payloads,
  cancellation, results at the underflow/overflow boundaries, integer
  conversion limits, and random `op_kill`.
- Mutation checks: deliberately broken roundings and a dropped sticky bit
  are detected.
