#include "kernel/dtb.h"

extern void* _dtb_addr;

int dtb_callback(char *string_addr, unsigned int prop_len){
    if(string_comp_l(string_addr, "ns16550a", prop_len) == 0)
        return 1;
    else if(string_comp_l(string_addr, "virtio,mmio", prop_len) == 0)
        return 2;
    return 0;
}

uint64_t parse_dtb_unit_name(char *unit_name){
    char *p = unit_name;
    uint64_t addr = 0;
    int i = 0;
    // skip "@"
    while(*p != '@' && *p != '\0'){
        p++;
    }

    p++; // skip '@'

    for(; p[i] != '\0'; i++){
        addr = addr << 4;
        if(p[i] >= '0' && p[i] <= '9')
            addr += (p[i] - '0');
        else if(p[i] >= 'a' && p[i] <= 'f')
            addr += (p[i] - 'a' + 10);
        else if(p[i] >= 'A' && p[i] <= 'F')
            addr += (p[i] - 'A' + 10);
        else
            break;
    }

    return addr;
}

void fdt_traverse(int (*callback)(char *, unsigned int)){
    struct fdt_header* fdt = (struct fdt_header*)_dtb_addr;

    if(BE2LE(fdt->magic) != 0xd00dfeed){
        uart_puts("dtb magic value error\n");
        uart_b2x(BE2LE(fdt->magic));
        uart_putc('\n');
        return;
    }
    //unsigned int total_size  = fdt->totalsize;
    //unsigned int string_size = fdt->size_dt_strings;
    unsigned int struct_size = BE2LE(fdt->size_dt_struct);
    // for manipulation of pointer
    char* struct_ptr = (char*)fdt + BE2LE(fdt->off_dt_struct);
    char* string_ptr = (char*)fdt + BE2LE(fdt->off_dt_strings);
    //struct_ptr += align_mem_offset((void*)struct_ptr, 4);
    
    //callback(cpio_addr);
    uint64_t addr = 0;

    while(struct_ptr < ((char*)fdt + BE2LE(fdt->off_dt_struct) + struct_size)){
        uint32_t token = *(uint32_t*)struct_ptr;
        int ret;
        struct_ptr += 4;
        switch(BE2LE(token)){
            case FDT_NOP:
                //struct_ptr++;
                break;
            case FDT_BEGIN_NODE:
                //uart_puts("node begin------------\n");
                // uart_puts("unit name:");
                // uart_puts(struct_ptr);
                unsigned int print_len = string_len(struct_ptr);
                addr = parse_dtb_unit_name(struct_ptr);
                // uart_putc('\n');
                struct_ptr += print_len;
                // as there's a NULL, so add 1
                struct_ptr++;
                struct_ptr += align_mem_offset((void*)struct_ptr, 4);
                break;
            case FDT_PROP:
                //uart_puts("Property---------------\n");
                // property value length 0 just indicate the property itself is sufficient(meaning that property name still exist)

                // property length
                unsigned int prop_len = BE2LE(*(uint32_t*)(struct_ptr));
                // 32bits
                struct_ptr += 4;
                // property name offset (starting at string block)
                // uart_puts("property name:");
                // uart_puts(string_ptr + BE2LE(*(uint32_t*)(struct_ptr)));
                // uart_putc('\n');
                // 32bits
                struct_ptr += 4;
                // property value
                if(prop_len > 0){
                    ret = callback(struct_ptr, prop_len);
                    if(ret){
                        uart_puts("callback matched, property value:");
                        uart_puts_fixed(struct_ptr, prop_len);
                        uart_putc('\n');
                        uart_b2x(addr);
                        uart_putc('\n');
                    }
                    // uart_puts("property value:");
                    // uart_puts_fixed(struct_ptr, prop_len);
                    // uart_putc('\n');
                    struct_ptr += prop_len;
                }        

                struct_ptr += align_mem_offset((void*)struct_ptr, 4);
                break;
            case FDT_END_NODE:
                //uart_puts("node end--------------\n");
                break;
            case FDT_END:
                //uart_puts("node all end----------\n");
                break;
            default:
                //uart_b2x((unsigned int)*struct_ptr);
                //uart_putc('\n');
                return;
        }
    }
    //uart_b2x((unsigned int)*struct_ptr);
    //uart_putc('\n');
}