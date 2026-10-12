/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef __ARCH_PLATFORM_H__
#define __ARCH_PLATFORM_H__

#include <bao.h>

// Arch-specific platform data
struct arch_platform {
    union irqc_dscrp {
        struct {
            paddr_t base;
        } plic;
        struct {
            struct {
                paddr_t base;
            } aplic;
            struct {
                paddr_t base;
                size_t num_msis;
                size_t num_guest_files;
            } imsic;
        } aia;
    } irqc;

    struct {
        paddr_t base;      // Base address of the IOMMU mmapped IF
        unsigned mode;     // Overall IOMMU mode (Off, Bypass, DDT-lvl)
        irqid_t fq_irq_id; // Fault Queue IRQ ID (wired)
    } iommu;

    struct {
        paddr_t base; // Base address of the ACLINT supervisor software interrupts
    } aclint_sswi;

    /**
     * Minimum number of SPMP entries reserved for hypervisor use. Used to set
     * hspmpdeleg.pmpnum, which defines the SPMP/vSPMP split: entries [0, pmpnum-1]
     * are kept by the hypervisor and entries [pmpnum, N-1] are delegated to vSPMP.
     * If zero, defaults to half of the available SPMP entries. Ignored on platforms
     * where hspmpdeleg is hardwired (WARL will override the written value).
     */
    size_t spmp_min_hyp_entries;
};

#endif /* __ARCH_PLATFORM_H__ */
