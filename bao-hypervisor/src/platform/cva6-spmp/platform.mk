## SPDX-License-Identifier: LicenseRef-OSYX-Proprietary
## Copyright (c) 2025-2026 OSYX Technologies, Lda. All Rights Reserved.

## SPDX-License-Identifier: Apache-2.0
## Copyright (c) Bao Project and Contributors. All rights reserved.

# Architecture definition
ARCH:=riscv
# CPU definition
CPU:=
# Interrupt controller definition
IRQC:=AIA

drivers := 8250_uart

platform_description:=cva6_desc.c

arch_mem_prot:=mpu

platform-cppflags =
platform-cflags = 
platform-asflags =
platform-ldflags =

ARCH_SUB:=riscv32
