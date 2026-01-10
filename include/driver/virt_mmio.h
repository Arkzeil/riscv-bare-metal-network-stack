#ifndef VIRT_MMIO_H
#define VIRT_MMIO_H

#include <stdint.h>

#define VIRTIO_MAGIC       0x74726976  // "virt" in little-endian
#define QEMU_VENDOR_ID    0x554D4551  // "QEMU" in little-endian
#define VIRTIO_SIZE         0x1000
#define VIRTIO_SLOTS        8

#define GPIO_BASE_ADDR       0x10000000UL
#define VIRT_MMIO(x)         (GPIO_BASE_ADDR + (x))


#define VIRT_MMIO_NET_HDR    VIRT_MMIO(0x1000)
#define VIRT_MMIO_BLOCK_HDR  VIRT_MMIO(0x2000)

#define VIRT_REG

typedef struct {
    uint32_t magic;             // always "virt" (0x74726976)
    uint32_t version;           // should be 2 for virtio 1.0+
    uint32_t device_id;         // 1: network, 2: block, 3: console, etc.
    uint32_t vendor_id;         // 0x554D4551 (“QEMU”)
    uint32_t host_features;
    uint32_t guest_features;
    uint32_t page_size;
    uint32_t num_queues;
    uint32_t queue_size;
    uint32_t config_generation;
    // Device-specific configuration space follows
} virt_mmio_device_t;

void virt_mmio_init(virt_mmio_device_t* device, uint32_t device_id, uint32_t vendor_id);
uint32_t virt_mmio_read(virt_mmio_device_t* device, uint32_t offset);
void virt_mmio_write(virt_mmio_device_t* device, uint32_t offset, uint32_t value);

void virt_print_device_info(virt_mmio_device_t* device);
void virt_find_all_devices(void);

struct virt_mmio_queue {
    uint16_t size;
    uint16_t align;
    uint64_t desc_table_addr;
    uint64_t avail_ring_addr;
    uint64_t used_ring_addr;
};

struct virt_driver {
    virt_mmio_device_t* device;
    struct virt_mmio_queue* queues;
    uint32_t num_queues;
    void (*init)(struct virt_driver* drv);
};

#endif // VIRT_MMIO_H