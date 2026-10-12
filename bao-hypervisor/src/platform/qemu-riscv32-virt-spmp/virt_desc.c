/**
 * SPDX-License-Identifier: LicenseRef-OSYX-Proprietary
 * Copyright (c) 2025-2026 OSYX Technologies, Lda. All Rights Reserved.
 */

/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <platform.h>
#include <interrupts.h>

struct platform platform = {

    .cpu_num = 1,

    .region_num = 1,
    .regions =  (struct mem_region[]) {
        {
            .base = 0x80400000,
            .size = 0x40000000 - 0x400000,
        },
    },

    .arch = {

#if (IRQC == PLIC)
        .irqc.plic.base = 0xc000000,
#else
        .irqc.aia.aplic.base = 0xd000000,
        .irqc.aia.imsic.base = 0x28000000,
        .irqc.aia.imsic.num_msis = 255,
        .irqc.aia.imsic.num_guest_files = 2,
#endif

#if (IPIC == IPIC_ACLINT)
        .aclint_sswi.base = 0x2f00000,
#endif
    },

};
