/* SPDX-License-Identifier: Apache-2.0 */
#ifndef WORKSHOP_ISOLATION_H
#define WORKSHOP_ISOLATION_H

#include <zephyr/kernel.h>

/* Called in supervisor mode before either user thread is started. */
int workshop_setup_sharing(struct k_mem_domain *domain,
			   struct k_mem_partition *partition,
			   struct k_thread *task_a, struct k_thread *task_b);

#endif
