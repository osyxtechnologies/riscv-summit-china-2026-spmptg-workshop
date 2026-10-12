#include <zephyr/shell/shell.h>
#include <zephyr/device.h>
#include <zephyr/drivers/virtualization/bao_ipcshmem.h>
#include "bao_ipcshmem_internal.h"

#include <stdlib.h>

static const struct device* shmem_dev_list[] = {
    DT_FOREACH_STATUS_OKAY(DT_DRV_COMPAT, DEVICE_DT_GET)
};
static const size_t shmem_dev_list_size =
    sizeof(shmem_dev_list)/sizeof(struct device*);

static const struct device* bao_ipcshmem_find_dev(const struct shell *shell,
    const char* shmem_id_str) {
    unsigned shmem_id = atoi(shmem_id_str);
    const struct device* dev = NULL;
    for (int i = 0; i < shmem_dev_list_size; i++) {
        const struct device* tmp = shmem_dev_list[i];
        if (bao_ipcshmem_id(tmp) == shmem_id) {
            dev = tmp;
            break;
        }
    }
    if (dev == NULL) {
        shell_error(shell, "Invalid shmem id: %s", shmem_id_str);
    }
    return dev;
}

#define MSG_SIZE 256
static char msg[MSG_SIZE];

static int cmd_bao_ipcshmem_read(const struct shell *shell,
			     size_t argc, char **argv)
{
    const struct device* dev = bao_ipcshmem_find_dev(shell, argv[1]);
    if (dev == NULL) return -EINVAL;
    bao_ipcshmem_read(dev, msg, MSG_SIZE);
    msg[MSG_SIZE-1] = '\0';
    shell_fprintf(shell, SHELL_NORMAL, "%s", msg);
	return 0;
}

static int cmd_bao_ipcshmem_write(const struct shell *shell,
			     size_t argc, char **argv)
{
    const struct device* dev = bao_ipcshmem_find_dev(shell, argv[1]);
    if (dev == NULL) return -EINVAL;
    size_t len = MIN(MSG_SIZE-2, strlen(argv[2]));
    memcpy(msg, argv[2], len);
    msg[len] = '\n';
    msg[len+1] = '\0';
    bao_ipcshmem_write(dev, msg, len+2);
	return 0;
}

static int cmd_bao_ipcshmem_notify(const struct shell *shell,
                 size_t argc, char **argv)
{
    const struct device* dev = bao_ipcshmem_find_dev(shell, argv[1]);
    if (dev == NULL) return -EINVAL;
    bao_ipcshmem_notify(dev);
    return 0;
}

static int cmd_bao_ipcshmem_write_notify(const struct shell *shell,
                 size_t argc, char **argv)
{
    const struct device* dev = bao_ipcshmem_find_dev(shell, argv[1]);
    if (dev == NULL) return -EINVAL;
    bao_ipcshmem_write(dev, argv[2], strlen(argv[2])+1);
    bao_ipcshmem_notify(dev);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_bao_ipcshmem_cmds,
			       SHELL_CMD_ARG(read, NULL,
					     "Read bao shared memory contents",
					     cmd_bao_ipcshmem_read, 2, 0),
			       SHELL_CMD_ARG(write, NULL,
					     "Write bao shared memory contents",
					     cmd_bao_ipcshmem_write, 3, 0),
			       SHELL_CMD_ARG(notify, NULL,
					     "Send a notification to the VMs connected to the "
                         "same bao shared memory",
					     cmd_bao_ipcshmem_notify, 2, 0),
			       SHELL_CMD_ARG(write_notify, NULL,
					     "Write bao shared memory contents and send a "
                         "nofitication to the VMs connected to the same bao "
                         "shared memory",
					     cmd_bao_ipcshmem_write_notify, 3, 0),
			       SHELL_SUBCMD_SET_END
		);

SHELL_CMD_REGISTER(baoipc, &sub_bao_ipcshmem_cmds,
		   "Bao IPC Shared Memory Commands", NULL);
