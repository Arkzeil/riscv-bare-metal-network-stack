OUTPUT_ARCH("riscv")

ENTRY(_start)

BASE_ADDR = 0x80000000;
VIRT_OFFSET = 0xFFFFFFC000000000;

PHDRS {
    text PT_LOAD FLAGS(5);   /* R + X = 4 + 1 */
    data PT_LOAD FLAGS(6);   /* R + W = 4 + 2 */
}

MEMORY
{
    RAM (rwx) : ORIGIN = BASE_ADDR + VIRT_OFFSET, LENGTH = 128M
}

SECTIONS{
    /* QEMU virt machine typically starts here for bare-metal kernels */
    
    /* set current location counter ('.' is location counter),current address is automatically incremented when the linker adds data. */
    /* C stack should start at address 0x80000000 and grow downwards, since hardware loads our kernel to address 0x80000000 and up, stack can safely run from 0x80000000 and downwards*/
    /* 0x80000 is our where our .text start*/

    . = BASE_ADDR + VIRT_OFFSET;

    /* VMA: BASE_ADDR + VIRT_OFFSET LMA: BASE_ADDR*/
    .text : AT(BASE_ADDR) {
        KEEP(*(.text.entry))      /* put _start here */
        *(.text*)           /* rest of the codes */
    } > RAM :text

    /* Only relocate .text for simpilcity */
    .rodata : {
        *(.rodata*)
    } > RAM :text

    .data : {
        *(.data*)
    } > RAM :data

    /* uninitialized global variable. (NOLOAD):link the program properly but must not be loaded to memory, since bss are all 0*/
    /* but in ELF, those initialized to 0 will be in .bss, uninitialized variables will be in COMMON section*/
    /* We put COMMON in .bss, so COMMON will be clear to 0 when program starts*/

    .bss : {
        /* align the section so that it starts at an address that is a multiple of 8. */
        /* If the section is not aligned, it would be more difficult to use the store instruction to store 0*/
        . = ALIGN(16);
        _sbss = .;          /* bss start */
        *(.bss*)
        *(COMMON)
        _ebss = .;          /* bss end */
        _end = .;
    } > RAM :data

    /* Heap and Stack are not sections and are not loaded */
    
    /* heap starts from the end of bss */
    PROVIDE(__heap_start = _end);
    /* stack starts from the very top of memory */
    PROVIDE(__stack_limit = ORIGIN(RAM) + LENGTH(RAM) - 0x4000); /* Bottom of 16KB stack */
    PROVIDE(__stack_top = ORIGIN(RAM) + LENGTH(RAM));
}