ARCH = riscv64
CROSS_PLATFORM = $(ARCH)-unknown-elf
CC = gcc
KERNEL = kernel

CFLAGS = -march=rv64gc -mabi=lp64 -mcmodel=medany -Wall -nostdlib -nostartfiles -ffreestanding -nostdinc -g
CFLAGS += -I$(shell $(CROSS_PLATFORM)-$(CC) -print-file-name=include)
CFLAGS += -Iinclude

KER_PATH = src/kernel
ASM_PATH = src/asm
DRV_PATH = src/driver

KER_HEADERS = $(wildcard include/kernel/*.h)
KER_SRC = $(wildcard src/kernel/*.c)
KER_OBJ = $(KER_SRC:src/kernel/%.c=build/kernel/%.o)

DRV_SRC = $(wildcard src/driver/*.c)
DRV_OBJ = $(DRV_SRC:src/driver/%.c=build/driver/%.o)

ASM_SRC = $(wildcard src/asm/*.S)
ASM_OBJ = $(ASM_SRC:src/asm/%.S=build/asm/%.o)

build/kernel/%.o: $(KER_PATH)/%.c $(KER_HEADERS)
	if [ ! -d build ]; then mkdir build; fi
	if [ ! -d build/kernel ]; then mkdir build/kernel; fi
	$(CROSS_PLATFORM)-gcc $(CFLAGS) -c $< -o $@

build/asm/%.o: $(ASM_PATH)/%.S
	if [ ! -d build ]; then mkdir build; fi
	if [ ! -d build/asm ]; then mkdir build/asm; fi
	$(CROSS_PLATFORM)-gcc $(CFLAGS) -c $< -o $@

build/driver/%.o: $(DRV_PATH)/%.c
	if [ ! -d build ]; then mkdir build; fi
	if [ ! -d build/driver ]; then mkdir build/driver; fi
	$(CROSS_PLATFORM)-gcc $(CFLAGS) -c $< -o $@

all: build/$(KERNEL).elf

build/$(KERNEL).elf: $(KER_OBJ) $(DRV_OBJ) $(ASM_OBJ)
	$(CROSS_PLATFORM)-ld -g -T $(KER_PATH)/linker.ld -o $@ $^

clean:
	rm -rf build

# `-nographic` is conflict with `-serial stdio`, so we use `-display none` instead
# The virt machine simulate UART on serial port 0 by default, so we map it to stdio
run:
	qemu-system-$(ARCH) -machine virt -display none -bios none \
	-kernel build/$(KERNEL).elf -serial stdio

net:
	qemu-system-riscv64 -machine virt -display none -bios none \
	-kernel build/$(KERNEL).elf -serial stdio \
	-netdev user,id=net0,hostfwd=udp::5555-:5555 \
	-device virtio-net-device,netdev=net0

debug:
	qemu-system-$(ARCH) -machine virt -display none -bios none \
	-kernel build/$(KERNEL).elf -serial stdio -S -s

# tell QEMU to dump dtb to a file and use dtc to convert to human-readable dts format
# dtc can be installed via `sudo apt install device-tree-compiler`
# no space after the comma in dumpdtb=location
dtb:
	qemu-system-$(ARCH) -machine virt,dumpdtb=build/virt.dtb \
	-display none -bios none -kernel build/$(KERNEL).elf -serial stdio
	dtc -I dtb -O dts -o virt.dts build/virt.dtb

dump:
	$(CROSS_PLATFORM)-objdump -d build/$(KERNEL).elf

dump_txt:
	$(CROSS_PLATFORM)-objdump -d build/$(KERNEL).elf > build/$(KERNEL)_dump.txt