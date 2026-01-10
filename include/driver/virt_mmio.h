#ifndef VIRT_MMIO_H
#define VIRT_MMIO_H

#include <stdint.h>

#define VIRTIO_MAGIC        0x74726976  // "virt" in little-endian
#define QEMU_VENDOR_ID      0x554D4551  // "QEMU" in little-endian
#define VIRTIO_SIZE         0x1000
#define VIRTIO_SLOTS        8

#define ALIGN_TO_PAGE      (4096 - sizeof(struct virtq_used))

#define GPIO_BASE_ADDR       0x10000000UL
#define VIRT_MMIO(x)         (GPIO_BASE_ADDR + (x))


#define VIRT_MMIO_NET_HDR    VIRT_MMIO(0x1000)
#define VIRT_MMIO_BLOCK_HDR  VIRT_MMIO(0x2000)

// Virtio MMIO register offsets
#define VIRTIO_MMIO_STATUS              0x070 
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010 
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014 
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020 
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024 
// Virtio MMIO status bits
#define RESET       0
#define ACKNOWLEDGE 1 
#define DRIVER      2 
#define FEATURES_OK 8 
#define DRIVER_OK   4
// Virtio device feature bits
#define VIRTIO_F_NOTIFY_ON_EMPTY    0
#define VIRTIO_F_ANY_LAYOUT         5
#define VIRTIO_F_RING_INDIRECT_DESC 6
#define VIRTIO_F_RING_EVENT_IDX     7
#define VIRTIO_NET_F_MAC            15
#define VIRTIO_NET_F_GSO            16
#define VIRTIO_NET_F_GUEST_CSUM     17
#define VIRTIO_NET_F_CTRL_VQ        18
#define VIRTIO_NET_F_CTRL_RX        19
#define VIRTIO_NET_F_CTRL_VLAN      20
#define VIRTIO_NET_F_CTRL_RX_EXTRA  21
#define VIRTIO_NET_F_GUEST_TSO4     22
#define VIRTIO_NET_F_GUEST_TSO6     23
#define VIRTIO_NET_F_GUEST_ECN      24
#define VIRTIO_NET_F_GUEST_UFO      25
#define VIRTIO_NET_F_HOST_TSO4      27
#define VIRTIO_NET_F_HOST_TSO6      28
#define VIRTIO_NET_F_HOST_ECN       29
#define VIRTIO_NET_F_HOST_UFO       30

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

void virt_print_device_info(virt_mmio_device_t* device);
void virt_find_all_devices(void);

void virt_mmio_init(virt_mmio_device_t* device, uint32_t device_id, uint32_t vendor_id);
uint32_t virt_mmio_read(virt_mmio_device_t* device, uint32_t offset);
void virt_mmio_write(virt_mmio_device_t* device, uint32_t offset, uint32_t value);
int8_t virt_mmio_negotiation(virt_mmio_device_t* device);

struct virtq_desc {
    uint64_t addr;   // Address (guest-physical)
    uint32_t len;    // Length
    uint16_t flags;  // The flags as indicated above
    uint16_t next;   // We chain unused descriptors via this field
};

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[256]; // Must be of size queue_size
    // uint16_t used_event; // Only if VIRTIO_F_EVENT_IDX
};

struct virtq_used {
    uint32_t id;  // Index of start of used descriptor chain
    uint32_t len; // Total length of the descriptor chain which was used (written to)
};

struct virtq {
    struct virtq_desc desc[256];     // 16 bytes each
    struct virtq_avail avail;        // 2 bytes + (2 * 256) + 2
    uint8_t padding[ALIGN_TO_PAGE];  // Padding to 4KB boundary
    struct virtq_used used;          // 4 bytes + (8 * 256) + 2
};

struct virt_driver {
    virt_mmio_device_t* device;
    struct virt_mmio_queue* queues;
    uint32_t num_queues;
    void (*init)(struct virt_driver* drv);
};

#endif // VIRT_MMIO_H