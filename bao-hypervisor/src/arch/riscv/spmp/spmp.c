/**
 * SPDX-License-Identifier: LicenseRef-OSYX-Proprietary
 * Copyright (c) 2025-2026 OSYX Technologies, Lda. All Rights Reserved.
 */

#include <arch/spmp.h>

#include <mem.h>
#include <platform.h>
#include <arch/csrs.h>
#include <cpu.h>
#include <vm.h>
#include <bit.h>
#include <arch/instructions.h>
#include <console.h>

/**
 * This code assumes the SPMP switch extensions are implemented.
 */

// This assumes all harts have the same number of SPMP entries
size_t SPMP_NUM_ENTRIES = 0;
size_t VSPMP_NUM_ENTRIES = 0;

static inline bool spmp_reg_is_napot(struct mp_region* mem)
{
    return ((mem->size & (mem->size - 1)) == 0) && (((mem->size - 1) & mem->base) == 0);
}

static inline vaddr_t spmp_entry_base(struct spmp* spmp, mpid_t mpid)
{
    struct spmp_entry* entry = &spmp->entry[mpid];
    uint16_t cfg_a = entry->cfg.a;
    vaddr_t base = INVALID_VA;

    if ((cfg_a == SPMPCFG_A_NA4) || (cfg_a == SPMPCFG_A_NAPOT)) {
        size_t reg_size;
        if (cfg_a == SPMPCFG_A_NA4) {
            reg_size = 4;
        } else {
            reg_size = (1UL << (bit_ffs(~entry->addr) + 3));
        }
        base = entry->addr << 2;
        base &= ~(reg_size - 1);
    } else if (cfg_a == SPMPCFG_A_TOR) {
        if (mpid == 0) {
            base = 0;
        } else {
            base = spmp->entry[mpid - 1].addr << 2;
        }
    }

    return base;
}

static inline void spmp_write_entry(mpid_t i, struct spmp_entry* entry)
{
    csrs_siselect_write(SPMP_SISELECT_BASE_ADDR + i);
    csrs_sireg_write(entry->addr);
    csrs_sireg2_write(entry->cfg.raw);
}

void spmp_init(struct spmp* spmp)
{
    spmp->spmpen = 0;
    for (size_t i = 0; i < SPMP_MAX_NUM_ENTRIES; i++) {
        spmp->entry[i].cfg.raw = 0;
        spmp->entry[i].addr = 0;
    }
    spmp->first_entry = INVALID_MPID;
    spmp->resident = false;
    spmp->active = false;
}

static inline void spmp_fence(bool guest)
{
    if (guest) {
        hfence_gvma();
    } else {
        sfence_vma();
    }
}

static inline void spmp_set_entry(struct spmp* spmp, mpid_t i, struct mp_region* mem, bool locked)
{
    UNUSED_ARG(locked);

    spmp_cfg_t cfg = mem->mem_flags;

    // if (locked) {
    //     cfg.l = 1;
    // }

    cfg.l = 0;

    if (spmp_reg_is_napot(mem)) {
        unsigned long addr = (mem->base >> 2) | ((mem->size - 1) >> 3);
        if (mem->size == 4) {
            cfg.a = SPMPCFG_A_NA4;
        } else {
            cfg.a = SPMPCFG_A_NAPOT;
        }

        spmp->entry[i].cfg = cfg;
        spmp->entry[i].addr = addr;

        if (spmp->active) {
            spmp_write_entry(i, &spmp->entry[i]);
        }

    } else {
        unsigned long addr_base = mem->base >> 2;
        unsigned long addr_top = (mem->base + mem->size) >> 2;
        cfg.a = SPMPCFG_A_TOR;

        spmp->entry[i].cfg = cfg;
        spmp->entry[i].addr = addr_top;
        spmp->entry[i - 1].cfg.a = SPMPCFG_A_OFF;
        spmp->entry[i - 1].addr = addr_base;

        if (spmp->active) {
            spmp_write_entry(i, &spmp->entry[i]);
            spmp_write_entry(i - 1, &spmp->entry[i - 1]);
        }
    }

    if (spmp->active) {
        bool guest_entry = (cfg.u != 0);
        spmp_fence(guest_entry);
    }
}

static inline void spmp_clear_entry(struct spmp* spmp, mpid_t mpid)
{
    spmp_cfg_t cfg = spmp->entry[mpid].cfg;
    bool guest_entry = (cfg.u != 0);

    spmp->entry[mpid].addr = 0;
    spmp->entry[mpid].cfg.a = SPMPCFG_A_OFF;
    spmp->spmpen &= (1UL << mpid);

    if (spmp->active) {
        spmp_write_entry(mpid, &spmp->entry[mpid]);

        spmp_fence(guest_entry);
    }
}

static mpid_t spmp_allocate_entry(struct spmp* spmp, struct mp_region* mem, bool locked)
{
    UNUSED_ARG(spmp);

    uint16_t count_min = ((uint16_t)~0U);
    mpid_t mpid = INVALID_MPID;
    bool napot = spmp_reg_is_napot(mem);

    for (size_t i = 0; i < SPMP_NUM_ENTRIES; i++) {
        bool last = (i == (SPMP_NUM_ENTRIES - 1));
        bool cur_locked = bitmap_get(cpu()->arch.spmp_mngmnt.entry_locked, i);
        bool next_locked = last ? true : bitmap_get(cpu()->arch.spmp_mngmnt.entry_locked, i + 1);
        if (locked) {
            /* If the requested entry is to be locked, we search for a free entry (or two if it is
             * tor) */
            bool cur_free = (cpu()->arch.spmp_mngmnt.entry_allocation_count[i] == 0);
            if (cur_free) {
                if (napot) {
                    mpid = i;
                    break;
                } else if (!last) {
                    bool next_free = cpu()->arch.spmp_mngmnt.entry_allocation_count[i + 1] == 0;
                    if (next_free) {
                        mpid = i;
                        break;
                    }
                }
            }
        } else if (!cur_locked) {
            /**
             * If the entry is not be locked, we search for the entry (or entry pair, if tor) with
             * the minimum allocation count.
             */

            uint16_t count = cpu()->arch.spmp_mngmnt.entry_allocation_count[i];

            if (!napot) {
                if (next_locked) {
                    continue;
                }
                count = (uint16_t)(count + cpu()->arch.spmp_mngmnt.entry_allocation_count[i + 1]);
            }

            if (count < count_min) {
                count_min = count;
                mpid = i;
            }
        }
    }

    if (mpid != INVALID_VMID) {
        cpu()->arch.spmp_mngmnt.entry_allocation_count[mpid]++;
        if (locked) {
            bitmap_set(cpu()->arch.spmp_mngmnt.entry_locked, mpid);
        }
        spmp->allocated_entries = bit64_set(spmp->allocated_entries, mpid);
        if (!napot) {
            cpu()->arch.spmp_mngmnt.entry_allocation_count[mpid + 1]++;
            if (locked) {
                bitmap_set(cpu()->arch.spmp_mngmnt.entry_locked, mpid + 1);
            }
            spmp->allocated_entries = bit64_set(spmp->allocated_entries, mpid + 1);
            /* If tor, the allocation returns the higher entry of the two */
            mpid += 1;
        }
    }

    return mpid;
}

static void spmp_free_entry(struct spmp* spmp, mpid_t mpid)
{
    cpu()->arch.spmp_mngmnt.entry_allocation_count[mpid]--;
    bitmap_clear(cpu()->arch.spmp_mngmnt.entry_locked, mpid);
    bit64_clear(spmp->allocated_entries, mpid);

    if (spmp->entry[mpid].cfg.a == SPMPCFG_A_TOR) {
        cpu()->arch.spmp_mngmnt.entry_allocation_count[mpid - 1]--;
        bitmap_clear(cpu()->arch.spmp_mngmnt.entry_locked, mpid - 1);
        bit64_clear(spmp->allocated_entries, mpid - 1);
    }
}

static bool spmp_perms_valid(spmp_cfg_t* cfg)
{
    // Notw we don't allow shared regions
    return !((cfg->w && !cfg->r) || cfg->s);
}

static inline struct spmp* spmp_find_by_as(struct addr_space* as)
{
    struct spmp* spmp = NULL;

    if (as->type == AS_HYP) {
        spmp = &cpu()->arch.spmp;
    } else {
        list_foreach (cpu()->vcpu_list, struct vcpu, vcpu) {
            if (vcpu->vm->as.id == as->id) {
                spmp = &vcpu->arch.spmp;
            }
        }
    }

    return spmp;
}

void spmp_restore(struct spmp* spmp)
{
    /* This function assumes spmp is the SPMP of a VM */
    if ((!spmp->resident) && (spmp->first_entry != INVALID_MPID)) {
        uint64_t allocated_entries = spmp->allocated_entries >> spmp->first_entry;
        for (size_t i = spmp->first_entry; (i < SPMP_NUM_ENTRIES) && (allocated_entries != 0); i++) {
            if ((allocated_entries & 1) != 0) {
                spmp_write_entry(i, &spmp->entry[i]);
                if (cpu()->arch.spmp_mngmnt.resident[i] != NULL) {
                    *(cpu()->arch.spmp_mngmnt.resident[i]) = false;
                    cpu()->arch.spmp_mngmnt.resident[i] = &spmp->resident;
                }
            }
            allocated_entries >>= 1;
        }
    }

    hfence_gvma();

    csrs_hspmpen_write(spmp->spmpen);

    spmp->resident = true;
    spmp->active = true;
    if (cpu()->arch.spmp_mngmnt.active_guest_spmp != NULL) {
        cpu()->arch.spmp_mngmnt.active_guest_spmp->active = false;
    }
    cpu()->arch.spmp_mngmnt.active_guest_spmp = spmp;
}

bool mpu_map(struct addr_space* as, struct mp_region* mem, bool locked)
{
    bool failed = true;

    struct spmp* spmp = spmp_find_by_as(as);

    if (mem->size > 0 && spmp_perms_valid(&mem->mem_flags)) {
        mpid_t mpid = spmp_allocate_entry(spmp, mem, locked);
        if (mpid == INVALID_MPID && locked) {
            /* A locked region (hypervisor image / MMIO, always resident) could
             * not get a free sPMP entry: this scenario does not fit in the
             * sPMP entries available to Bao. */
            ERROR("insufficient sPMP entries: this scenario does not fit in the "
                  "%lu sPMP entries available to Bao.\n\r"
                  "  Check the VM memory map and QEMU CPU configuration.\n\r",
                (unsigned long)SPMP_NUM_ENTRIES);
        }
        if (mpid != INVALID_MPID) {
            failed = false;

            if (as->type == AS_VM) {
                mem->mem_flags.u = 1;
            } else {
                mem->mem_flags.u = 0;
            }

            spmp_set_entry(spmp, mpid, mem, locked);

            spmp->spmpen |= (1ULL << mpid);
            if (as->type == AS_HYP) {
                csrs_spmpen_set(1ULL << mpid);
            } else if (spmp->active) {
                csrs_hspmpen_set(1ULL << mpid);
            }

            // We don't have to check the sign of ffs output since we know at least one bit is set
            spmp->first_entry = (mpid_t)bit64_ffs(spmp->allocated_entries);
        }
    }

    return !failed;
}

static mpid_t spmp_find_region_entry(struct spmp* spmp, struct mp_region* mem)
{
    mpid_t mpid = INVALID_MPID;

    for (size_t i = 0; i < SPMP_NUM_ENTRIES; i++) {
        if (spmp_entry_base(spmp, i) == mem->base) {
            mpid = i;
            break;
        }
    }

    return mpid;
}

bool mpu_unmap(struct addr_space* as, struct mp_region* mem)
{
    bool failed = true;
    struct spmp* spmp = spmp_find_by_as(as);
    mpid_t mpid = spmp_find_region_entry(spmp, mem);

    if (mpid != INVALID_MPID) {
        spmp_free_entry(spmp, mpid);

        spmp_clear_entry(spmp, mpid);

        spmp->spmpen &= ~(1ULL << mpid);
        if (as->type == AS_HYP) {
            csrs_spmpen_clear(1ULL << mpid);
        } else if (spmp->active) {
            csrs_hspmpen_clear(1ULL << mpid);
        }

        ssize_t first_entry = bit64_ffs(spmp->allocated_entries);
        spmp->first_entry = (first_entry >= 0) ? (mpid_t)first_entry : INVALID_MPID;

        failed = false;
    }

    return !failed;
}

bool mpu_update(struct addr_space* as, struct mp_region* mpr)
{
    bool failed = true;

    if (mpu_unmap(as, mpr)) {
        failed = !mpu_map(as, mpr, false);
    }

    return !failed;
}

static inline ssize_t my_ffs(uint64_t word)
{
    ssize_t pos = (ssize_t)0;
    uint64_t mask = UINT64_C(1);
    while (mask != 0U) {
        if ((mask & word) != 0U) {
            break;
        }
        mask <<= 1U;
        pos++;
    }
    return (mask != 0U) ? pos : (ssize_t)~0L;
}

void mpu_init(void)
{
    if (cpu_is_master()) {
        /* Probe the total number of available SPMP entries. Writing ~0UL is safe
         * because it only maximizes the hypervisor's share, keeping the SBI's
         * allow-all entry within hSPMP range. */
        csrs_hspmpdeleg_write(~0UL);
        size_t total_spmp = csrs_hspmpdeleg_read() & HSPMPDELEG_PMPNUM_MSK;

        size_t hyp_entries = platform.arch.spmp_min_hyp_entries;
        if (hyp_entries == 0) {
            hyp_entries = total_spmp / 2;
        }
        if (hyp_entries == 0) {
            hyp_entries = 1;
        }

        /* Before lowering hspmpdeleg, write an allow-all entry at the future
         * last hypervisor slot so the hypervisor keeps running during the
         * transition (the SBI's allow-all is at total_spmp-1 and would fall
         * outside the hypervisor range after the write). */
        if (hyp_entries < total_spmp) {
            csrs_siselect_write(SPMP_SISELECT_BASE_ADDR + hyp_entries - 1);
            csrs_sireg_write(~0UL);
            csrs_sireg2_write(SPMPCFG_NAPOT | SPMPCFG_R_BIT | SPMPCFG_W_BIT | SPMPCFG_X_BIT);
            csrs_spmpen_set(1ULL << (hyp_entries - 1));
        }

        csrs_hspmpdeleg_write(hyp_entries);
        SPMP_NUM_ENTRIES = csrs_hspmpdeleg_read() & HSPMPDELEG_PMPNUM_MSK;

        /* If WARL rejected our value (e.g. hspmpdeleg is hardwired), the
         * allow-all entry we pre-wrote at hyp_entries-1 is spurious; remove it
         * so it does not linger as an untracked open entry. */
        if (SPMP_NUM_ENTRIES != hyp_entries && hyp_entries < total_spmp) {
            csrs_siselect_write(SPMP_SISELECT_BASE_ADDR + hyp_entries - 1);
            csrs_sireg_write(0);
            csrs_sireg2_write(0);
            csrs_spmpen_clear(1ULL << (hyp_entries - 1));
        }

        csrs_vspmpen_write((uint64_t)(-1));
        ssize_t tmp_vspmp_entries = my_ffs(~csrs_vspmpen_read());
        if (tmp_vspmp_entries >= 0) {
            VSPMP_NUM_ENTRIES = (size_t)tmp_vspmp_entries;
        }
        csrs_vspmpen_write((uint64_t)(0));
    }

    cpu_sync_barrier(&cpu_glb_sync);

    for (size_t i = 0; i < SPMP_MAX_NUM_ENTRIES; i++) {
        bitmap_clear(cpu()->arch.spmp_mngmnt.entry_locked, i);
        cpu()->arch.spmp_mngmnt.entry_allocation_count[i] = 0;
        cpu()->arch.spmp_mngmnt.resident[i] = NULL;
    }
    cpu()->arch.spmp_mngmnt.active_guest_spmp = NULL;

    spmp_init(&cpu()->arch.spmp);
    spmp_set_active(&cpu()->arch.spmp, true);
}

void mpu_enable(void)
{
    /**
     * We assume that the supervisor execution environemtn set the last
     * hSPMP entry using NAPOT to allow all accesses. At this point we clear that region
     * because all hypervisor regions were previously set in mem_init_boot_regions, effectively
     * enabling SPMP protection for the hypervisor itself.
     */

    spmp_clear_entry(&cpu()->arch.spmp, SPMP_NUM_ENTRIES - 1);
}

bool mpu_perms_compatible(unsigned long perms1, unsigned long perms2)
{
    return perms1 == perms2;
}
