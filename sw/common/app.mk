# Common build rules for bare-metal apps on the Hazard3 example SoC.
# Usage (in an app Makefile):  APP = name; SRCS = main.c; include ../common/app.mk
#
# ARCH=rv32imac_zicsr_zifencei_zfinx (default: hardware FP32 in integer regs)
# ARCH=rv32imac_zicsr_zifencei       (soft-float, for a core without the FPU)

CROSS_PREFIX ?= riscv64-unknown-elf-
ARCH         ?= rv32imac_zicsr_zifencei_zfinx
ABI          ?= ilp32
CLK_HZ       ?= 35000000
OPT          ?= -Os
# Also available: EXTRA_CFLAGS, EXTRA_DEPS (extra prerequisites of the ELF)

COMMON  := $(dir $(lastword $(MAKEFILE_LIST)))
BUILD   ?= build

CFLAGS  += -march=$(ARCH) -mabi=$(ABI) $(OPT) -g -Wall -Wextra \
           -ffreestanding -fno-builtin -I$(COMMON) -DCLK_HZ=$(CLK_HZ)u $(EXTRA_CFLAGS)
LDFLAGS += -nostartfiles -nostdlib -T $(COMMON)soc.ld -Wl,--gc-sections \
           -Wl,--no-warn-rwx-segments -Wl,-Map=$(BUILD)/$(APP).map
LIBS    += -lgcc

OPENOCD_CFG ?= $(COMMON)../../fpga/colorlight_i5/openocd-colorlight-i5.cfg

.PHONY: all clean load
all: $(BUILD)/$(APP).elf $(BUILD)/$(APP).bin $(BUILD)/$(APP).dis

$(BUILD)/$(APP).elf: $(COMMON)crt0.S $(SRCS) $(wildcard $(COMMON)*.h) $(COMMON)soc.ld $(COMMON)app.mk Makefile $(EXTRA_DEPS)
	@mkdir -p $(BUILD)
	$(CROSS_PREFIX)gcc $(CFLAGS) $(LDFLAGS) $(COMMON)crt0.S $(SRCS) $(LIBS) -o $@
	$(CROSS_PREFIX)size $@

$(BUILD)/$(APP).bin: $(BUILD)/$(APP).elf
	$(CROSS_PREFIX)objcopy -O binary $< $@

$(BUILD)/$(APP).dis: $(BUILD)/$(APP).elf
	$(CROSS_PREFIX)objdump -d $< > $@

# Load into SRAM over JTAG (FPGA must already be configured) and run from reset vector
load: $(BUILD)/$(APP).elf
	openocd -f $(OPENOCD_CFG) -c "load_image $< 0x0 elf" \
		-c "catch {verify_image $< 0x0 elf}" -c "resume 0x40" -c shutdown

# Load and run, showing the UART output until a line starting with "Done"
TTY      ?= /dev/ttyACM0
RUN_SECS ?= 120
.PHONY: run
run: $(BUILD)/$(APP).elf
	@stty -F $(TTY) 115200 raw -echo
	@timeout $(RUN_SECS) sed -u '/^Done/q' $(TTY) & \
	 sleep 0.5; \
	 openocd -f $(OPENOCD_CFG) -c "load_image $< 0x0 elf" \
		-c "catch {verify_image $< 0x0 elf}" -c "resume 0x40" -c shutdown > $(BUILD)/load.log 2>&1 \
	   || { cat $(BUILD)/load.log; kill $$!; exit 1; }; \
	 wait

clean:
	rm -rf $(BUILD)

# Word-per-line hex for $readmemh (simulation preload)
all: $(BUILD)/$(APP).hex
$(BUILD)/$(APP).hex: $(BUILD)/$(APP).bin
	od -An -v -tx4 -w4 $< | tr -d ' ' > $@
