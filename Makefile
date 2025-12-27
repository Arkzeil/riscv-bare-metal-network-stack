CROSS_PLATFORM = riscv64-unknown-elf
CC = gcc
KERNEL = kernel

CFLAGS =  -Wall -nostdlib -nostartfiles -ffreestanding -nostdinc -Iinclude -mgeneral-regs-only -g


KER_SRC = $(wildcard src/kernel/*.c)
KER_HEADERS = $(wildcard include/kernel/*.h)
KER_OBJ = $(KER_SRC:src/kernel/%.c=build/kernel/%.o)

ASM_SRC = $(wildcard src/asm/*.S)
ASM_OBJ = $(ASM_SRC:src/asm/%.S=build/asm/%.o)

all: build/$(KERNEL).elf

build/$(KERNEL).elf: $(KER_OBJ) $(ASM_OBJ)
	$(CROSS_PLATFORM)-ld -g -T linker.ld -o $@ $^

clean:
	rm -rf build/*

run:
	qemu-system-riscv64 -machine virt -nographic -bios none -kernel build/$(KERNEL).elf
