#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/cpu/portio.h"
#include "header/driver/framebuffer.h"
#include "header/driver/keyboard.h"
#include <stdbool.h>
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"
#include "header/driver/disk.h"
#include "header/filesystem/fat32.h"

void kernel_setup(void) {
  load_gdt(&_gdt_gdtr);
  pic_remap();
  activate_keyboard_interrupt();
  initialize_idt();
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);
  initialize_filesystem_fat32();

  while (true);
}

