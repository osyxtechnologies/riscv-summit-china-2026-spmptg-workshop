/**
 * SPDX-License-Identifier: LicenseRef-OSYX-Proprietary
 * Copyright (c) 2025-2026 OSYX Technologies, Lda. All Rights Reserved.
 */


#include <timer.h>
#include <interrupts.h>

irqid_t timer_arch_irq_id(void)
{
    return (irqid_t)TIMR_INT_ID;
}
