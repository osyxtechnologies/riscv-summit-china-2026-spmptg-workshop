/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Workshop scenario: two periodic tasks running standalone or as a Bao guest, plus a
 * UART RX interrupt handler that prints whenever a character is received.
 *
 * Both tasks run in user mode (K_USER): they fire every 1 s and print their
 * name and an iteration counter so it is easy to verify they are scheduled
 * independently.  printk() and k_msleep() reach the kernel via syscalls.
 */

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#define STACK_SIZE 2048

#define TASK_A_PERIOD_MS 1000
#define TASK_B_PERIOD_MS 1000

K_THREAD_STACK_DEFINE(task_a_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(task_b_stack, STACK_SIZE);

static struct k_thread task_a_data;
static struct k_thread task_b_data;

static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static void uart_rx_handler(const struct device *dev, void *user_data)
{
	char c;

	if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) {
		return;
	}

	while (uart_fifo_read(dev, &c, 1) == 1) {
		if (c >= 0x20 && c < 0x7f) {
			printk("[uart rx] '%c'    0x%02x  %3d\n",
			       c, (uint8_t)c, (uint8_t)c);
		} else {
			printk("[uart rx] '\\x%02x' 0x%02x  %3d\n",
			       (uint8_t)c, (uint8_t)c, (uint8_t)c);
		}
	}
}

/* Runs in user mode (U standalone, VU under Bao): only printk()/k_msleep() syscalls, no kernel data. */
static void periodic_task(void *name, void *period_ms, void *arg3)
{
	const char *task_name = name;
	int period = (int)(uintptr_t)period_ms;
	unsigned int iter = 0;

	while (1) {
		printk("[%s] iter=%u (user mode)\n", task_name, iter++);
		k_msleep(period);
	}
}

int main(void)
{
	printk("Zephyr dual-task demo (board: %s)\n", CONFIG_BOARD_TARGET);

	uart_irq_callback_set(uart, uart_rx_handler);
	uart_irq_rx_enable(uart);

	k_thread_create(&task_a_data, task_a_stack,
			K_THREAD_STACK_SIZEOF(task_a_stack),
			periodic_task, "task_a", (void *)TASK_A_PERIOD_MS, NULL,
			K_PRIO_PREEMPT(1), K_USER, K_NO_WAIT);

	k_thread_create(&task_b_data, task_b_stack,
			K_THREAD_STACK_SIZEOF(task_b_stack),
			periodic_task, "task_b", (void *)TASK_B_PERIOD_MS, NULL,
			K_PRIO_PREEMPT(2), K_USER, K_NO_WAIT);

	return 0;
}
