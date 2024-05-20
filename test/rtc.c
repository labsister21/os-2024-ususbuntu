#include <stdint.h>
#include "header/filesystem/fat32.h"
#include "header/stdlib/string.h"

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
  __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
  __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
  __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
  __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
  // Note : gcc usually use %eax as intermediate register,
  //        so it need to be the last one to mov
  __asm__ volatile("int $0x30");
}

void clock(){
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  syscall(17, (uint32_t)&hour, (uint32_t)&minute, (uint32_t)&second);
}

int main(){
    do{
        clock();
    }while (true);
}