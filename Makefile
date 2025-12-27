ARCH = riscv64
CROSS_PLATFORM = $(ARCH)-unknown-elf
CC = gcc
KERNEL = kernel

CFLAGS = -march=rv64gc -mabi=lp64 -Wall -nostdlib -nostartfiles -ffreestanding -nostdinc -g

KER_PATH = src/kernel

KER_HEADERS = $(wildcard include/kernel/*.h)
KER_SRC = $(wildcard src/kernel/*.c)
KER_OBJ = $(KER_SRC:src/kernel/%.c=build/kernel/%.o)

ASM_SRC = $(wildcard src/asm/*.S)
ASM_OBJ = $(ASM_SRC:src/asm/%.S=build/asm/%.o)

build/kernel/%.o: src/kernel/%.c $(KER_HEADERS)
	if [ ! -d build ]; then mkdir build; fi
	if [ ! -d build/kernel ]; then mkdir build/kernel; fi
	$(CROSS_PLATFORM)-gcc $(CFLAGS) -c $< -o $@

build/asm/%.o: src/asm/%.S
	if [ ! -d build ]; then mkdir build; fi
	if [ ! -d build/asm ]; then mkdir build/asm; fi
	$(CROSS_PLATFORM)-gcc $(CFLAGS) -c $< -o $@

all: build/$(KERNEL).elf

build/$(KERNEL).elf: $(KER_OBJ) $(ASM_OBJ)
	$(CROSS_PLATFORM)-ld -g -T $(KER_PATH)/linker.ld -o $@ $^

clean:
	rm -rf build

run:
	qemu-system-$(ARCH) -machine virt -nographic -bios none -kernel build/$(KERNEL).elf

debug:
	qemu-system-$(ARCH) -machine virt -nographic -bios none -kernel build/$(KERNEL).elf -S -s

dump:
	$(CROSS_PLATFORM)-objdump -d build/$(KERNEL).elf

dump_txt:
	$(CROSS_PLATFORM)-objdump -d build/$(KERNEL).elf > build/$(KERNEL)_dump.txt