#include <zephyr/drivers/virtualization/bao_ipcshmem.h>
#include "bao_ipcshmem_internal.h"

#include <zephyr/irq.h>

#include <stdio.h>
#include <string.h>

#if defined(CONFIG_ARM64) || defined(CONFIG_ARM)
#include <zephyr/arch/arm64/arm-smccc.h>

#define ARM_SMCCC_OWNER_VENDOR_HYP	6
#define ARM_SMCCC_FAST_CALL	    1UL
#define ARM_SMCCC_SMC_64		1
#define ARM_SMCCC_SMC_32		0
#if defined(CONFIG_ARM64)
#define ARM_SMCCC_SMC  ARM_SMCCC_SMC_64
#else
#define ARM_SMCCC_SMC  ARM_SMCCC_SMC_32
#endif
#define ARM_SMCCC_CALL_VAL(type, calling_convention, owner, func_id) \
    (((type) << 31) | ((calling_convention) << 30) | \
     ((owner) << 24) | ((func_id) & 0xffff))
#define BAO_IPC_HC_ID 1
#define BAO_SHMEMIPC_SMCCC_VAL ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, \
    ARM_SMCCC_SMC, ARM_SMCCC_OWNER_VENDOR_HYP, BAO_IPC_HC_ID)

#elif defined(CONFIG_RISCV)
#include <zephyr/arch/riscv/sbi.h>

/* Bao uses an experimental SBI extension ID space for hypercalls. The FID
 * carries the hypercall ID; HC_IPC = 1 (see bao-hypervisor's
 * src/core/inc/hypercall.h). */
#define BAO_SBI_EXTID    0x08000ba0
#define BAO_SBI_FID_IPC  1

#else
#error "Bao IPC Shared Memory: unsupported architecture"
#endif

static void shmem_read(const struct device *dev, char *buf, size_t n) {
    size_t size = MIN(SHMEM_CONFIG(dev)->read_buf_size, n);
    memcpy(buf, SHMEM_DATA(dev)->read_buf, size);
}

static void shmem_write(const struct device *dev, const char *buf, size_t n) {
    size_t size = MIN(SHMEM_CONFIG(dev)->write_buf_size, n);
    snprintf(SHMEM_DATA(dev)->write_buf, size, "%s", buf);
}

static void shmem_notify(const struct device *dev) {
    unsigned shmem_id = SHMEM_CONFIG(dev)->id;
    unsigned notification_id = 0; // we assume only one notification

#if defined(CONFIG_ARM64) || defined(CONFIG_ARM)
    struct arm_smccc_res hvc_res;
    arm_smccc_hvc(BAO_SHMEMIPC_SMCCC_VAL, shmem_id, notification_id,
        0, 0, 0, 0, 0, &hvc_res);
#elif defined(CONFIG_RISCV)
    (void)sbi_ecall(BAO_SBI_EXTID, BAO_SBI_FID_IPC,
        shmem_id, notification_id, 0, 0, 0, 0);
#endif
}

static unsigned shmem_id(const struct device *dev) {
    return SHMEM_CONFIG(dev)->id;
}

static void shmem_irq_enable(const struct device *dev) {
    irq_enable(SHMEM_CONFIG(dev)->irq);
}

static void shmem_irq_set_callback(const struct device *dev,
    bao_ipcshmem_callback_t callback)
{
    irq_connect_dynamic(SHMEM_CONFIG(dev)->irq, 0,
        (void (*)(const void*))callback, dev, 0);
}

static struct bao_ipcshmem_api shmem_api = {
    .read = shmem_read,
    .write = shmem_write,
    .notify = shmem_notify,
    .id = shmem_id,
    .irq_set_enable = shmem_irq_enable,
    .irq_set_callback = shmem_irq_set_callback,
};

static int shmemipc_init(const struct device *dev) {

    struct shmem_config *config = SHMEM_CONFIG(dev);
    struct shmem_data *data = SHMEM_DATA(dev);

    DEVICE_MMIO_MAP(dev, K_MEM_CACHE_WB | K_MEM_PERM_RW);

    data->read_buf = (char*)(DEVICE_MMIO_GET(dev) + config->read_buf_off);
    data->write_buf = (char*)(DEVICE_MMIO_GET(dev) + config->write_buf_off);

    memset(data->read_buf, 0, config->read_buf_size);
    memset(data->write_buf, 0, config->write_buf_size);

    return 0;
}

#define BAO_IPCSHMEM_INSTANTIATE(inst) \
    struct shmem_data shmem_data_##inst; \
    struct shmem_config shmem_config_##inst = { \
         DEVICE_MMIO_ROM_INIT(DT_DRV_INST(inst)), \
        .read_buf_off = DT_PROP_BY_IDX(DT_DRV_INST(inst), read_channel, 0), \
        .read_buf_size = DT_PROP_BY_IDX(DT_DRV_INST(inst), read_channel, 1), \
        .write_buf_off = DT_PROP_BY_IDX(DT_DRV_INST(inst), write_channel, 0), \
        .write_buf_size = DT_PROP_BY_IDX(DT_DRV_INST(inst), write_channel, 1), \
        .irq = DT_IRQN(DT_DRV_INST(inst)), \
        .id = DT_PROP(DT_DRV_INST(inst), id), \
    }; \
    DEVICE_DT_DEFINE( \
        DT_INST(inst, DT_DRV_COMPAT), \
        shmemipc_init, \
        NULL, \
        &shmem_data_##inst, \
        &shmem_config_##inst, \
        POST_KERNEL, \
        1, \
        &shmem_api, \
    );

DT_INST_FOREACH_STATUS_OKAY(BAO_IPCSHMEM_INSTANTIATE)
