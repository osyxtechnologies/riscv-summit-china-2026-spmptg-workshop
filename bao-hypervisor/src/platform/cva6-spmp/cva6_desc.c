/**
 * SPDX-License-Identifier: LicenseRef-OSYX-Proprietary
 * Copyright (c) 2025-2026 OSYX Technologies, Lda. All Rights Reserved.
 */

#include <platform.h>
#include <interrupts.h>

struct platform platform = {

    .cpu_num = 1,

    .region_num = 1,
    .regions =  (struct mem_region[]) {
        {
#if REGLEN == 8
            .base = 0x80200000,
            .size = 0x40000000 - 0x200000
#else
            .base = 0x80400000,
            .size = 0x40000000 - 0x400000
#endif
        }
    },

    .console = {
        .base = 0x10000000,
    },

    .arch = {
#if (IRQC == PLIC)
        .irqc.plic.base = 0xc000000,
#else
        .irqc.aia.aplic.base = 0xd000000,
        .irqc.aia.imsic.base = 0x28000000,
        .irqc.aia.imsic.num_msis = 255,
        .irqc.aia.imsic.num_guest_files = 4,
#endif
    },
};
