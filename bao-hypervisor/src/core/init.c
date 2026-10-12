/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <bao.h>

#include <cpu.h>
#include <mem.h>
#include <interrupts.h>
#include <console.h>
#include <printk.h>
#include <platform.h>
#include <timer.h>
#include <vmm.h>
#include <sched.h>

void init(cpuid_t cpu_id)
{
    /**
     * These initializations must be executed first and in fixed order.
     */

    cpu_init(cpu_id);
    mem_init();

    /* -------------------------------------------------------------- */

    platform_init();

    console_init();

    if (cpu_is_master()) {
        console_printk("Bao Hypervisor %s (%s - %s)\n\r", BAO_VERSION, __DATE__, __TIME__);
    }

    interrupts_init();

    timer_init();

    vmm_init();

    sched_start();

    vcpu_arch_entry();

    /* Should never reach here */
    while (1) { }
}
