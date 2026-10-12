/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <hypercall.h>
#include <sched.h>

long int hypercall(unsigned long id)
{
    long int ret = -HC_E_INVAL_ID;

    switch (id) {
        case HC_IPC:
            ret = ipc_hypercall();
            break;
        case HC_REMIO:
            ret = remio_hypercall();
            break;
        case HC_YIELD:
            sched_yield();
            ret = HC_E_SUCCESS;
            break;
        default:
            WARNING("Unknown hypercall id %d\n", id);
    }

    return ret;
}
