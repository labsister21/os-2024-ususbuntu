#include <stdint.h>
#include "header/filesystem/fat32.h"
#include "header/command.h"

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

void read_syscall(struct FAT32DriverRequest request, int32_t* retcode) {
    syscall(0, (uint32_t)&request, (uint32_t)retcode, 0);
}

void get_user_input(char* buf) {
    syscall(4, (uint32_t)buf, 0, 0);
}

void puts(char* str, uint32_t len, uint32_t color) {
    syscall(6, (uint32_t)str, len, color);
}

void activate_keyboard(void) {
    syscall(7, 0, 0, 0);
}

void command(char* buf, char* current_dir){
    puts("UsusBuntu@OS-IF2230:", 21, 0xA);
    puts(current_dir, 255, 0x9);
    puts("$ ", 3, 0xF);
    get_user_input(buf);
}


int main(void) {
    struct ClusterBuffer      cl[2] = { 0 };
    struct FAT32DriverRequest request = {
        .buf = &cl,
        .name = "shell",
        .ext = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size = CLUSTER_SIZE,
    };
    int32_t retcode;
    read_syscall(request, &retcode);
    if (retcode == 0)
        puts("owo\n", 4, 0xF);

    char current_dir[255] = "/";
    char buf[256];
    buf[255] = '\0';
    do {
        activate_keyboard();
        command(buf, current_dir);
        puts(buf, 256, 0xF);
    } while (true);

    return 0;
}
