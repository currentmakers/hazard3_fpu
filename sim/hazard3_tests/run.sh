#!/bin/bash
# Run Hazard3's own software regression (hazard3/test/sim/sw_testcases) on
# its Verilator testbench, using Debian's riscv64-unknown-elf toolchain.
#
#   sim/hazard3_tests/run.sh                 all tests, default config (with Zfinx)
#   sim/hazard3_tests/run.sh zfinx_golden    selected tests
#   CONFIG=min sim/hazard3_tests/run.sh hellow
#
# Workarounds handled here:
# - The tb_verilator Makefile forces a precompiled-header option that breaks
#   with current GCC/Verilator, so the Verilated model is built directly.
# - Debian's picolibc crt0 does not fit the testbench memory map, so tests
#   link with -nostartfiles and use the minimal _start in common/init.S.
#
# Known failure: amo_timer_irq (timing-sensitive IRQ count; fails the same
# way without the FPU, with this toolchain).

set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SIM="$ROOT/hazard3/test/sim"
CONFIG="${CONFIG:-default}"
MARCH="${MARCH:-rv32ima_zca_zcb_zcmp_zicsr_zifencei_zba_zbb_zbkb_zbs}"
TB=tb
[ "$CONFIG" != default ] && TB="tb-$CONFIG"

cd "$SIM/tb_verilator"
echo ">>> Building tb_verilator (CONFIG=$CONFIG)"
# Build dir is shared between configs, so always rebuild from scratch
rm -rf build-tb "$TB"
make -s CONFIG="$CONFIG" vcc >/dev/null
make -s -j"$(nproc)" VERILATOR_ROOT="$(verilator --getenv VERILATOR_ROOT)" \
	-C build-tb/obj_dir -f Vtb.mk >/dev/null
touch build-tb/vlib.touch
make -s CONFIG="$CONFIG" >/dev/null

cd "$SIM/sw_testcases"
echo ">>> Running sw_testcases"
CROSS_PREFIX=riscv64-unknown-elf- EXTRA_CCFLAGS=-nostartfiles \
	./runtests --tb="../tb_verilator/$TB" --march="$MARCH" --no-tb-build \
	--build-dir="deb-$CONFIG" "$@"
