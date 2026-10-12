/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Milestone 0: two userspace tasks, private stacks and explicit sharing.
 * UART commands are queued by the kernel; all demonstration memory accesses
 * happen in the user threads, where SPMP can enforce their permissions.
 */
#include <zephyr/app_memory/app_memdomain.h>
#include <zephyr/arch/riscv/csr.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/fatal.h>
#include <string.h>

#include "isolation.h"

#define STACK_SIZE 2048

K_THREAD_STACK_DEFINE(task_a_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(task_b_stack, STACK_SIZE);
static struct k_thread task_a_data, task_b_data;
K_MSGQ_DEFINE(task_a_commands, sizeof(char), 4, 1);
K_MSGQ_DEFINE(task_b_commands, sizeof(char), 4, 1);
K_SEM_DEFINE(message_read, 0, 1);

K_APPMEM_PARTITION_DEFINE(message_partition);
K_APP_BMEM(message_partition) static struct {
	unsigned int sequence;
	char text[32];
} shared_message;
static struct k_mem_domain shared_domain;
static volatile bool task_faulted;
static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static void print_help(void)
{
	printk("\nMilestone 0: Zephyr task isolation\n"
	       "  f: task A tries to write task B's private stack\n"
	       "  s: task A sends a shared message to task B\n"
	       "  h: show commands\n"
	       "After a fault, exit with Ctrl-A x and run again.\n");
}

static void uart_rx_handler(const struct device *dev, void *user_data)
{
	char c;

	ARG_UNUSED(user_data);
	if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) {
		return;
	}
	while (uart_fifo_read(dev, &c, 1) == 1) {
		if (c == 'h') {
			print_help();
		} else if (c == 'f' || c == 's') {
			if (task_faulted) {
				printk("[workshop] A task stopped. Restart QEMU before another test.\n");
			} else if (k_msgq_put(&task_a_commands, &c, K_NO_WAIT) != 0) {
				printk("[workshop] Command queue full; try again.\n");
			}
		}
	}
}

static void task_a(void *private_stack, void *unused2, void *unused3)
{
	unsigned int iter = 0, sequence = 0;
	char command;

	while (1) {
		printk("[task_a] iter=%u (user mode)\n", iter++);
		if (k_msgq_get(&task_a_commands, &command, K_SECONDS(1)) != 0) {
			continue;
		}
		if (command == 'f') {
			printk("[task_a] Attempting a write to task B's private stack\n");
			*(volatile unsigned char *)private_stack = 0x5a;
			printk("[FAIL] Private-stack write succeeded; isolation is broken!\n");
		} else if (command == 's') {
			printk("[task_a] Writing the shared message\n");
			shared_message.sequence = ++sequence;
			strcpy(shared_message.text, "hello from task A");
			/* The queue synchronizes publication and receipt of the buffer. */
			k_msgq_put(&task_b_commands, &command, K_FOREVER);
			/* Wait for the reader before overwriting the shared buffer. */
			k_sem_take(&message_read, K_FOREVER);
		}
	}
}

static void task_b(void *unused1, void *unused2, void *unused3)
{
	unsigned int iter = 0;
	char command;

	while (1) {
		printk("[task_b] iter=%u (user mode)\n", iter++);
		if (k_msgq_get(&task_b_commands, &command, K_SECONDS(1)) == 0) {
			printk("[task_b] received #%u: %s\n",
			       shared_message.sequence, shared_message.text);
			k_sem_give(&message_read);
		}
	}
}

/* Recover only from the two intentional user-access tests. Unexpected kernel
 * faults retain the normal halt policy. Returning lets Zephyr abort the
 * faulting thread; the other task continues to demonstrate containment. */
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
	uintptr_t address = csr_read(stval);
	unsigned long cause = csr_read(scause);
	bool private_stack = address == (uintptr_t)K_THREAD_STACK_BUFFER(task_b_stack);
	bool message = address >= (uintptr_t)&shared_message &&
		address < (uintptr_t)&shared_message + sizeof(shared_message);
	struct k_thread *thread = k_current_get();

	if (reason == K_ERR_CPU_EXCEPTION && esf != NULL &&
	    (esf->mstatus & (1UL << 8)) == 0 && /* SPP=0: fault came from U-mode */
	    (cause == 5 || cause == 7) && /* load/store access fault */
	    ((thread == &task_a_data && (private_stack || message)) ||
	     (thread == &task_b_data && message))) {
		task_faulted = true;
		printk("[SPMP] Blocked %s access by task %s. Stopping this task only.\n",
		       private_stack ? "private-stack" : "unshared-buffer",
		       thread == &task_a_data ? "A" : "B");
		printk("[workshop] Restart QEMU to retry. Only the message partition may be shared.\n");
		return;
	}
	printk("[workshop] Unexpected fatal error: %u\n", reason);
	k_fatal_halt(reason);
}

int main(void)
{
	int ret;

	printk("Zephyr SPMP task-isolation demo (board: %s)\n", CONFIG_BOARD_TARGET);
	k_thread_create(&task_a_data, task_a_stack, K_THREAD_STACK_SIZEOF(task_a_stack),
			task_a, K_THREAD_STACK_BUFFER(task_b_stack), NULL, NULL,
			K_PRIO_PREEMPT(1), K_USER, K_FOREVER);
	k_thread_create(&task_b_data, task_b_stack, K_THREAD_STACK_SIZEOF(task_b_stack),
			task_b, NULL, NULL, NULL, K_PRIO_PREEMPT(2), K_USER, K_FOREVER);
	k_thread_access_grant(&task_a_data, &task_a_commands, &task_b_commands, &message_read);
	k_thread_access_grant(&task_b_data, &task_b_commands, &message_read);
	ret = k_mem_domain_init(&shared_domain, 0, NULL);
	if (ret == 0) {
		ret = workshop_setup_sharing(&shared_domain, &message_partition,
					    &task_a_data, &task_b_data);
	}
	if (ret != 0) {
		printk("[workshop] Memory-domain setup failed: %d\n", ret);
		return ret;
	}
	print_help();
	uart_irq_callback_set(uart, uart_rx_handler);
	uart_irq_rx_enable(uart);
	k_thread_start(&task_a_data);
	k_thread_start(&task_b_data);
	return 0;
}
