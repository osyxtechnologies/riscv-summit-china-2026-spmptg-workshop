## SPDX-License-Identifier: LicenseRef-OSYX-Proprietary
## Copyright (c) 2025-2026 OSYX Technologies, Lda. All Rights Reserved.

## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.
##
## QEMU `virt` machine, RV32 variant, configured for SPMP-for-Hypervisor.
## Mirrors qemu-riscv32-virt but selects the MPU (SPMP) memory-protection
## backend and the AIA interrupt controller, matching the workshop's
## cva6-spmp platform so guest builds and Bao configurations can be smoke
## tested under QEMU before flashing the FPGA.

# Architecture definition
ARCH    := riscv
# CPU definition
CPU     :=
# Interrupt controller definition
IRQC    := AIA
# Core IPIs controller
IPIC    := IPIC_SBI

# Use SPMP-based memory protection (no MMU; matches the cva6-spmp model).
arch_mem_prot := mpu

drivers := sbi_uart

platform_description := virt_desc.c

platform-cppflags = -DIPIC=$(IPIC)
platform-cflags   =
platform-asflags  =
platform-ldflags  =

ARCH_SUB := riscv32
