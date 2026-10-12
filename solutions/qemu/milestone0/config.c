/* SPDX-License-Identifier: Apache-2.0 */
#include "isolation.h"

int workshop_setup_sharing(struct k_mem_domain *domain,
			   struct k_mem_partition *partition,
			   struct k_thread *task_a, struct k_thread *task_b)
{
	int ret = k_mem_domain_add_partition(domain, partition);

	if (ret != 0) {
		return ret;
	}
	ret = k_mem_domain_add_thread(domain, task_a);
	if (ret != 0) {
		return ret;
	}
	return k_mem_domain_add_thread(domain, task_b);
}
