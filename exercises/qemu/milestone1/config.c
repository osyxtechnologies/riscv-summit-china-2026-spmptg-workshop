/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * Workshop scenario 01 -- single baremetal guest on QEMU.
 *
 * One VM at 0x84200000 (16 MiB) running the baremetal app on UART0
 * (0x10000000, APLIC source 10). No IPC, no second guest.
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
