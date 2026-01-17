# riscv-bare-metal-network-stack

## Prerequisites
+ [QEMU](https://www.qemu.org/download/#linux)
+ [RISCV toolchain](https://github.com/riscv-software-src/opensbi)
    + `riscv64-unknown-elf-gcc`
    + `riscv64-unknown-elf-objcopy`
    + `riscv64-unknown-elf-objdump`
  
## Makefile
[ref](https://www.sifive.com/blog/all-aboard-part-1-compiler-args)
+ We choose `rv64gc` (RV64IMAFDC) as our machine architecture (march), check [wiki](https://en.wikipedia.org/wiki/RISC-V#Standard_extensions)
+ mabi: `lp64`
  + long and pointers are 64-bits long, while int is a 32-bit type. The other types remain the same as ilp32.
  + abi basiclly defines where which registers to use, how to pass function parameters (registers and stack) and other stuffs...
+ mcmodel: 
+ `virt` machine option specified in qemu
  + virt machine uses the 16550A standard
    + UART base address is: 0x10000000
+ `-netdev user,id=net0,hostfwd=udp::5555-:5555 -device virtio-net-device,netdev=net0`
  + The Backend (`-netdev`) 
    + This defines how packets leave QEMU.
    + `user`: Built-in stack. No root/sudo required. Great for simple TCP/UDP.
      + `tap`: Connects to a virtual bridge on your host. Higher performance, but requires host configuration.
    + `hostfwd=[proto]:[hostaddr]:hostport-[guestaddr]:guestport`
      + proto: tcp or udp (defaults to tcp if omitted).
      + hostaddr: The IP on your host to bind to (leave empty to bind to all interfaces).
      + hostport: The port number on your host machine.
      + guestaddr: The IP address of your kernel (defaults to 10.0.2.15 in the standard SLIRP config).
        + If not using DHCP, you must manually assign this IP to your VirtIO interface.
      + guestport: The port your kernel is listening on.
  + The Frontend (`-device`)
    + This defines the virtual "silicon" your kernel interacts with
      + On the RISC-V `virt` machine, `virtio-net-device` creates a VirtIO-MMIO device.
      + The `netdev=net0` flag acts as the "cable" connecting this hardware to your backend.

## Linker Script
+ `PHDRS` defines section permission
+ `NOLOAD` tells the linker:
  + Reserve address space in RAM
  + Do not put anything in the ELF file
  + Do not generate a PT_LOAD file range
  + For sections or spaces having no initial contents
    + .bss
    + heap
    + stack

## File layout
```
.
├── include
│   └── kernel
│       ├── dtb.h
│       ├── uart.h
│       └── utils.h
├── Makefile
├── README.md
└── src
    ├── asm
    │   └── Start.S
    ├── driver
    └── kernel
        ├── dtb.c
        ├── linker.ld
        ├── main.c
        ├── uart.c
        └── utils.c
```