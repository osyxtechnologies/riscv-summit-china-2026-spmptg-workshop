#ifndef BAO_IPCSHMEM_H_
#define BAO_IPCSHMEM_H_

#include <zephyr/device.h>
#include <zephyr/sys/device_mmio.h>

typedef void (*bao_ipcshmem_callback_t)(const struct device *dev);

struct bao_ipcshmem_api {
    void (*read)(const struct device *dev, char *buf, size_t n);
    void (*write)(const struct device *dev, const char *write, size_t n);
    void (*notify)(const struct device *dev);
    unsigned (*id)(const struct device *dev);
    void (*irq_set_enable)(const struct device *dev);
    void (*irq_set_callback)(const struct device *dev, bao_ipcshmem_callback_t callback);
};

#define BAO_IPCSHMEM_API(dev) ((struct bao_ipcshmem_api *)((dev)->api))

static inline void bao_ipcshmem_write(const struct device *dev, char *buf, size_t n) {
    BAO_IPCSHMEM_API(dev)->write(dev, buf, n);
}

static inline void bao_ipcshmem_read(const struct device *dev, char *buf, size_t n) {
    BAO_IPCSHMEM_API(dev)->read(dev, buf, n);
}

static inline void bao_ipcshmem_notify(const struct device *dev) {
    BAO_IPCSHMEM_API(dev)->notify(dev);
}

static inline unsigned bao_ipcshmem_id(const struct device *dev) {
    return BAO_IPCSHMEM_API(dev)->id(dev);
}

static inline void bao_ipcshmem_irq_enable(const struct device *dev) {
    BAO_IPCSHMEM_API(dev)->irq_set_enable(dev);
}

static inline void bao_ipcshmem_irq_set_callback(const struct device *dev,
    bao_ipcshmem_callback_t callback)
{
    BAO_IPCSHMEM_API(dev)->irq_set_callback(dev, callback);
}

#endif /* BAO_IPCSHMEM_H_ */
