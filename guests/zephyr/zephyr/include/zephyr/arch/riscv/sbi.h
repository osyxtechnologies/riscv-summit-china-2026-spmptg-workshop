/*
 * Copyright (c) 2026 Alexios Lyrakis <alexios.lyrakis@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief RISC-V Supervisor Binary Interface (SBI) definitions.
 *
 * Defines SBI extension IDs, function IDs, and error codes used by
 * S-mode code to request M-mode firmware services via the @c ecall
 * instruction.  The subset defined here covers only the extensions
 * used by Zephyr's in-tree minimal SBI runtime
 * (@c arch/riscv/core/sbi.S).
 *
 * References: RISC-V SBI Specification v2.0
 * (https://github.com/riscv-non-isa/riscv-sbi-doc)
 */

#ifndef ZEPHYR_ARCH_RISCV_INCLUDE_SBI_H_
#define ZEPHYR_ARCH_RISCV_INCLUDE_SBI_H_

/** @brief SBI extension ID for the Timer extension (TIME) */
#define SBI_EXT_TIME			0x54494D45

/** @brief SBI_EXT_TIME function ID: set the next timer deadline */
#define SBI_FUNC_SET_TIMER		0

/** @brief SBI extension ID for the System Reset extension (SRST) */
#define SBI_EXT_SRST			0x53525354

/** @brief SBI_EXT_SRST function ID: reset or power off the system */
#define SBI_FUNC_SYSTEM_RESET		0

/** @brief SBI_EXT_SRST reset type: clean shutdown (power off) */
#define SBI_SRST_RESET_TYPE_SHUTDOWN	0
/** @brief SBI_EXT_SRST reset type: cold reboot */
#define SBI_SRST_RESET_TYPE_COLD_REBOOT	1
/** @brief SBI_EXT_SRST reset type: warm reboot */
#define SBI_SRST_RESET_TYPE_WARM_REBOOT	2

/** @brief SBI_EXT_SRST reset reason: no specific reason */
#define SBI_SRST_RESET_REASON_NONE	0

/** @brief SBI return code: call completed successfully */
#define SBI_SUCCESS			0
/** @brief SBI return code: requested extension/function is not available */
#define SBI_ERR_NOT_SUPPORTED		-1

#ifndef _ASMLANGUAGE
#include <stdint.h>

/** @brief Return value from an SBI ecall: (error, value) pair. */
struct sbiret {
	long error;
	long value;
};

/**
 * @brief Issue an SBI ecall to the supervisor above the kernel.
 *
 * Used when CONFIG_RISCV_S_MODE_NATIVE_ENTRY is set and the kernel runs
 * under an external supervisor (hypervisor or M-mode firmware).
 *
 * @param ext         SBI extension ID
 * @param fid         Function ID within the extension
 * @param arg0..arg5  Function arguments (ABI: a0..a5)
 * @return            error / value pair returned by the supervisor
 */
struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5);
#endif /* !_ASMLANGUAGE */

#endif /* ZEPHYR_ARCH_RISCV_INCLUDE_SBI_H_ */
