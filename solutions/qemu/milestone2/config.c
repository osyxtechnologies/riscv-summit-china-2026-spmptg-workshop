/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * Workshop scenario 02 - single Zephyr guest, UART only.
 *
 * One VM at 0x84200000 (16 MiB), UART (NS16550) at 0x10000000 with APLIC
 * source 10 routed to the guest. No baremetal guest, no IPC shared memory.
 *
 */

#include <config.h>

struct config config = {

    .vmlist_size = 1,
    .vmlist = (struct vm_config[]) {
        {
            .image = VM_IMAGE_LOADED(0x84200000, 0x84200000, 0x1000000),

            .entry = 0x84200000,

            .platform = {
                .cpu_num = 1,

                .region_num = 1,
                .regions = (struct vm_mem_region[]) {
                    {
                        .base = 0x84200000,
                        .size = 0x1000000,
                    },
                },

                .dev_num = 1,
                .devs = (struct vm_dev_region[]) {
                    {
                        .pa = 0x10000000,
                        .va = 0x10000000,
                        .size = 0x1000,
                        .interrupt_num = 1,
                        .interrupts = (irqid_t[]) {10}
                    },
                },

                .arch = {
                        .irqc.aia.aplic.base = 0xd000000,
                        .irqc.aia.imsic.base = 0x28000000,
                },
            },
        },
    }
};
