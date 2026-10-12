/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * Workshop scenario 03 - Zephyr + baremetal guests, UART + IPC shmem.
 *
 * Memory layout:
 *   VM1 (Zephyr):    0x84200000, 16 MiB, UART0 0x10000000 (APLIC source 10)
 *   VM2 (baremetal): 0x88200000, 16 MiB, UART1 0x06000000 (APLIC source 12)
 *   IPC shmem:       0x8A000000 in both VMs, 16 KiB (2×8 KiB channels)
 *                    Zephyr reads [0x0, 0x2000), writes [0x2000, 0x4000)
 *                    IPC interrupt: APLIC source 3 injected by Bao on notify
 */

#include <config.h>

struct config config = {

    .shmemlist_size = 1,
    .shmemlist = (struct shmem[]) {
        [0] = {
            .base = 0x8A000000,
            .size = 0x00004000,
        },
    },

    .vmlist_size = 2,
    .vmlist = (struct vm_config[]) {
        /* VM1: Zephyr ------------------------------------------------- */
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

                .ipc_num = 1,
                .ipcs = (struct ipc[]) {
                    {
                        .base = 0x8A000000,
                        .size = 0x00004000,
                        .shmem_id = 0,
                        .interrupt_num = 1,
                        .interrupts = (irqid_t[]) {3}
                    }
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

        /* VM2: Baremetal ---------------------------------------------- */
        {
            .image = VM_IMAGE_LOADED(0x88200000, 0x88200000, 0x1000000),
            .entry = 0x88200000,

            .platform = {
                .cpu_num = 1,

                .region_num = 1,
                .regions = (struct vm_mem_region[]) {
                    {
                        .base = 0x88200000,
                        .size = 0x1000000,
                    }
                },

                .ipc_num = 1,
                .ipcs = (struct ipc[]) {
                    {
                        .base = 0x8A000000,
                        .size = 0x00004000,
                        .shmem_id = 0,
                        .interrupt_num = 1,
                        .interrupts = (irqid_t[]) {3}
                    }
                },

                /* UART1 (0x6000000, APLIC source 12) assigned exclusively
                 * to the baremetal guest for its console output. */
                .dev_num = 1,
                .devs = (struct vm_dev_region[]) {
                    {
                        .pa = 0x6000000,
                        .va = 0x6000000,
                        .size = 0x1000,
                        .interrupt_num = 1,
                        .interrupts = (irqid_t[]) {12}
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
