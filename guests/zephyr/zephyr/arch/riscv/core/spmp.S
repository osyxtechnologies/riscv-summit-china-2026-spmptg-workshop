/*
 * Copyright (c) 2022 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/toolchain.h>
#include <zephyr/linker/sections.h>
#include <zephyr/arch/cpu.h>
#include "asm_macros.inc"

#define CSR_SISELECT		0x150
#define CSR_SIREG			0x151
#define CSR_SIREG2			0x152

#define CSR_SPMPEN		0x183
#define CSR_SPMPENH		0x193

#define SISELECT_BASE		0x100

/*
 * Prototype:
 *
 * void z_riscv_write_spmp_entries(unsigned int start,			// a0
 *                                unsigned int end,				// a1
 *                                uint16_t *spmp_cfg,			// a2
 *                                unsigned long *spmp_addr,		// a3
 *                                unsigned long *spmp_switch)	// a4
 *
 * Called from spmp.c to write a range of SPMP entries.
 *
 * SPMP registers are accessed with the csr instruction which only takes an
 * immediate value as the actual register. In order to avoid traversing
 * the whole register list, we use the start index to jump directly to the
 * location corresponding to the start of the wanted range. For this to work
 * we disallow compressed instructions so the update block sizes are easily
 * known (luckily they're all power-of-2's simplifying the code further).
 *
 * start < end && end <= CONFIG_SPMP_SLOTS must be true.
 */

GTEXT(z_riscv_write_spmp_entries)
SECTION_FUNC(TEXT, z_riscv_write_spmp_entries)

	la t0, spmp_store
	slli t1, a0, 5  /* 32-byte instruction blocks */
	add t0, t0, t1
	jr t0

spmp_store:

	.option push
	.option norvc
	.set _index, 0
	.rept CONFIG_SPMP_SLOTS
		li t2, (SISELECT_BASE + _index)
		csrw CSR_SISELECT, t2

		lr t0, (RV_REGSIZE * _index)(a3)
		csrw CSR_SIREG, t0

		lhu t0, (_index * 2)(a2)
		csrw CSR_SIREG2, t0

		li t0, _index + 1
		beq t0, a1, spmpswitch_store
		.set _index, _index + 1
	.endr
	.option pop

spmpswitch_store:

	.option push
	.option norvc
	lr t0, 0(a4)
	csrr t1, CSR_SPMPEN
	or t0, t0, t1
	csrw CSR_SPMPEN, t0
#ifndef CONFIG_64BIT
	lr t0, RV_REGSIZE(a4)
	csrr t1, CSR_SPMPENH
	or t0, t0, t1
	csrw CSR_SPMPENH, t0
#endif
	.option pop

done:
	ret
