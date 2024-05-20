#include "header/cpu/portio.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/fat32.h"
#include "header/driver/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/process/process.h"
#include "header/clock.h"

// I/O port wait, around 1-4 microsecond, for I/O synchronization purpose
void io_wait(void)
{
  out(0x80, 0);
}

// Send ACK to PIC - @param irq Interrupt request number destination, note: ACKED_IRQ = irq+PIC1_OFFSET
void pic_ack(uint8_t irq)
{
  if (irq >= 8)
    out(PIC2_COMMAND, PIC_ACK);
  out(PIC1_COMMAND, PIC_ACK);
}

// Shift PIC interrupt number to PIC1_OFFSET and PIC2_OFFSET (master and slave)
void pic_remap(void)
{
  // Starts the initialization sequence in cascade mode
  out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();
  out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();
  out(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
  io_wait();
  out(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset
  io_wait();
  out(PIC1_DATA, 0b0100); // ICW3: tell Master PIC, slave PIC at IRQ2 (0000 0100)
  io_wait();
  out(PIC2_DATA, 0b0010); // ICW3: tell Slave PIC its cascade identity (0000 0010)
  io_wait();

  out(PIC1_DATA, ICW4_8086);
  io_wait();
  out(PIC2_DATA, ICW4_8086);
  io_wait();

  // Disable all interrupts
  out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
  out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void syscall(struct InterruptFrame frame)
{
  switch (frame.cpu.general.eax)
  {
  case 0:
    *((int8_t*)frame.cpu.general.ecx) = read(
      *(struct FAT32DriverRequest*)frame.cpu.general.ebx);
    break;
  case 1:
    *((int8_t*)frame.cpu.general.ecx) = read_directory(
      *(struct FAT32DriverRequest*)frame.cpu.general.ebx);
    break;
  case 2:
    *((int8_t*)frame.cpu.general.ecx) = write(
      *(struct FAT32DriverRequest*)frame.cpu.general.ebx);
    break;
  case 3:
    *((int8_t*)frame.cpu.general.ecx) = delete (
      *(struct FAT32DriverRequest*)frame.cpu.general.ebx);
    break;
  case 4:
    get_keyboard_buffer((char*)frame.cpu.general.ebx, (int32_t*)frame.cpu.general.ecx);
    break;
  case 5:
    putchar((char)frame.cpu.general.ebx, frame.cpu.general.ecx);
    break;
  case 6:
    puts(
      (char*)frame.cpu.general.ebx,
      frame.cpu.general.ecx,
      frame.cpu.general.edx); // Assuming puts() exist in kernel
    break;
  case 7:
    keyboard_state_activate();
    break;
  case (8):
    *((uint32_t*)frame.cpu.general.ecx) = move_to_child_directory(*(struct FAT32DriverRequest*)frame.cpu.general.ebx);
    break;
  case (9):
    *((uint32_t*)frame.cpu.general.ecx) = move_to_parent_directory(*(struct FAT32DriverRequest*)frame.cpu.general.ebx);
    break;
  case (10):
    list_dir_content((char*)frame.cpu.general.ebx, frame.cpu.general.ecx);
    break;
  case (11):
    print((char*)frame.cpu.general.ebx, frame.cpu.general.ecx);
    break;
  case (12):
    print_path_to_dir((char*)frame.cpu.general.ebx, frame.cpu.general.ecx, (char*)frame.cpu.general.edx);
    break;
  case (13):
    framebuffer_clear();
    framebuffer_state.cur_col = 0;
    framebuffer_state.cur_row = 0;
    break;
  case (14):
    process_destroy(frame.cpu.general.ebx);
    break;
  case (15):
    process_create_user_process(*(struct FAT32DriverRequest*)frame.cpu.general.ebx);
    break;
  case (16):
    ps((char*)frame.cpu.general.ebx);
    break;
  case (17):
    read_rtc();
    *((uint32_t*)frame.cpu.general.ebx) = (uint32_t)hour;
    *((uint32_t*)frame.cpu.general.ecx) = (uint32_t)minute;
    *((uint32_t*)frame.cpu.general.edx) = (uint32_t)second;
    break;
  }
}

void main_interrupt_handler(struct InterruptFrame frame)
{
  switch (frame.int_number)
  {
  case PIC1_OFFSET + IRQ_KEYBOARD:
    keyboard_isr();
    break;
  case PIC1_OFFSET + IRQ_TIMER:
    pic_ack(0); // timer_isr();
    break;
  case 0x30:
    syscall(frame);
    break;
  }
};

void activate_keyboard_interrupt(void)
{
  out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

struct TSSEntry _interrupt_tss_entry = {
    .ss0 = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void set_tss_kernel_current_stack(void)
{
  uint32_t stack_ptr;
  // Reading base stack frame instead esp
  __asm__ volatile("mov %%ebp, %0" : "=r"(stack_ptr) : /* <Empty> */);
  // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
  _interrupt_tss_entry.esp0 = stack_ptr + 8;
}