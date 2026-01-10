#include <driver/virt_mmio.h>
#include <kernel/uart.h>

void virt_mmio_init(virt_mmio_device_t* device, uint32_t device_id, uint32_t vendor_id) {
    device->magic = 0x74726976; // "virt" in little-endian
    device->version = 2; // Virtio version 2
    device->device_id = device_id;
    device->vendor_id = vendor_id;
    device->host_features = 0;
    device->guest_features = 0;
    device->page_size = 4096; // 4KB page size
    device->num_queues = 1; // Default to 1 queue
    device->queue_size = 256; // Default queue size
    device->config_generation = 0;
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

    volatile uint32_t *mmio = (uint32_t *)((uintptr_t)device);
    uint32_t *status = mmio + (VIRTIO_MMIO_STATUS / 4); // Status register offset
    uint32_t *device_features = mmio + (VIRTIO_MMIO_DEVICE_FEATURES / 4); // Device features offset
    uint32_t *device_features_sel = mmio + (VIRTIO_MMIO_DEVICE_FEATURES_SEL / 4); // Device features select offset
    uint32_t drv_low, drv_high;

    *status = RESET;                                       // Reset device
    *status |= ACKNOWLEDGE;                                // Acknowledge
    *status |= DRIVER;                                     // Driver
    // driver would set up queues and other structures here (omitted for now)
    *status |= DRIVER_OK;                                  // driver OK
    // Read Features
    *device_features_sel = 0;                              // Select lower 32 bits
    uint32_t dev_feat_low = *device_features;
    *device_features_sel = 1;                              // Select higher 32 bits
    uint32_t dev_feat_high = *device_features;
    // choose features ()
    drv_low = dev_feat_low & (1 << VIRTIO_NET_F_MAC);       //Accept MAC feature
    drv_high = 0;                                          // No high features
    // Write back accepted features
    *device_features_sel = 0;                              // Select lower 32 bits
    *device_features = drv_low;
    *device_features_sel = 1;                              // Select higher 32 bits
    *device_features = drv_high;
    // Finalize initialization
    *status |= FEATURES_OK;                                // features OK

    if(!(*status & FEATURES_OK)) {
        uart_puts("Virtio MMIO negotiation failed.\n");
        return -1;
    }

    uart_puts("Virtio MMIO negotiation completed.\n");
    return 0;
}