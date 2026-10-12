/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * Exercise 02 -- single Zephyr guest on QEMU.
 *
 * Goal: configure Bao to boot one Zephyr VM with UART console access.
 *
 * Memory map reference (QEMU virt, riscv32):
 *   RAM start:     0x80000000
 *   Zephyr image:  0x84200000  (16 MiB region)
 *   UART0 (NS16550): 0x10000000  APLIC source 10
 *   APLIC:         0x0d000000
 *   IMSIC:         0x28000000
 */

#include <config.h>

struct config config = {

    .vmlist_size = 1,
    .vmlist = (struct vm_config[]) {
        {
            /* TODO: set the load address, image base, and size.
             * VM_IMAGE_LOADED(load_addr, base, size) */
            .image = VM_IMAGE_LOADED(/* TODO */, /* TODO */, /* TODO */),

            /* TODO: VM entry point address */
            .entry = /* TODO */,

            .platform = {
                .cpu_num = 1,

                .region_num = 1,
                .regions = (struct vm_mem_region[]) {
                    {
                        /* TODO: base address and size of the VM's RAM region */
                        .base = /* TODO */,
                        .size = /* TODO */,
                    },
                },

                .dev_num = 1,
                .devs = (struct vm_dev_region[]) {
                    {
                        /* TODO: UART physical and virtual address */
                        .pa   = /* TODO */,
                        .va   = /* TODO */,
                        .size = 0x1000,
                        .interrupt_num = 1,
                        /* TODO: APLIC source number for UART0 */
                        .interrupts = (irqid_t[]) {/* TODO */}
                    },
                },

                .arch = {
                    /* TODO: APLIC and IMSIC base addresses */
                    .irqc.aia.aplic.base = /* TODO */,
                    .irqc.aia.imsic.base = /* TODO */,
                },
            },
        },
    }
};
