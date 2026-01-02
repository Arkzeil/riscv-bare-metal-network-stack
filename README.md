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
  + 

## 