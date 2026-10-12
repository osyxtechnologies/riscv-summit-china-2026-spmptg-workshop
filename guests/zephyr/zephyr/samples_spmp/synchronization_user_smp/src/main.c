/* main.c - Hello World demo */

/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sem.h>	
#include <stdio.h>
#include <zephyr/app_memory/app_memdomain.h>

/*
 * The hello world demo has two threads that utilize semaphores and sleeping
 * to take turns printing a greeting message at a controlled rate. The demo
 * shows both the static and dynamic approaches for spawning a thread; a real
 * world application would likely use the static approach for both threads.
 */

#define PIN_THREADS (IS_ENABLED(CONFIG_SMP) && IS_ENABLED(CONFIG_SCHED_CPU_MASK))

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

/* delay between greetings (in ms) */
#define SLEEPTIME 500


/*
 * @param my_name      thread identification string
 * @param my_sem       thread's own semaphore
 * @param other_sem    other thread's semaphore
 */
void helloLoop(const char *my_name,
	       struct sys_sem *my_sem, struct sys_sem *other_sem)
{
	const char *tname;
	struct k_thread *current_thread;

	while (1) {
		/* take my semaphore */
		sys_sem_take(my_sem, K_FOREVER);
		
		// k_current_get is a syscall. CONFIG_APPLICATION_DEFINED_SYSCALL is needed to be enabled when we want to use it in userspace.
		current_thread = k_current_get();
		tname = k_thread_name_get(current_thread);

	// #if CONFIG_SMP
	// 		TODO: in userspace this API is not supported
	// 		cpu = arch_curr_cpu()->id;
	// #else
	// 		cpu = 0;
	// #endif
		/* say "hello" */
		if (tname == NULL) {
			printf("%s: Hello World on %s userspace!\n",
				my_name, CONFIG_BOARD);
		} else {
			printf("%s: Hello World on %s userspace!\n",
				tname, CONFIG_BOARD);
		}

		/* wait a while, then let other thread have a turn */
		k_busy_wait(100000);
		k_msleep(SLEEPTIME);
		sys_sem_give(other_sem);
	}
}

/* define semaphores
   In userspace, the statically defined semaphores are required to be placed in a userspace's patrition
   which allows threads running in User mode to access to those semaphores.
   For thread struct it's the same.
 */
K_APPMEM_PARTITION_DEFINE(my_partition);

K_APP_DMEM(my_partition) struct sys_sem threadA_sem, threadB_sem;
SYS_SEM_DEFINE(threadA_sem, 1, 1);	/* starts off "available" */
SYS_SEM_DEFINE(threadB_sem, 0, 1);	/* starts off "not available" */

/* threadB is a dynamic thread that is spawned by threadA */

void threadB(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	/* invoke routine to ping-pong hello messages with threadA */
	helloLoop(__func__, &threadB_sem, &threadA_sem);
}

K_THREAD_STACK_DEFINE(threadA_stack_area, STACKSIZE);
static struct k_thread threadA_data;

K_THREAD_STACK_DEFINE(threadB_stack_area, STACKSIZE);
static struct k_thread threadB_data;

/* threadA is a static thread that is spawned automatically */

void threadA(void *dummy1, void *dummy2, void *dummy3)
{
	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	/* invoke routine to ping-pong hello messages with threadB */
	helloLoop(__func__, &threadA_sem, &threadB_sem);
}

int main(void)
{
	k_mem_domain_add_partition(&k_mem_domain_default, &my_partition);
	k_thread_create(&threadA_data, threadA_stack_area,
			K_THREAD_STACK_SIZEOF(threadA_stack_area),
			threadA, NULL, NULL, NULL,
			PRIORITY, K_USER, K_FOREVER);
	k_thread_name_set(&threadA_data, "thread_a");
#if PIN_THREADS
	if (arch_num_cpus() > 1) {
		k_thread_cpu_pin(&threadA_data, 0);
	}
#endif

	k_thread_create(&threadB_data, threadB_stack_area,
			K_THREAD_STACK_SIZEOF(threadB_stack_area),
			threadB, NULL, NULL, NULL,
			PRIORITY, K_USER, K_FOREVER);
	k_thread_name_set(&threadB_data, "thread_b");
#if PIN_THREADS
	if (arch_num_cpus() > 1) {
		k_thread_cpu_pin(&threadB_data, 1);
	}
#endif

	k_thread_start(&threadA_data);
	k_thread_start(&threadB_data);
}
