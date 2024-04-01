#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/cpu/portio.h"
#include "header/driver/framebuffer.h"
#include <stdbool.h>
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"

void kernel_setup(void)
{
    load_gdt(&_gdt_gdtr);

    // Interrupt
    pic_remap();
    initialize_idt();

    framebuffer_clear();
    framebuffer_write(3, 8, 'H', 0, 0xF);
    framebuffer_write(3, 9, 'a', 0, 0xF);
    framebuffer_write(3, 10, 'i', 0, 0xF);
    framebuffer_write(3, 11, '!', 0, 0xF);
    framebuffer_set_cursor(3, 10);

    __asm__("int $0x4");
    while (true)
        ;
}
