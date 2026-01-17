#ifndef VIRT_MMIO_H
#define VIRT_MMIO_H

#include <stdint.h>

#define VIRTIO_MAGIC        0x74726976  // "virt" in little-endian
#define QEMU_VENDOR_ID      0x554D4551  // "QEMU" in little-endian
#define VIRTIO_SIZE         0x1000
#define VIRTIO_SLOTS        8

#define ALIGN_TO_PAGE       (4096 - sizeof(struct virtq_used))

#define GPIO_BASE_ADDR       0x10000000UL
#define VIRT_MMIO(x)         (GPIO_BASE_ADDR + (x))


#define VIRT_MMIO_NET_HDR    VIRT_MMIO(0x1000)
#define VIRT_MMIO_BLOCK_HDR  VIRT_MMIO(0x2000)

/* 
VirtIO MMIO register offsets 
*/
#define VIRTIO_MMIO_STATUS              0x070 
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010 
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014 
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020 
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024 
#define VIRTIO_MMIO_CONFIG              0x100
/*
VirtIO MMIO Queue register offsets
*/
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW     0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH    0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW      0x0A0
#define VIRTIO_MMIO_QUEUE_USED_HIGH     0x0A4
/* 
    Virtio MMIO status bits 
*/
#define ACKNOWLEDGE 1
#define DRIVER      2
#define DRIVER_OK   4
#define FEATURES_OK 8
/*
    Virtio device feature bits
*/
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

#define DESC_QUEUE_SIZE 256
#define QUEUE_BUF_SIZE 2048

/*
    Descriptor flag bits
*/
// #define VIRTQ_DESC_F_READONLY 0 // device reads from buffer
#define VIRTQ_DESC_F_NEXT 1     // descriptor continues at next
#define VIRTQ_DESC_F_WRITE 2    // device writes into this buffer; if not set, device reads from it
#define VIRTQ_DESC_F_INDIRECT 4 // descriptor points to an array of descriptors (indirect)

typedef struct {
    uint32_t magic;             // always "virt" (0x74726976)
    uint32_t version;           // 1: VirtIO‑MMIO (legacy), 2: VirtIO‑PCI (modern)
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

// Virtio MMIO queue structure: each descpriptor describes a buffer
struct virtq_desc {
    uint64_t addr;      // Address of buffer (guest-physical)
    uint32_t len;       // Length
    uint16_t flags;     // bits set with VIRTQ_DESC_F_* 
    uint16_t next;      // We chain unused descriptors via this field
};

/*
Avail Ring:
    + Driver:
        1. Writes descriptor index into ring[...]
        2. Increments idx
    + Device:
        1. Watches idx
        2. Consumes new entries
*/
struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[DESC_QUEUE_SIZE]; // descriptor indices, must be of size queue_size
    // uint16_t used_event; // Only if VIRTIO_F_EVENT_IDX
};

/*
Used Ring:
    + Device:
        1. Writes entries here.
        2. Increments idx.
    + Driver:
        1. Polls idx.
        2. Processes any new ring[...] entries.
*/
struct virtq_used_elem {
    uint32_t id;  // index of start descriptor
    uint32_t len; // total bytes written/read (not including virt-io net header)
};

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[DESC_QUEUE_SIZE];
    // uint16_t avail_event;   // optional
};

// virtio queue
struct virtq {
    struct virtq_desc desc[DESC_QUEUE_SIZE];        // 16 bytes each
    struct virtq_avail avail;                       // 2 bytes + (2 * 256) + 2
    struct virtq_used used;                         // 4 bytes + (8 * 256) + 2
    uint8_t padding[ALIGN_TO_PAGE];                 // pad to multiple of page size
};

// Negotiated VIRTIO_NET_F_MAC, the device sets a stable MAC here
struct virtio_net_config {
    uint8_t  mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    // there may be more fields in newer versions
};

struct virt_driver {
    virt_mmio_device_t* device;
    struct virt_mmio_queue* queues;
    uint32_t num_queues;
    void (*init)(struct virt_driver* drv);
};
/*
minimal header for virtio net packets
Each received packet has:
    1. A virtio‑net header (L1, physical layer?)
    2. The actual Ethernet frame (L2/L3/L4)
*/
// Generic Segmentation Offload (GSO): allows guest kernel stacks to 
// transmit over-sized TCP segments that far exceed the kernel interface’s MTU
struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;           
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;       // maybe absent in some legacy setups
};

int8_t virt_mmio_net_virtq_init(virt_mmio_device_t* device);

void virt_print_device_info(virt_mmio_device_t* device);
void virt_find_all_devices(void);

uint32_t virt_mmio_read(virt_mmio_device_t* device, uint32_t offset);
void virt_mmio_write(virt_mmio_device_t* device, uint32_t offset, uint32_t value);
int8_t virt_mmio_negotiation(virt_mmio_device_t* device);

#endif // VIRT_MMIO_H