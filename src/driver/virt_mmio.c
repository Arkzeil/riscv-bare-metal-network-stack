#include <driver/virt_mmio.h>
#include <kernel/uart.h>
#include <kernel/utils.h>
#include <kernel/mem.h>

enum virtio_net_queue_index {
    VIRTIO_NET_RX_QUEUE = 0,
    VIRTIO_NET_TX_QUEUE = 1,
    VIRTIO_NET_QUEUE_COUNT = 2,
};

/* Driver-owned state for each split virtqueue.  The device owns used.idx. */
struct virtio_net_queue {
    struct virtq *ring;
    uint8_t *buffers;
    uint16_t size;
    uint16_t last_used_idx;
    uint16_t next_desc;
    uint8_t in_use[DESC_QUEUE_SIZE];
};

static struct virtio_net_queue net_queues[VIRTIO_NET_QUEUE_COUNT];

#define ETH_P_IP   0x0800
#define ETH_P_ARP  0x0806
#define ETH_P_IPV6 0x86DD

#ifdef QEMU_NIC
uint8_t my_ip[4] = {10, 0, 2, 15};
#endif

struct eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

struct ipv4_hdr {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t off;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t sum;
    uint32_t src;
    uint32_t dst;
} __attribute__((packed));

struct arp_hdr {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];
    uint32_t spa;
    uint8_t  tha[6];
    uint32_t tpa;
} __attribute__((packed));

static inline uint16_t ntohs(uint16_t n) {
    return (uint16_t)((n << 8) | (n >> 8));
}

static inline uint32_t ntohl(uint32_t n) {
    return BE2LE(n);
}

static void print_mac(uint8_t *mac) {
    for(int i = 0; i < 6; i++) {
        uint8_t b = mac[i];
        uint8_t h = (b >> 4) & 0xF;
        uint8_t l = b & 0xF;
        uart_putc(h > 9 ? h + 'A' - 10 : h + '0');
        uart_putc(l > 9 ? l + 'A' - 10 : l + '0');
        if(i < 5) uart_putc(':');
    }
}

static void print_ip(uint32_t ip) {
    uint8_t *b = (uint8_t *)&ip;
    for(int i = 0; i < 4; i++) {
        uint8_t val = b[i];
        if (val >= 100) {
            uart_putc(val / 100 + '0');
            uart_putc((val / 10) % 10 + '0');
            uart_putc(val % 10 + '0');
        } else if (val >= 10) {
            uart_putc(val / 10 + '0');
            uart_putc(val % 10 + '0');
        } else {
            uart_putc(val + '0');
        }
        if(i < 3) uart_putc('.');
    }
}

static void parse_arp(struct arp_hdr *arp) {
    uart_puts("  ARP: ");
    if (ntohs(arp->oper) == 1) {
        uart_puts("Request for ");
        print_ip(arp->tpa);
        uart_puts(" from ");
        print_ip(arp->spa);
        uart_putc('\n');
    } else if (ntohs(arp->oper) == 2) {
        uart_puts("Reply from ");
        print_ip(arp->spa);
        uart_puts(" to ");
        print_ip(arp->tpa);
        uart_putc('\n');
    } else {
        uart_puts("Unknown ARP operation: ");
        uart_b2x(ntohs(arp->oper));
        uart_putc('\n');
    }
}

static void build_virtio_net_hdr(struct virtio_net_hdr *hdr, uint16_t flags, uint16_t gso_type, uint16_t hdr_len, uint16_t gso_size, uint16_t csum_start, uint16_t csum_offset) {
    hdr->flags = flags;
    hdr->gso_type = gso_type;
    hdr->hdr_len = hdr_len;
    hdr->gso_size = gso_size;
    hdr->csum_start = csum_start;
    hdr->csum_offset = csum_offset;
}

static void build_eth_frame(uint8_t *buffer, uint8_t *dst_mac, uint8_t *src_mac, uint16_t eth_type, const uint8_t *payload, uint16_t payload_len) {
    struct eth_hdr *eth = (struct eth_hdr *)buffer;
    for(int i = 0; i < 6; i++) {
        eth->dst[i] = dst_mac[i];
        eth->src[i] = src_mac[i];
    }
    eth->type = ntohs(eth_type);
    mem_copy(buffer + sizeof(struct eth_hdr), payload, payload_len);
}

static void build_arp_response(uint8_t *buffer, uint8_t *src_mac, uint32_t src_ip, uint8_t *dst_mac, uint32_t dst_ip) {
    struct arp_hdr *arp = (struct arp_hdr *)buffer;
    arp->htype = ntohs(1); // Ethernet
    arp->ptype = ntohs(ETH_P_IP);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = ntohs(2); // ARP Reply
    for(int i = 0; i < 6; i++) {
        arp->sha[i] = src_mac[i];
        arp->tha[i] = dst_mac[i];
    }
    arp->spa = src_ip;
    arp->tpa = dst_ip;
}

uint32_t virt_mmio_read(virt_mmio_device_t* device, uint32_t offset) {
    uint32_t *mmio = (uint32_t *)((uintptr_t)device);
    return r32(mmio + (offset / 4));
}

void virt_mmio_write(virt_mmio_device_t* device, uint32_t offset, uint32_t value) {
    uint32_t *mmio = (uint32_t *)((uintptr_t)device);
    w32(mmio + (offset / 4), value);
}

// should only trust the values in the config space after 
// we have set the DRIVER_OK (4) bit in the Status register
static void virt_mmio_read_config(virt_mmio_device_t* device, uint32_t offset, void* buf, uint32_t len) {
    uint8_t* config_base = (uint8_t*)device + VIRTIO_MMIO_CONFIG;
    mem_copy(buf, config_base + offset, len);
}

static void mem_zero(void *buffer, uint32_t length) {
    uint8_t *bytes = buffer;
    for (uint32_t i = 0; i < length; i++) {
        bytes[i] = 0;
    }
}

int8_t virt_mmio_net_virtq_init(virt_mmio_device_t* device) {
    (void)device;
    uart_puts("Initializing Virtio Network Device Queue...\n");

    for (uint32_t queue_index = 0; queue_index < VIRTIO_NET_QUEUE_COUNT; queue_index++) {
        struct virtio_net_queue *queue = &net_queues[queue_index];
        queue->ring = mem_alloc(sizeof(*queue->ring));
        queue->buffers = mem_alloc(QUEUE_BUF_SIZE * DESC_QUEUE_SIZE);
        if (queue->ring == NULL || queue->buffers == NULL) {
            uart_puts("Failed to allocate Virtio queue memory.\n");
            return -1;
        }

        mem_zero(queue->ring, sizeof(*queue->ring));
        mem_zero(queue->buffers, QUEUE_BUF_SIZE * DESC_QUEUE_SIZE);
        queue->size = DESC_QUEUE_SIZE;
        queue->last_used_idx = 0;
        queue->next_desc = 0;

        for (uint16_t i = 0; i < queue->size; i++) {
            queue->ring->desc[i].addr = (uint64_t)(uintptr_t)queue->buffers +
                                        ((uint64_t)i * QUEUE_BUF_SIZE);
            queue->ring->desc[i].len = QUEUE_BUF_SIZE;
            queue->ring->desc[i].flags =
                queue_index == VIRTIO_NET_RX_QUEUE ? VIRTQ_DESC_F_WRITE : 0;
        }
    }

    struct virtio_net_queue *rx = &net_queues[VIRTIO_NET_RX_QUEUE];
    for (uint16_t i = 0; i < rx->size; i++) {
        rx->ring->avail.ring[i] = i;
    }
    rx->ring->avail.idx = rx->size;

    mem_barrier();

    uart_puts("Virtio Network Device Queue initialized.\n");
    return 0;
}

void virt_mmio_net_get_mac_address(virt_mmio_device_t* device, uint8_t* mac_buf) {
    // Read MAC address from device configuration space
    virt_mmio_read_config(device, 0x0, mac_buf, 6); // MAC address is typically at offset 0x14
    uart_puts("MAC Address: ");
    for(int i = 0; i < 6; i++) {
        uart_b2x(mac_buf[i]);
        if(i < 5) uart_putc(':');
    }
    uart_putc('\n');
}

void virt_mmio_net_send(virt_mmio_device_t* device, const uint8_t* data, uint32_t length) {
    uart_puts("Sending data via Virtio Network Device...\n");
    struct virtio_net_queue *tx = &net_queues[VIRTIO_NET_TX_QUEUE];
    if (length > QUEUE_BUF_SIZE) {
        uart_puts("TX packet exceeds queue buffer size.\n");
        return;
    }

    uint16_t desc_index = tx->next_desc;
    for (uint16_t searched = 0; searched < tx->size; searched++) {
        if (!tx->in_use[desc_index]) {
            break;
        }
        desc_index = (desc_index + 1) % tx->size;
    }
    if (tx->in_use[desc_index]) {
        uart_puts("No free TX descriptors.\n");
        return;
    }

    mem_copy((void *)(uintptr_t)tx->ring->desc[desc_index].addr, data, length);
    tx->ring->desc[desc_index].len = length;
    tx->ring->desc[desc_index].flags = 0;
    tx->ring->avail.ring[tx->ring->avail.idx % tx->size] = desc_index;
    tx->in_use[desc_index] = 1;
    tx->next_desc = (desc_index + 1) % tx->size;
    mem_barrier();
    tx->ring->avail.idx++;
    mem_barrier();
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_NET_TX_QUEUE);
}

void virt_mmio_net_poll_rx(virt_mmio_device_t* device, uint8_t* buffer, uint32_t length) {
    uart_puts("Polling for received data via Virtio Network Device...\n");
    struct virtio_net_queue *rx = &net_queues[VIRTIO_NET_RX_QUEUE];
    (void)buffer;

    while(1) {
        mem_barrier();
        // if(device->interrupt_status & 0x1) { // Check if interrupt is for RX
        //     uart_puts("Received interrupt for RX queue.\n");
            // w32((uint32_t*)((uintptr_t)device + VIRTIO_MMIO_INTERRUPT_ACK), 0x1); // Acknowledge RX interrupt
        // }
        if(rx->last_used_idx != rx->ring->used.idx) {
            uart_puts("Device updated used index to: ");
            uart_b2x(rx->ring->used.idx);
            uart_putc('\n');

            struct virtq_used_elem* used_elem = &(rx->ring->used.ring[rx->last_used_idx % rx->size]);
            uint32_t desc_index = used_elem->id;
            uint32_t len = used_elem->len;
            if (desc_index >= rx->size) {
                uart_puts("Device returned an invalid RX descriptor.\n");
                return;
            }
            uint8_t *buf_addr = (uint8_t*)(uintptr_t)rx->ring->desc[desc_index].addr;
            if(len > length) {
                len = length; // Truncate if buffer is smaller
            }
            // mem_copy(buffer, buf_addr, len);

            uint8_t *frame = buf_addr + sizeof(struct virtio_net_hdr);
            uint32_t frame_len = len - sizeof(struct virtio_net_hdr);
            
            struct eth_hdr *eth = (struct eth_hdr *)frame;
            uint16_t eth_type = ntohs(eth->type);

            uart_puts("Received Frame [");
            print_mac(eth->src);
            uart_puts(" -> ");
            print_mac(eth->dst);
            uart_puts("] type=0x");
            uart_b2x(eth_type);
            uart_putc('\n');

            if (eth_type == ETH_P_IP) {
                struct ipv4_hdr *ip = (struct ipv4_hdr *)(frame + sizeof(struct eth_hdr));
                uart_puts("  IPv4: ");
                print_ip(ip->src);
                uart_puts(" -> ");
                print_ip(ip->dst);
                uart_puts(" proto=");
                uart_b2x(ip->proto);
                uart_putc('\n');
            } else if (eth_type == ETH_P_ARP) {
                uart_puts("  ARP Packet detected\n");
                parse_arp((struct arp_hdr *)(frame + sizeof(struct eth_hdr)));
            }

            // Recycle descriptor back to available ring
            rx->ring->avail.ring[rx->ring->avail.idx % rx->size] = desc_index;
            mem_barrier();
            rx->ring->avail.idx++;
            rx->last_used_idx++;
            // Notify device about recycled descriptor
            mem_barrier();
            virt_mmio_write(device, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_NET_RX_QUEUE);

            uart_puts("Received data length: ");
            uart_b2x(frame_len);
            uart_putc('\n');
            goto exit;
        }
    }
exit:
    uart_puts("Finished polling for received data.\n");
}

int8_t virt_mmio_net_tx(virt_mmio_device_t* device, const uint8_t* data, uint32_t length) {
    uint8_t *packet = mem_alloc(length + sizeof(struct arp_hdr) + sizeof(struct eth_hdr) + sizeof(struct virtio_net_hdr));
    uint8_t *p = packet;
    if(packet == NULL) {
        uart_puts("Failed to allocate memory for TX packet.\n");
        return -1;
    }

    build_virtio_net_hdr((struct virtio_net_hdr *)p, 0, 0, 0, 0, 0, 0); // No special flags or GSO
    build_eth_frame(p += sizeof(struct virtio_net_hdr), (uint8_t[]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, (uint8_t[]){0xDE,0xAD,0xBE,0xEF,0x00,0x01}, ETH_P_IP, data, length);
    build_arp_response(p += sizeof(struct eth_hdr), (uint8_t[]){0xDE,0xAD,0xBE,0xEF,0x00,0x01}, ntohl(*(uint32_t*)my_ip), (uint8_t[]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, 0); // Example ARP response
    mem_copy(p + sizeof(struct arp_hdr), data, length); // Copy actual payload after ARP

    virt_mmio_net_send(device, data, length);
    uart_puts("Data sent, waiting for completion...\n");

    // Wait for the device to process the TX descriptor
    while(1) {
        mem_barrier();
        struct virtio_net_queue *tx = &net_queues[VIRTIO_NET_TX_QUEUE];
        if(tx->last_used_idx != tx->ring->used.idx) {
            uart_puts("Device processed TX descriptor.\n");
            struct virtq_used_elem *used =
                &tx->ring->used.ring[tx->last_used_idx % tx->size];
            if (used->id < tx->size) {
                tx->in_use[used->id] = 0;
            }
            tx->last_used_idx++;
            break;
        }
    }

    return 0;
}


void virt_print_device_info(virt_mmio_device_t* device) {
    uart_puts("Virtio MMIO Device Info:\n");
    uart_puts("Magic: ");
    uart_b2x(virt_mmio_read(device, 0x000));
    uart_putc('\n');
    uart_puts("Version: ");
    uart_b2x(virt_mmio_read(device, 0x004));
    uart_putc('\n');
    uart_puts("Device ID: ");
    uart_b2x(virt_mmio_read(device, 0x008));
    uart_putc('\n');
    uart_puts("Vendor ID: ");
    uart_b2x(virt_mmio_read(device, 0x00c));
    uart_putc('\n');
    uart_puts("Queue Max Size: ");
    uart_b2x(virt_mmio_read(device, VIRTIO_MMIO_QUEUE_NUM_MAX));
    uart_putc('\n');
    uart_puts("Host feature sel: ");
    uart_b2x(virt_mmio_read(device, VIRTIO_MMIO_DEVICE_FEATURES_SEL));
    uart_putc('\n');
    uart_puts("Host Features Low: ");
    uart_b2x(virt_mmio_read(device, VIRTIO_MMIO_DEVICE_FEATURES));
    uart_putc('\n');
    uart_puts("Guest Features Low: ");
    uart_b2x(virt_mmio_read(device, VIRTIO_MMIO_DRIVER_FEATURES));
    uart_putc('\n');
    virt_mmio_write(device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uart_puts("Host feature sel: ");
    uart_b2x(virt_mmio_read(device, VIRTIO_MMIO_DEVICE_FEATURES_SEL));
    uart_putc('\n');
    uart_puts("Host Features High: ");
    uart_b2x(virt_mmio_read(device, VIRTIO_MMIO_DEVICE_FEATURES));
    uart_putc('\n');
    uart_puts("Guest Features High: ");
    uart_b2x(virt_mmio_read(device, VIRTIO_MMIO_DRIVER_FEATURES));
    uart_putc('\n');
}

void virt_find_all_devices(void) {
    uart_puts("Scanning for Virtio MMIO devices...\n");

    for(int i = 1; i <= VIRTIO_SLOTS; i++) {
        virt_mmio_device_t* device = (virt_mmio_device_t*)(VIRTIO_SIZE * i + GPIO_BASE_ADDR);
        if(virt_mmio_read(device, 0x000) == VIRTIO_MAGIC &&
           virt_mmio_read(device, 0x00c) == QEMU_VENDOR_ID) {
            uart_puts("Slot ");
            uart_b2x(i);
            uart_puts(": ");
            switch(virt_mmio_read(device, 0x008)) {
            case 0:
                uart_puts("Found placeholder.\n");
                break;
            case 1:
                uart_puts("Found Virtio Network Device.\n");
                virt_print_device_info(device);
                virt_mmio_write(device, VIRTIO_MMIO_STATUS, 0);
                if (virt_mmio_net_virtq_init(device) != 0 ||
                    virt_mmio_negotiation(device) != 0) {
                    uart_puts("Virtio network initialization failed.\n");
                    break;
                }

                uint8_t mac_buf[6];
                virt_mmio_net_get_mac_address(device, mac_buf);
                
                virt_mmio_net_poll_rx(device, NULL, 2048); // Example: Poll for received data (replace with actual buffer and length)
                // virt_mmio_net_tx(device, (const uint8_t*)"Hello, Virtio!", 14); // Example: Send data (replace with actual data and length)
                break;
            case 2:
                uart_puts("Found Virtio Block Device.\n");
                break;
            default:
                uart_puts("Found Unknown Virtio Device. Device ID: ");
                uart_b2x(virt_mmio_read(device, 0x008));
                uart_putc('\n');
            }
        }
    }
}

static int8_t virt_mmio_setup_queue(virt_mmio_device_t *device,
                                    uint16_t queue_index,
                                    struct virtio_net_queue *queue) {
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_SEL, queue_index);
    uint32_t max_size = virt_mmio_read(device, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_size < queue->size) {
        uart_puts("Virtio queue is smaller than the driver queue.\n");
        return -1;
    }

    uint64_t desc_addr = (uint64_t)(uintptr_t)queue->ring->desc;
    uint64_t avail_addr = (uint64_t)(uintptr_t)&queue->ring->avail;
    uint64_t used_addr = (uint64_t)(uintptr_t)&queue->ring->used;
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_NUM, queue->size);
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)desc_addr);
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint32_t)(desc_addr >> 32));
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)avail_addr);
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32_t)(avail_addr >> 32));
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)used_addr);
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_USED_HIGH, (uint32_t)(used_addr >> 32));
    mem_barrier();
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_READY, 1);
    return 0;
}

int8_t virt_mmio_negotiation(virt_mmio_device_t* device) {
    uart_puts("Starting Virtio MMIO handshake...\n");
    virt_mmio_write(device, VIRTIO_MMIO_STATUS, ACKNOWLEDGE);
    virt_mmio_write(device, VIRTIO_MMIO_STATUS, ACKNOWLEDGE | DRIVER);

    virt_mmio_write(device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t feat_low = virt_mmio_read(device, VIRTIO_MMIO_DEVICE_FEATURES) & VIRTIO_NET_F_MAC;
    virt_mmio_write(device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t feat_high = virt_mmio_read(device, VIRTIO_MMIO_DEVICE_FEATURES);
    if (!(feat_high & VIRTIO_F_VERSION_1)) {
        uart_puts("Virtio MMIO device does not support VERSION_1.\n");
        return -1;
    }

    virt_mmio_write(device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    virt_mmio_write(device, VIRTIO_MMIO_DRIVER_FEATURES, feat_low);
    virt_mmio_write(device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    virt_mmio_write(device, VIRTIO_MMIO_DRIVER_FEATURES, VIRTIO_F_VERSION_1);
    uint32_t status = virt_mmio_read(device, VIRTIO_MMIO_STATUS);
    virt_mmio_write(device, VIRTIO_MMIO_STATUS, status | FEATURES_OK);
    if (!(virt_mmio_read(device, VIRTIO_MMIO_STATUS) & FEATURES_OK)) {
        uart_puts("Virtio MMIO negotiation failed.\n");
        return -1;
    }

    for (uint16_t i = 0; i < VIRTIO_NET_QUEUE_COUNT; i++) {
        if (virt_mmio_setup_queue(device, i, &net_queues[i]) != 0) {
            return -1;
        }
    }

    status = virt_mmio_read(device, VIRTIO_MMIO_STATUS);
    virt_mmio_write(device, VIRTIO_MMIO_STATUS, status | DRIVER_OK);
    mem_barrier();
    virt_mmio_write(device, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_NET_RX_QUEUE);
    uart_puts("Virtio MMIO negotiation completed.\n");
    return 0;
}
