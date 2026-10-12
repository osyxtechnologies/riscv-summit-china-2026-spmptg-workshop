/*
 * Copyright (c) 2020 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#define USER_STACKSIZE	2048

#ifndef CONFIG_USERSPACE
#error This sample requires CONFIG_USERSPACE.
#endif

// struct k_thread user_thread;
// K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

struct k_thread user_thread1;
K_THREAD_STACK_DEFINE(user_stack1, USER_STACKSIZE);

struct k_thread user_thread2;
K_THREAD_STACK_DEFINE(user_stack2, USER_STACKSIZE);

// static void user_function(void *p1, void *p2, void *p3)
// {
// 	printf("Hello World from %s (%s)\n",
// 	       k_is_user_context() ? "UserSpace!" : "privileged mode.",
// 	       CONFIG_BOARD);
// 	__ASSERT(k_is_user_context(), "User mode execution was expected");
// }

static void user_function1(void *p1, void *p2, void *p3)
{
	while (1)
	{
		printf("Executing Thread #1\n");
		k_sleep(K_MSEC(1000));
	}
}

static void user_function2(void *p1, void *p2, void *p3)
{
	while (1)
	{
		printf("Execute Thread #2\n");
		k_sleep(K_MSEC(1000));
	}
}


int main(void)
{
	// k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
	// 		user_function, NULL, NULL, NULL,
	// 		-1, K_USER, K_MSEC(0));

	k_thread_create(&user_thread1, user_stack1, USER_STACKSIZE,
			user_function1, NULL, NULL, NULL,
			1, K_USER, K_MSEC(0));

	k_thread_create(&user_thread2, user_stack2, USER_STACKSIZE,
			user_function2, NULL, NULL, NULL,
			2, K_USER, K_MSEC(0));
	return 0;
}
