/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Milestone 0 -- protect private stacks, then share a message on purpose.
 *
 * The app already places its message buffer in an application partition.
 * Both user threads are created but not started. The supplied domain is
 * initialized and empty. Complete its configuration before the tasks run.
 *
 * Keep both threads in user mode. Never share either thread's stack.
 * After solving: 's' delivers a message; 'f' must still fault.
 */
#include "isolation.h"

int workshop_setup_sharing(struct k_mem_domain *domain,
			   struct k_mem_partition *partition,
			   struct k_thread *task_a, struct k_thread *task_b)
{
	/* TODO: add partition to domain using k_mem_domain_add_partition(). */
	/* TODO: add task_a and task_b using k_mem_domain_add_thread(). */
	/* Return any API error; return 0 when all three operations succeed. */
	return 0;
}
