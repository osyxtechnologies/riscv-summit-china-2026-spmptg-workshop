/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * RISC-V SBI client wrappers.
 *
 * Used when CONFIG_RISCV_S_MODE_NATIVE_ENTRY is set: the kernel runs in
 * S-mode under an external supervisor (a hypervisor or M-mode firmware)
 * and issues ecalls upward to request services.
 */

#include <zephyr/arch/riscv/sbi.h>

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5)
{
	struct sbiret ret;

	register unsigned long a0 __asm__("a0") = arg0;
	register unsigned long a1 __asm__("a1") = arg1;
	register unsigned long a2 __asm__("a2") = arg2;
	register unsigned long a3 __asm__("a3") = arg3;
	register unsigned long a4 __asm__("a4") = arg4;
	register unsigned long a5 __asm__("a5") = arg5;
	register unsigned long a6 __asm__("a6") = (unsigned long)fid;
	register unsigned long a7 __asm__("a7") = (unsigned long)ext;

	__asm__ volatile("ecall"
			 : "+r"(a0), "+r"(a1)
			 : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
			 : "memory");

	ret.error = a0;
	ret.value = a1;
	return ret;
}
