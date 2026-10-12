/*
 * Copyright (c) 2020 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This benchmark is for the test of float computation.
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
// #include <zephyr/sys/util.h>
// #include <zephyr/random/rand32.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/libc-hooks.h>


#define USER_STACKSIZE	2048

struct k_thread user_thread;
K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);


void linpack(int n) {
    int i, j;
    double *A = (double *)malloc(n * n * sizeof(double));
    double *B = (double *)malloc(n * sizeof(double));
    double *x = (double *)malloc(n * sizeof(double));
    if(x==NULL || B ==NULL || A==NULL){
        printf("Error: Malloc area size too small, try to enlarge it in conf.proj.\n");
        return;
    }

    // Generate random numbers for A
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            /* When we need to use rand, CONFIG_MINIMAL_LIBC_NON_REENTRANT_FUNCTIONS and CONFIG_MINIMAL_LIBC_RAND are required.*/
             A[i * n + j] = (double)rand() / RAND_MAX - 0.5;
        }
    }

    // Calculate sum of each row in A
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            B[i] += A[i * n + j];
        }
    }

    // Ax = B
    for (i = 0; i < n; i++) {
        x[i] = B[i];
        for (j = 0; j < n; j++) {
            x[i] -= A[i * n + j] * x[j];
        }
        x[i] /= A[i * n + j];
    }

    for (i = n - 1; i >= 0; i--) {
        for (j = i - 1; j >= 0; j--) {
            x[j] -= A[i * n + j] * x[i];
        }
    }

    free(A);
    free(B);
    free(x);
}

static void user_function(void *p1, void *p2, void *p3) {

	uint64_t start_time = k_uptime_get();

	linpack(1000);

	uint64_t stop_time = k_uptime_get();


    printf("time:%lld ms\n", start_time);
	printf("timex:%lld ms\n", stop_time);
}


int main(void)
{
    /*
    If you want to use malloc in userspace, z_malloc_partition which is enabled in <zephyr/sys/libc-hooks.h> has to be involved  in current memory domain.
    For more information about memory management and memory protection in Zephyr, please refer to the following links:
    https://docs.zephyrproject.org/latest/develop/languages/c/index.html
    https://docs.zephyrproject.org/latest/kernel/usermode/memory_domain.html#pre-defined-memory-partitions
    By the way, do remember to set memory domains and partitions before ret to userspace!
    You can change CONFIG_MINIMAL_LIBC_MALLOC_ARENA_SIZE in prj.conf to configure the size of z_malloc_partition.
    */
    k_mem_domain_add_partition(&k_mem_domain_default, &z_malloc_partition);
    // k_mem_domain_add_partition(&k_mem_domain_default, &z_libc_partition);
	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, NULL, NULL, NULL,
			-1, K_USER, K_MSEC(0));
}