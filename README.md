# hazard3_fpu: Hazard3 + FP32 (Zfinx) on Colorlight i5

`hazard3/` is a git submodule of upstream [Hazard3](https://github.com/Wren6991/Hazard3)
(Apache-2.0), kept unmodified: everything else references its files in place.

| Dir                    | What                                                         |
|------------------------|--------------------------------------------------------------|
| `fpga/colorlight_i5/`  | Bitstream build for Colorlight i5 (LFE5U-25F), 35 MHz        |
| `sw/`                  | Bare-metal software: `common/` (crt0, linker script, UART), apps |
| `sim/soc/`             | Verilator sim of the whole SoC, decodes UART output           |

Toolchain: Debian `gcc-riscv64-unknown-elf` (builds RV32 with
`-march=rv32... -mabi=ilp32`), picolibc, Yosys, nextpnr-ecp5, Verilator,
openFPGALoader, OpenOCD.

Clone with the submodule:

    git clone --recurse-submodules git@github.com:currentmakers/hazard3_fpu.git
    # or, after a plain clone: git submodule update --init

Quick start:

    make -C sw/hello                         # build hello (soft-float)
    make -C sim/soc run                      # run it in simulation
    make -C fpga/colorlight_i5 bit prog      # build + load bitstream
    make -C sw/hello load                    # load + run over JTAG
    # UART: 115200 8N1 on the extension board's USB serial
