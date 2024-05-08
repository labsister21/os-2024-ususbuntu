#include <stdint.h>
#include "header/filesystem/fat32.h"
#include "header/command.h"
#include "header/stdlib/string.h"

struct ClusterBuffer cl[2] = {0};
struct FAT32DriverRequest request = {
    .buf = &cl,
    .name = "shell",
    .ext = "\0\0\0",
    .parent_cluster_number = ROOT_CLUSTER_NUMBER,
    .buffer_size = CLUSTER_SIZE,
};
int32_t retcode;

char current_dir[255] = "/";
char buf[256];
uint32_t cur_depth = 1;

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

void read_syscall(struct FAT32DriverRequest request, int32_t *retcode)
{
    syscall(0, (uint32_t)&request, (uint32_t)retcode, 0);
}

void read_dir_syscall(struct FAT32DriverRequest request, int32_t *retcode)
{
    syscall(1, (uint32_t)&request, (uint32_t)retcode, 0);
}

void write_syscall(struct FAT32DriverRequest request, int32_t *retcode)
{
    syscall(2, (uint32_t)&request, (uint32_t)retcode, 0);
}

void get_user_input(char *buf)
{
    syscall(4, (uint32_t)buf, 0, 0);
}

void puts(char *str, uint32_t len, uint32_t color)
{
    syscall(6, (uint32_t)str, len, color);
}

void activate_keyboard(void)
{
    syscall(7, 0, 0, 0);
}



void command(char *buf, char *current_dir)
{
    puts("UsusBuntu@OS-IF2230:", 21, 0xA);
    puts(current_dir, 255, 0x9);
    puts("$ ", 3, 0xF);
    get_user_input(buf);
}

void remove_newline(char *str)
{
    int len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        if (str[i] == '\n')
        {
            str[i] = '\0';
            break;
        }
    }
}
uint8_t currentdirlen = 1;

uint32_t cwd_cluster_number = ROOT_CLUSTER_NUMBER;

int main(void)
{
    buf[255] = '\0';
    read_syscall(request, &retcode);
    do
    {
        activate_keyboard();
        command(buf, current_dir);
        if (!memcmp(buf, "cd", 2))
        {
            char *argument = buf + 3;
            remove_newline(argument);
            if (memcmp("..", argument, 2) == 0 && strlen(argument) == 2)
            {
                request.parent_cluster_number = cwd_cluster_number;
                if (cwd_cluster_number != ROOT_CLUSTER_NUMBER)
                {
                    syscall(9, (uint32_t)&request, (uint32_t)&retcode, 0);
                    cwd_cluster_number = retcode;
                    currentdirlen -= 2;
                    while (current_dir[currentdirlen] != '/')
                    {
                        current_dir[currentdirlen] = '\0';
                        currentdirlen--;
                    }
                    current_dir[currentdirlen] = '/';
                    currentdirlen++;
                }
            }
            else
            {
                request.buffer_size = CLUSTER_SIZE;
                request.buf = buf;
                memcpy(request.name, argument, strlen(argument));
                int idx = strlen(argument);
                while (idx <= 7)
                {
                    request.name[idx] = '\0';
                    idx++;
                }
                memcpy(request.ext, "dir", 3);
                request.parent_cluster_number = cwd_cluster_number;
                syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);
                if (retcode == 0)
                {
                    syscall(8, (uint32_t)&request, (uint32_t)&retcode, 0);
                    cwd_cluster_number = retcode;
                    int32_t idx = 0;
                    while (idx < strlen(request.name))
                    {
                        current_dir[currentdirlen] = request.name[idx];
                        currentdirlen++;
                        idx++;
                    }
                    current_dir[currentdirlen] = '/';
                    currentdirlen++;
                }
                else if (retcode == 2)
                {
                    puts("Folder not found.\n", 18, 0xF);
                }
            }
        }
        else if (!memcmp(buf, "mkdir", 5))
        {
            char *dirname = buf + 6;
            remove_newline(dirname);
            uint8_t name_len = strlen(dirname);
            request.buffer_size = 0;
            request.buf = buf;
            while (name_len < 8)
            {
                dirname[name_len] = '\0';
                name_len++;
            }
            memcpy(request.ext, "dir", 3);
            request.parent_cluster_number = cwd_cluster_number;
            memcpy(request.name, dirname, name_len);
            read_dir_syscall(request, &retcode);
            if (retcode == 0)
            {
                puts("Folder'", 7, 0xF);
                puts(request.name, 8, 0xF);
                puts("' already exists.\n", 19, 0xF);
            }
            else if (retcode == 2)
            {
                memset(buf, 0, CLUSTER_SIZE);
                write_syscall(request, &retcode);
                if (retcode != 0)
                {
                    puts("Unknown error.\n", 15, 0xF);
                }
                else
                {
                    puts("Folder '", 8, 0xF);
                    puts(request.name, 8, 0xF);
                    puts("' created.\n", 12, 0xF);
                }
            }
        }
    } while (true);

    return 0;
}