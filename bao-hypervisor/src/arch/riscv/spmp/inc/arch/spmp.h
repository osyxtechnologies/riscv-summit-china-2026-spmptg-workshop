/**
 * SPDX-License-Identifier: LicenseRef-OSYX-Proprietary
 * Copyright (c) 2025-2026 OSYX Technologies, Lda. All Rights Reserved.
 */

#ifndef SPMP_H
#define SPMP_H

#include <bao.h>
#include <arch/csrs.h>
#include <bitmap.h>

#define SPMP_MAX_NUM_ENTRIES    64
#define SPMP_SISELECT_BASE_ADDR (0x100)

typedef union {
    struct {
        uint8_t r : 1;
        uint8_t w : 1;
        uint8_t x : 1;
        uint8_t a : 2;
        uint8_t res0 : 2;
        uint8_t l : 1;
        uint8_t u : 1;
        uint8_t s : 1;
    };

    uint16_t raw;
} spmp_cfg_t;

struct spmp {
    bool active;
    bool resident;

    uint64_t allocated_entries;
    mpid_t first_entry;
    struct spmp_entry {
        spmp_cfg_t cfg;
        unsigned long addr;
    } entry[SPMP_MAX_NUM_ENTRIES];

    uint64_t spmpen;
};

extern size_t SPMP_NUM_ENTRIES;
extern size_t VSPMP_NUM_ENTRIES;

void spmp_init(struct spmp* spmp);
void spmp_restore(struct spmp* spmp);

static inline void spmp_set_active(struct spmp* spmp, bool active)
{
    spmp->active = active;
}

#endif /* SPMP_H */
