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