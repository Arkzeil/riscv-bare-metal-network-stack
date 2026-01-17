#include <driver/virt_mmio.h>
#include <kernel/uart.h>
#include <kernel/utils.h>
#include <kernel/mem.h>

static struct virtq *rx_queue;
static struct virtq *tx_queue;

int8_t virt_mmio_net_virtq_init(virt_mmio_device_t* device) {
    // Initialization specific to Virtio Network Device
    uart_puts("Initializing Virtio Network Device Queue...\n");
    
    // Setup RX queues
    rx_queue = mem_alloc(sizeof(struct virtq));
    if(rx_queue == NULL) {
        uart_puts("Failed to allocate memory for RX queue.\n");
        return -1;
    }
    // Setup TX queues
    tx_queue = mem_alloc(sizeof(struct virtq));
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
        rx_queue->desc[i].flags = 0; // Set appropriate flags
        rx_queue->desc[i].next = 0; // No next descriptor
        rx_queue->avail.ring[i] = i; // Initialize available ring

        tx_queue->desc[i].addr = (uint64_t)queue_buf + (i * QUEUE_BUF_SIZE);
        tx_queue->desc[i].len = QUEUE_BUF_SIZE; // Example buffer length
        tx_queue->desc[i].flags = 0; // Set appropriate flags
        tx_queue->desc[i].next = 0; // No next descriptor
        tx_queue->avail.ring[i] = i; // Initialize available ring
    }

    rx_queue->avail.idx = DESC_QUEUE_SIZE;
    rx_queue->used.idx = 0;

    tx_queue->avail.idx = 0;
    tx_queue->used.idx = 0;

    uart_puts("Virtio Network Device Queue initialized.\n");
    return 0;
}

void virt_mmio_net_send(const uint8_t* data, uint32_t length) {
    // Placeholder implementation for sending data
    uart_puts("Sending data via Virtio Network Device...\n");
    // Actual implementation would involve populating TX queue descriptors
}

void virt_mmio_net_poll_rx(uint8_t* buffer, uint32_t* length) {
    // Placeholder implementation for polling received data
    uart_puts("Polling for received data via Virtio Network Device...\n");
    // Actual implementation would involve checking RX queue descriptors
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
    uart_puts("Host Features: ");
    uart_b2x(device->host_features);
    uart_putc('\n');
    uart_puts("Guest Features: ");
    uart_b2x(device->guest_features);
    uart_putc('\n');
    uart_puts("Page Size: ");
    uart_b2x(device->page_size);
    uart_putc('\n');
    uart_puts("Number of Queues: ");
    uart_b2x(device->num_queues);
    uart_putc('\n');
    uart_puts("Queue Size: ");
    uart_b2x(device->queue_size);
    uart_putc('\n');
    uart_puts("Config Generation: ");
    uart_b2x(device->config_generation);
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
                virt_mmio_negotiation(device);
                virt_mmio_net_virtq_init(device);
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

// should only trust the values in the config space after 
// we have set the DRIVER_OK (4) bit in the Status register
static void virt_mmio_read_config(virt_mmio_device_t* device, uint32_t offset, void* buf, uint32_t len) {
    uint8_t* config_base = (uint8_t*)device + VIRTIO_MMIO_CONFIG;
    mem_copy(buf, config_base + offset, len);
}

int8_t virt_mmio_negotiation(virt_mmio_device_t* device) {
    uart_puts("Starting Virtio MMIO handshake...\n");

    uint32_t *mmio = (uint32_t *)((uintptr_t)device);
    uint32_t *status = mmio + (VIRTIO_MMIO_STATUS / 4);                     // Status register offset
    uint32_t *device_features = mmio + (VIRTIO_MMIO_DEVICE_FEATURES / 4);   // Device features offset
    uint32_t *device_features_sel = mmio + (VIRTIO_MMIO_DEVICE_FEATURES_SEL / 4); // Device features select offset
    uint32_t feat_low, feat_high;
    
    uint32_t *driver_queue_desc_low = mmio + (VIRTIO_MMIO_QUEUE_DESC_LOW / 4);      // Queue descriptor low offset
    uint32_t *driver_queue_desc_high = mmio + (VIRTIO_MMIO_QUEUE_DESC_HIGH / 4);    // Queue descriptor high offset
    uint32_t *driver_avail_low = mmio + (VIRTIO_MMIO_QUEUE_AVAIL_LOW / 4);          // Queue available ring low offset
    uint32_t *driver_avail_high = mmio + (VIRTIO_MMIO_QUEUE_AVAIL_HIGH / 4);        // Queue available ring high offset
    uint32_t *driver_used_low = mmio + (VIRTIO_MMIO_QUEUE_USED_LOW / 4);            // Queue used ring low offset
    uint32_t *driver_used_high = mmio + (VIRTIO_MMIO_QUEUE_USED_HIGH / 4);          // Queue used ring high offset
    uint32_t *driver_queue_size = mmio + (DESC_QUEUE_SIZE / 4);                     // Queue size offset
    uint32_t *driver_queue_select = mmio + (VIRTIO_MMIO_QUEUE_SEL / 4);             // Queue select offset
    uint32_t *driver_queue_ready = mmio + (VIRTIO_MMIO_QUEUE_READY / 4);             // Queue ready offset
    uint32_t drv_low, drv_high;

    w32(status, 0x0);                                           // Reset device
    w32(status, 1 << ACKNOWLEDGE);                                   // Acknowledge
    w32(status, (1 << ACKNOWLEDGE) | (1 << DRIVER));                                        // Driver
    // Read Features
    w32(device_features_sel, 0);                                // Select lower 32 bits
    uint32_t dev_feat_low = r32(device_features);
    w32(device_features_sel, 1);                                // Select higher 32 bits
    uint32_t dev_feat_high = r32(device_features);
    // choose features ()
    feat_low = dev_feat_low & (1 << VIRTIO_NET_F_MAC);           // Accept MAC feature
    feat_high = dev_feat_high & 0;                                               // No high features
    // Write back accepted features
    w32(device_features_sel, 0);                                // Select lower 32 bits
    w32(device_features, feat_low);
    w32(device_features_sel, 1);                                // Select higher 32 bits
    w32(device_features, feat_high);
    // Finalize initialization
    w32(status, *status | (1 << FEATURES_OK));                                   // features OK

    if(!(*status & FEATURES_OK)) {
        uart_puts("Virtio MMIO negotiation failed.\n");
        return -1;
    }

    // driver would set up queues and other structures
    w32(driver_queue_desc_low, (uint32_t)(rx_queue->desc) & 0xFFFFFFFF);
    w32(driver_queue_desc_high, (uint32_t)(rx_queue->desc) >> 32);
    w32(driver_avail_low, (uint32_t)(&(rx_queue->avail)) & 0xFFFFFFFF);
    w32(driver_avail_high, (uint32_t)(&(rx_queue->avail)) >> 32);
    w32(driver_used_low, (uint32_t)(&(rx_queue->used)) & 0xFFFFFFFF);
    w32(driver_used_high, (uint32_t)(&(rx_queue->used)) >> 32);

    w32(driver_queue_ready, 1); // mark queue as ready

    w32(status, *status | (1 << DRIVER_OK));                                     // driver OK

    uart_puts("Virtio MMIO negotiation completed.\n");
    return 0;
}
