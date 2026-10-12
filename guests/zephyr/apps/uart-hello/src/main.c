/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Workshop scenario 01 / Bao + Zephyr smoke test.
 *
 * Prints a banner once Zephyr is up, then idles. The purpose is just to
 * confirm Zephyr boots as a Bao guest with UART output reaching the
 * console; no shell, no IPC.
 */

#include <stdio.h>
#include <zephyr/kernel.h>

int main(void)
{
	printk("Hello from Zephyr (workshop guest, board %s)\n",
	       CONFIG_BOARD_TARGET);
	return 0;
}
