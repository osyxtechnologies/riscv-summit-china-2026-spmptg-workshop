/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <ipc.h>

#include <cpu.h>
#include <vmm.h>
#include <hypercall.h>
#include <config.h>
#include <shmem.h>

enum { IPC_NOTIFY };

union ipc_msg_data {
    struct {
        uint16_t source_vm_id;
        uint16_t shmem_id;
        uint32_t event_id;
    };
    uint64_t raw;
};

static struct ipc* ipc_find_by_shmemid(struct vm* vm, size_t shmem_id)
{
    struct ipc* ipc_obj = NULL;

    for (size_t i = 0; i < vm->ipc_num; i++) {
        if (vm->ipcs[i].shmem_id == shmem_id) {
            ipc_obj = &vm->ipcs[i];
            break;
        }
    }

    return ipc_obj;
}

static void ipc_notify(struct vcpu* vcpu, size_t shmem_id, size_t event_id)
{
    struct ipc* ipc_obj = ipc_find_by_shmemid(vcpu->vm, shmem_id);
    if (ipc_obj != NULL && event_id < ipc_obj->interrupt_num) {
        irqid_t irq_id = ipc_obj->interrupts[event_id];
        vcpu_inject_irq(vcpu, irq_id);
    }
}

static inline void ipc_handle_notify(union ipc_msg_data* ipc_data)
{
    struct shmem* shmem = &config.shmemlist[ipc_data->shmem_id];
    list_foreach (cpu()->vcpu_list, struct vcpu, vcpu) {
        if ((vcpu->vm->id != ipc_data->source_vm_id) && bitmap_get(shmem->vms, vcpu->vm->id)) {
            ipc_notify(vcpu, ipc_data->shmem_id, ipc_data->event_id);
        }
    }
}

static void ipc_handler(uint32_t event, uint64_t data)
{
    union ipc_msg_data ipc_data = { .raw = data };
    switch (event) {
        case IPC_NOTIFY:
            ipc_handle_notify(&ipc_data);
            break;
        default:
            WARNING("Unknown IPC IPI event\n");
            break;
    }
}
CPU_MSG_HANDLER(ipc_handler, IPC_CPUMSG_ID)

long int ipc_hypercall(void)
{
    unsigned long ipc_id = hypercall_get_arg(cpu()->vcpu, 0);
    unsigned long ipc_event = hypercall_get_arg(cpu()->vcpu, 1);

    long int ret = -HC_E_SUCCESS;

    struct shmem* shmem = NULL;
    bool valid_ipc_obj = ipc_id < cpu()->vcpu->vm->ipc_num;
    if (valid_ipc_obj) {
        shmem = shmem_get(cpu()->vcpu->vm->ipcs[ipc_id].shmem_id);
    }
    bool valid_shmem = shmem != NULL;

    if (valid_ipc_obj && valid_shmem) {
        union ipc_msg_data data = {
            .source_vm_id = (uint16_t)cpu()->vcpu->vm->id,
            .shmem_id = (uint16_t)cpu()->vcpu->vm->ipcs[ipc_id].shmem_id,
            .event_id = (uint32_t)ipc_event,
        };
        struct cpu_msg msg = { (uint32_t)IPC_CPUMSG_ID, IPC_NOTIFY, data.raw };

        for (size_t i = 0; i < platform.cpu_num; i++) {
            if (shmem->cpu_masters & (1ULL << i)) {
                if (cpu()->id != i) {
                    cpu_send_msg(i, &msg);
                } else {
                    ipc_handle_notify(&data);
                }
            }
        }

    } else {
        ret = -HC_E_INVAL_ARGS;
    }

    return ret;
}
