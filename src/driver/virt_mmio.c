#include <driver/virt_mmio.h>
#include <kernel/uart.h>
#include <kernel/utils.h>
#include <kernel/mem.h>

static struct virtq *rx_queue;
static struct virtq *tx_queue;
static volatile uint32_t rx_last_used_idx = 0;

static uint8_t avail_wrap_count = 1;

#define ETH_P_IP   0x0800
#define ETH_P_ARP  0x0806
#define ETH_P_IPV6 0x86DD

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

// should only trust the values in the config space after 
// we have set the DRIVER_OK (4) bit in the Status register
static void virt_mmio_read_config(virt_mmio_device_t* device, uint32_t offset, void* buf, uint32_t len) {
    uint8_t* config_base = (uint8_t*)device + VIRTIO_MMIO_CONFIG;
    mem_copy(buf, config_base + offset, len);
}

int8_t virt_mmio_net_virtq_init(virt_mmio_device_t* device) {
    // Initialization specific to Virtio Network Device
    uart_puts("Initializing Virtio Network Device Queue...\n");
    
    // Setup RX queues
    rx_queue = mem_alloc(sizeof(struct virtq));
    uart_puts("Allocated memory for RX queue at: ");
    uart_b2x((uint32_t)(uintptr_t)rx_queue);
    uart_putc('\n');
    if(rx_queue == NULL) {
        uart_puts("Failed to allocate memory for RX queue.\n");
        return -1;
    }
    // Setup TX queues
    tx_queue = mem_alloc(sizeof(struct virtq));
    uart_puts("Allocated memory for TX queue at: ");
    uart_b2x((uint32_t)(uintptr_t)tx_queue);
    uart_putc('\n');
    if(tx_queue == NULL) {
        uart_puts("Failed to allocate memory for TX queue.\n");
        return -1;
    }

    uint8_t *queue_buf = mem_alloc(QUEUE_BUF_SIZE * DESC_QUEUE_SIZE); // Example buffer allocation
    if(queue_buf == NULL) {
        uart_puts("Failed to allocate memory for RX buffers.\n");
        return -1;
    }

    for(uint32_t i = 0; i < DESC_QUEUE_SIZE; i++) {
        rx_queue->desc[i].addr = (uint64_t)queue_buf + (i * QUEUE_BUF_SIZE);
        rx_queue->desc[i].len = QUEUE_BUF_SIZE; // Example buffer length
        rx_queue->desc[i].id = 0; // Set buffer id to index
        rx_queue->desc[i].flags = VIRTQ_DESC_F_WRITE; // Buffers must be device‑writable
        rx_queue->avail.ring[i] = i; // Initialize available ring
        if (avail_wrap_count) {
            rx_queue->desc[i].flags |= VIRTQ_DESC_F_AVAIL; // Mark as available to device
            rx_queue->desc[i].flags &= ~VIRTQ_DESC_F_USED; // Mark as not used by device
        }
        else {
            rx_queue->desc[i].flags |= VIRTQ_DESC_F_USED; // Mark as not available to device
            rx_queue->desc[i].flags &= ~VIRTQ_DESC_F_AVAIL; // Mark as not available to device
        }

        if(i != DESC_QUEUE_SIZE - 1) {
            rx_queue->desc[i].flags |= VIRTQ_DESC_F_NEXT; // Chain descriptors
        }

        tx_queue->desc[i].addr = 0;
        tx_queue->desc[i].len = QUEUE_BUF_SIZE; // Example buffer length
        tx_queue->desc[i].id = i; // Set buffer id to index
        tx_queue->desc[i].flags = 0; // TX descriptors are device‑read‑only
        tx_queue->avail.ring[i] = i; // Initialize available ring
    }

    rx_queue->avail.idx = 0;  // Start with 0 available descriptors
    rx_queue->used.idx = 0;

    tx_queue->avail.idx = 0;
    tx_queue->used.idx = 0;

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

void virt_mmio_net_send(const uint8_t* data, uint32_t length) {
    uart_puts("Sending data via Virtio Network Device...\n");

    // Find the next available descriptor
    uint16_t desc_index = tx_queue->avail.ring[tx_queue->avail.idx % DESC_QUEUE_SIZE];
    // Copy data to the descriptor buffer
    mem_copy((void*)(uintptr_t)tx_queue->desc[desc_index].addr, data, length);
    tx_queue->desc[desc_index].len = length;
    // Update available ring (available to device to consume)
    tx_queue->avail.idx++;
}

void virt_mmio_net_poll_rx(virt_mmio_device_t* device, uint8_t* buffer, uint32_t length) {
    uart_puts("Polling for received data via Virtio Network Device...\n");

    // Populate available ring with all RX descriptors and notify device
    // mem_barrier();
    // for(uint32_t i = 0; i < DESC_QUEUE_SIZE; i++) {
    //     rx_queue->avail.ring[rx_queue->avail.idx % DESC_QUEUE_SIZE] = i;
    //     rx_queue->avail.idx++;
    // }
    // mem_barrier();
    // // Notify device that RX descriptors are available (queue 0 = RX)
    // w32((uint32_t*)((uintptr_t)device + VIRTIO_MMIO_QUEUE_NOTIFY), 0);

    while(1) {
        mem_barrier();
        if(device->interrupt_status & 0x1) { // Check if interrupt is for RX
            uart_puts("Received interrupt for RX queue.\n");
            // w32((uint32_t*)((uintptr_t)device + VIRTIO_MMIO_INTERRUPT_ACK), 0x1); // Acknowledge RX interrupt
        }
        if(rx_queue->desc[0].flags & VIRTQ_DESC_F_USED) {
            uart_puts("Device marked descriptor 0 as used.\n");
        }
        if(rx_last_used_idx != rx_queue->used.idx) {
            uart_puts("Device updated used index to: ");
            uart_b2x(rx_queue->used.idx);
            uart_putc('\n');

            struct virtq_used_elem* used_elem = &(rx_queue->used.ring[rx_last_used_idx % DESC_QUEUE_SIZE]);
            uint32_t desc_index = used_elem->id;
            uint32_t len = used_elem->len;
            uint8_t *buf_addr = (uint8_t*)(uintptr_t)rx_queue->desc[desc_index].addr;
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
            }

            // Recycle descriptor back to available ring
            rx_queue->avail.ring[rx_queue->avail.idx % DESC_QUEUE_SIZE] = desc_index;
            rx_queue->avail.idx++;
            mem_barrier();
            rx_last_used_idx++;
            // Notify device about recycled descriptor
            w32((uint32_t*)((uintptr_t)device + VIRTIO_MMIO_QUEUE_NOTIFY), 0);

            uart_puts("Received data length: ");
            uart_b2x(frame_len);
            uart_putc('\n');
            goto exit;
        }
    }
exit:
    uart_puts("Finished polling for received data.\n");
}


void virt_print_device_info(virt_mmio_device_t* device) {
    uart_puts("Virtio MMIO Device Info:\n");
    uart_puts("Magic: ");
    uart_b2x(device->magic);
    uart_putc('\n');
    uart_puts("Version: ");
    uart_b2x(device->version);
    uart_putc('\n');
    uart_puts("Device ID: ");
    uart_b2x(device->device_id);
    uart_putc('\n');
    uart_puts("Vendor ID: ");
    uart_b2x(device->vendor_id);
    uart_putc('\n');
    uart_puts("Queue Max Size: ");
    uart_b2x(device->queue_num_max);
    uart_putc('\n');
    uart_puts("Host feature sel: ");
    uart_b2x(device->device_features_sel);
    uart_putc('\n');
    uart_puts("Host Features Low: ");
    uart_b2x(device->host_features);
    uart_putc('\n');
    uart_puts("Guest Features Low: ");
    uart_b2x(device->guest_features);
    uart_putc('\n');
    w32((uint32_t *)device + VIRTIO_MMIO_DEVICE_FEATURES_SEL / 4, 1); // Select higher 32 bits
    mem_barrier();
    wait_cycles(1000); // Ensure the device has time to update the features
    uart_puts("Host feature sel: ");
    uart_b2x(device->device_features_sel);
    uart_putc('\n');
    uart_puts("Host Features High: ");
    uart_b2x(device->host_features);
    uart_putc('\n');
    uart_puts("Guest Features High: ");
    uart_b2x(device->guest_features);
    uart_putc('\n');
}

void virt_find_all_devices(void) {
    uart_puts("Scanning for Virtio MMIO devices...\n");

    for(int i = 1; i <= VIRTIO_SLOTS; i++) {
        virt_mmio_device_t* device = (virt_mmio_device_t*)(VIRTIO_SIZE * i + GPIO_BASE_ADDR);
        if(device->magic == VIRTIO_MAGIC && device->vendor_id == QEMU_VENDOR_ID) {
            uart_puts("Slot ");
            uart_b2x(i);
            uart_puts(": ");
            switch(device->device_id) {
            case 0:
                uart_puts("Found placeholder.\n");
                break;
            case 1:
                uart_puts("Found Virtio Network Device.\n");
                virt_print_device_info(device);
                w32((uint32_t *)((uintptr_t)device) + (VIRTIO_MMIO_STATUS / 4), 0x0);  // Reset device
                virt_mmio_net_virtq_init(device);
                virt_mmio_negotiation(device);

                uint8_t mac_buf[6];
                virt_mmio_net_get_mac_address(device, mac_buf);
                
                virt_mmio_net_poll_rx(device, NULL, 2048); // Example: Poll for received data (replace with actual buffer and length)
                break;
            case 2:
                uart_puts("Found Virtio Block Device.\n");
                break;
            default:
                uart_puts("Found Unknown Virtio Device. Device ID: ");
                uart_b2x(device->device_id);
                uart_putc('\n');
            }
        }
    }
}

int8_t virt_mmio_negotiation(virt_mmio_device_t* device) {
    uart_puts("Starting Virtio MMIO handshake...\n");

    uint32_t *mmio = (uint32_t *)((uintptr_t)device);
    uint32_t *status = mmio + (VIRTIO_MMIO_STATUS / 4);                             // Status register offset
    uint32_t *device_features = mmio + (VIRTIO_MMIO_DEVICE_FEATURES / 4);           // Device features offset
    uint32_t *device_features_sel = mmio + (VIRTIO_MMIO_DEVICE_FEATURES_SEL / 4);   // Device features select offset
    uint32_t *driver_features = mmio + (VIRTIO_MMIO_DRIVER_FEATURES / 4);
    uint32_t *driver_features_sel = mmio + (VIRTIO_MMIO_DRIVER_FEATURES_SEL / 4);
    uint32_t feat_low, feat_high;
    
    uint32_t *driver_queue_desc_low = mmio + (VIRTIO_MMIO_QUEUE_DESC_LOW / 4);      // Queue descriptor low offset
    uint32_t *driver_queue_desc_high = mmio + (VIRTIO_MMIO_QUEUE_DESC_HIGH / 4);    // Queue descriptor high offset
    uint32_t *driver_avail_low = mmio + (VIRTIO_MMIO_QUEUE_AVAIL_LOW / 4);          // Queue available ring low offset
    uint32_t *driver_avail_high = mmio + (VIRTIO_MMIO_QUEUE_AVAIL_HIGH / 4);        // Queue available ring high offset
    uint32_t *driver_used_low = mmio + (VIRTIO_MMIO_QUEUE_USED_LOW / 4);            // Queue used ring low offset
    uint32_t *driver_used_high = mmio + (VIRTIO_MMIO_QUEUE_USED_HIGH / 4);          // Queue used ring high offset
    uint32_t *driver_queue_num = mmio + (VIRTIO_MMIO_QUEUE_NUM / 4);                // Queue size offset
    uint32_t *driver_queue_select = mmio + (VIRTIO_MMIO_QUEUE_SEL / 4);             // Queue select offset
    uint32_t *driver_queue_ready = mmio + (VIRTIO_MMIO_QUEUE_READY / 4);            // Queue ready offset

    w32(status, ACKNOWLEDGE);                                   // Acknowledge
    w32(status, ACKNOWLEDGE | DRIVER);                          // Driver
    // Read Features
    w32(device_features_sel, 0);                                // Select lower 32 bits
    uint32_t dev_feat_low = r32(device_features);
    w32(device_features_sel, 1);                                // Select higher 32 bits
    uint32_t dev_feat_high = r32(device_features);
    // choose features ()
    feat_low = dev_feat_low & VIRTIO_NET_F_MAC;                 // Accept MAC feature
    feat_high = dev_feat_high & (uint32_t)VIRTIO_F_VERSION_1;   // Accept version 1 feature
    feat_high |= VIRTIO_F_RING_PACKED; // Accept packed ring feature
    // Write back accepted features
    w32(driver_features_sel, 0);                                // Select lower 32 bits
    w32(driver_features, feat_low);
    w32(driver_features_sel, 1);                                // Select higher 32 bits
    w32(driver_features, feat_high);
    // Finalize initialization
    w32(status, *status | FEATURES_OK);                         // features OK

    for(int i = 0; i < 1000000; i++){
        asm volatile("nop");
    } // Simple delay loop to allow device to process features

    if(!(*status & FEATURES_OK)) {
        uart_puts("Virtio MMIO negotiation failed.\n");
        return -1;
    }

    uint32_t max_queue_size = 0;

    // driver would set up queues and other structures
    w32(driver_queue_select, 0); // select queue 0 (RX queue)
    uart_b2x(r32(&(device->queue_ready)));
    
    uint64_t desc_addr = (uint64_t)rx_queue->desc;
    w32(driver_queue_desc_low, desc_addr & 0xFFFFFFFF);
    w32(driver_queue_desc_high, desc_addr >> 32);
    uint64_t avail_addr = (uint64_t)&(rx_queue->avail);
    w32(driver_avail_low, avail_addr & 0xFFFFFFFF);
    w32(driver_avail_high, avail_addr >> 32);
    uint64_t used_addr = (uint64_t)&(rx_queue->used);
    w32(driver_used_low, used_addr & 0xFFFFFFFF);
    w32(driver_used_high, used_addr >> 32);
    w32(driver_queue_num, DESC_QUEUE_SIZE); // set queue size
    // w32(mmio + VIRTIO_MMIO_QUEUE_ALIGN / 4, 4096);
    // uint32_t pfn = ((uintptr_t)rx_queue) >> PAGE_SHIFT;
    // w32(mmio + (VIRTIO_MMIO_QUEUE_PFN / 4), pfn);
    w32(driver_queue_ready, 1); // mark queue as ready

    max_queue_size = r32(mmio + (VIRTIO_MMIO_QUEUE_NUM_MAX / 4));
    uart_b2x(max_queue_size);
    uart_putc('\n');

    w32(driver_queue_select, 1); // select queue 1 (TX queue)
    desc_addr = (uint64_t)tx_queue->desc;
    w32(driver_queue_desc_low, desc_addr & 0xFFFFFFFF);
    w32(driver_queue_desc_high, desc_addr >> 32);
    avail_addr = (uint64_t)&(tx_queue->avail);
    w32(driver_avail_low, avail_addr & 0xFFFFFFFF);
    w32(driver_avail_high, avail_addr >> 32);
    used_addr = (uint64_t)&(tx_queue->used);
    w32(driver_used_low, used_addr & 0xFFFFFFFF);
    w32(driver_used_high, used_addr >> 32);
    w32(driver_queue_num, DESC_QUEUE_SIZE); // set queue size
    w32(driver_queue_ready, 1); // mark queue as ready

    max_queue_size = r32(mmio + (VIRTIO_MMIO_QUEUE_NUM_MAX / 4));
    uart_b2x(max_queue_size);
    uart_putc('\n');

    w32(driver_queue_select, 2);
    max_queue_size = r32(mmio + (VIRTIO_MMIO_QUEUE_NUM_MAX / 4));
    uart_b2x(max_queue_size);
    uart_putc('\n');

    w32(status, *status | DRIVER_OK);                                     // driver OK
    uart_puts("Virtio MMIO negotiation completed.\n");
    return 0;
}
