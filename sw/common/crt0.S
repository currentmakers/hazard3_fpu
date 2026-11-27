// Minimal startup for the Hazard3 example SoC (no libc init).
.section .vectors, "ax"
.option push
.option norvc
.global _trap
_trap:                       // 0x00: mtvec (direct mode), all traps land here
	csrr a0, mcause
	csrr a1, mepc
	j    trap_handler
.balign 0x40
.global _reset
_reset:                      // 0x40: Hazard3 reset vector in example_soc
.option push
.option norelax
	la   gp, __global_pointer$
.option pop
	la   sp, __stack_top
	la   t0, __bss_start
	la   t1, __bss_end
1:	bgeu t0, t1, 2f
	sw   zero, 0(t0)
	addi t0, t0, 4
	j    1b
2:	call main
3:	wfi
	j    3b
.option pop

.text
.weak trap_handler
trap_handler:
	j trap_handler
