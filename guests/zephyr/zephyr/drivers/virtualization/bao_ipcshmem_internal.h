#ifndef BAO_IPCSHMEM_INTERNAL_H_
#define BAO_IPCSHMEM_INTERNAL_H_

#include <zephyr/device.h>

#define DT_DRV_COMPAT   bao_ipcshmem

struct shmem_data {
    DEVICE_MMIO_RAM;
    char* read_buf;
    char* write_buf;
};

struct shmem_config {
    DEVICE_MMIO_ROM;
    size_t read_buf_off;
    size_t read_buf_size;
    size_t write_buf_off;
    size_t write_buf_size;
    unsigned irq;
    unsigned id;
};

#define SHMEM_CONFIG(dev) ((struct shmem_config*)(dev->config))
#define SHMEM_DATA(dev) ((struct shmem_data*)(dev->data))


#endif /* BAO_IPCSHMEM_INTERNAL_H_ */
