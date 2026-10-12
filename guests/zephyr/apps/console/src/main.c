/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Workshop console app: Zephyr shell over UART.
 *
 * The shell itself runs in supervisor (VS) mode.  The `usermode` command
 * spawns a K_USER thread that executes in user (VU) mode behind the guest's
 * vsPMP, demonstrating in-guest privilege separation under the Bao hypervisor.
 * Entering the task triggers Zephyr's pre-sret vsPMP dump (see
 * z_riscv_spmp_dump_umode_state), so the user thread's vsPMP entries are
 * printed automatically as it starts.
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define USER_STACK_SIZE 2048

K_THREAD_STACK_DEFINE(user_task_stack, USER_STACK_SIZE);
static struct k_thread user_task_data;

/* Runs in VU-mode.  printk() reaches the console via the k_str_out syscall;
 * k_is_user_context() / k_msleep() are likewise syscalls, all permitted from
 * user mode. */
static void user_task_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (unsigned int i = 0; i < 5; i++) {
		printk("[user_task] iter=%u  user_context=%d\n",
		       i, (int)k_is_user_context());
		k_msleep(500);
	}

	printk("[user_task] done, exiting user mode\n");
}

static int cmd_usermode(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "spawning user-mode (VU) task; watch for the vsPMP dump");

	k_tid_t tid = k_thread_create(&user_task_data, user_task_stack,
			K_THREAD_STACK_SIZEOF(user_task_stack),
			user_task_entry, NULL, NULL, NULL,
			K_PRIO_PREEMPT(5), K_USER, K_NO_WAIT);

	/* Block the shell until the user task terminates, so the prompt only
	 * returns after it is done (instead of interleaving with its output). */
	k_thread_join(tid, K_FOREVER);

	return 0;
}

SHELL_CMD_REGISTER(usermode, NULL,
		   "Spawn a task that runs in user (VU) mode behind the vsPMP",
		   cmd_usermode);

/*
 * `led on` / `led off` / `led blink` are convenience aliases for
 * `baoipc write_notify 0 "led <state>"`: they just re-run that existing
 * IPC-shmem command, which writes the string to the channel and notifies the
 * baremetal guest (milestone 2), whose led_cmd_run() matches "led on/off/blink".
 */
static int cmd_led_on(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return shell_execute_cmd(sh, "baoipc write_notify 0 \"led on\"");
}

static int cmd_led_off(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return shell_execute_cmd(sh, "baoipc write_notify 0 \"led off\"");
}

static int cmd_led_blink(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return shell_execute_cmd(sh, "baoipc write_notify 0 \"led blink\"");
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_led_cmds,
	SHELL_CMD(on,    NULL, "Turn the baremetal-guest LED on",  cmd_led_on),
	SHELL_CMD(off,   NULL, "Turn the baremetal-guest LED off", cmd_led_off),
	SHELL_CMD(blink, NULL, "Blink the baremetal-guest LED",    cmd_led_blink),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(led, &sub_led_cmds,
		   "Control the baremetal guest's LED over IPC shmem", NULL);

int main(void)
{
	/* Shell is started automatically by Zephyr. */
	return 0;
}
