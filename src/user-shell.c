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
// current directory text to show up in the cli
char current_dir[255] = "/";
// buffer to store user input
char buf[256];
// current directory length to keep track of the current directory length
uint8_t currentdirlen = 1;
// current working directory cluster number
uint32_t cwd_cluster_number = ROOT_CLUSTER_NUMBER;

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

void delete_syscall(struct FAT32DriverRequest request, int32_t *retcode)
{
    syscall(3, (uint32_t)&request, (uint32_t)retcode, 0);
}

void get_user_input(char *buf)
{
    syscall(4, (uint32_t)buf, 0, 0);
}

// void putchar(char *buf)
// {
//     syscall(5, (uint32_t)buf, 0, 0);
// }

void puts(char *str, uint32_t len, uint32_t color)
{
    syscall(6, (uint32_t)str, len, color);
}

void activate_keyboard(void)
{
    syscall(7, 0, 0, 0);
}

void move_child_dir(struct FAT32DriverRequest request, int32_t *retcode)
{
    syscall(8, (uint32_t)&request, (uint32_t)retcode, 0);
}

void move_parent_dir(struct FAT32DriverRequest request, int32_t *retcode)
{
    syscall(9, (uint32_t)&request, (uint32_t)retcode, 0);
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

void split_by_first(char *pstr, char by, char *result)
{
    int i = 0;
    while (pstr[i] != '\0' && pstr[i] != by)
    {
        result[i] = pstr[i];
        i++;
    }
    result[i] = '\0';

    if (pstr[i] == by)
    {
        i++;
        int j = 0;
        while (pstr[i] != '\0')
        {
            pstr[j] = pstr[i];
            i++;
            j++;
        }
        pstr[j] = '\0';
    }
    else
    {
        *pstr = '\0';
    }
}

int32_t is_include(char *str, char target)
{
    while (*str != '\0')
    {
        if (*str == target)
        {
            return 1;
        }
        str++;
    }
    return 0;
}

void cd(char *argument)
{
    if (memcmp("..", argument, 2) == 0 && strlen(argument) == 2)
    {
        request.parent_cluster_number = cwd_cluster_number;
        if (cwd_cluster_number != ROOT_CLUSTER_NUMBER)
        {
            move_parent_dir(request, &retcode);
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
        read_dir_syscall(request, &retcode);
        if (retcode == 0)
        {
            move_child_dir(request, &retcode);
            cwd_cluster_number = retcode;
            int32_t idx = 0;
            while (idx < strlen(request.name) && idx < 8)
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

void mkdir(char *argument)
{
    uint8_t name_len = strlen(argument);
    request.buffer_size = 0;
    request.buf = buf;
    while (name_len < 8)
    {
        argument[name_len] = '\0';
        name_len++;
    }
    memcpy(request.ext, "dir", 3);
    request.parent_cluster_number = cwd_cluster_number;
    memcpy(request.name, argument, name_len);
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

void rm(char *argument)
{
    uint8_t name_len = strlen(argument);
    request.buffer_size = 0;
    request.buf = buf;

    while (name_len < 8)
    {
        argument[name_len] = '\0';
        name_len++;
    }

    memcpy(request.ext, "dir", 3);
    request.parent_cluster_number = cwd_cluster_number;
    memcpy(request.name, argument, name_len);
    delete_syscall(request, &retcode);
    if (retcode == 0)
    {
        puts("Success: Folder '", 17, 0xF);
        puts(request.name, 8, 0xF);
        puts("' is deleted.\n", 15, 0xF);
    }
    else if (retcode == 1)
    {
        puts("Cannot remove: Folder'", 22, 0xF);
        puts(request.name, 8, 0xF);
        puts("' is not found.\n", 17, 0xF);
    }
    else if (retcode == 2)
    {
        puts("Cannot remove: Folder'", 22, 0xF);
        puts(request.name, 8, 0xF);
        puts("' is not empty.\n", 17, 0xF);
    }
    else if (retcode == -1)
    {
        puts("Unknown error.\n", 15, 0xF);
    }
}

void touch(char *argument)
{
    char filename[8];
    split_by_first(argument, '.', filename);

    request.buffer_size = CLUSTER_SIZE;
    request.buf = buf;
    if (strlen(filename) < 8)
    {
        filename[strlen(filename)] = '\0';
    }
    else
    {
        filename[8] = '\0';
    }
    if (strlen(argument) < 3)
    {
        argument[strlen(argument)] = '\0';
    }
    else
    {
        argument[3] = '\0';
    }

    memcpy(request.ext, argument, 3);
    request.parent_cluster_number = cwd_cluster_number;
    memcpy(request.name, filename, 8);

    read_syscall(request, &retcode);
    if (retcode == 0)
    {
        puts("File'", 7, 0xF);
        puts(request.name, 8, 0xF);
        puts(".", 1, 0xF);
        puts(request.ext, 3, 0xF);
        puts("' already exists.\n", 19, 0xF);
    }
    else if (retcode == 3)
    {
        memset(buf, 0, CLUSTER_SIZE);
        write_syscall(request, &retcode);
        if (retcode != 0)
        {
            puts("Unknown error.\n", 15, 0xF);
        }
        else
        {
            puts("File '", 8, 0xF);
            puts(request.name, 8, 0xF);
            puts(".", 1, 0xF);
            puts(request.ext, 3, 0xF);
            puts("' created.\n", 12, 0xF);
        }
    }
}

void remove_space(char *str)
{
    int32_t len = strlen(str);
    int32_t shift = 0;

    for (int32_t i = 0; i < len; i++)
    {
        if (str[i] == ' ')
        {
            shift++;
        }
        else
        {
            str[i - shift] = str[i];
        }
    }
    str[len - shift] = '\0';
}

void remove_petik(char *str)
{
    int32_t len = strlen(str);
    int32_t shift = 0;

    for (int32_t i = 0; i < len; i++)
    {
        if (str[i] == '"')
        {
            shift++;
        }
        else
        {
            str[i - shift] = str[i];
        }
    }
    str[len - shift] = '\0';
}

void echo(char *argument)
{
    remove_space(argument);
    remove_petik(argument);
    char text[strlen(argument)];
    split_by_first(argument, '>', text);

    request.buffer_size = CLUSTER_SIZE;
    text[strlen(text)] = '\0';

    request.parent_cluster_number = cwd_cluster_number;
    char name[8];
    split_by_first(argument, '.', name);

    if (strlen(argument) < 3)
    {
        argument[strlen(argument)] = '\0';
    }
    else
    {
        argument[8] = '\0';
    }
    if (strlen(name) < 3)
    {
        name[strlen(name)] = '\0';
    }
    else
    {
        name[8] = '\0';
    }

    request.buf = text;
    memcpy(request.ext, argument, 3);
    memcpy(request.name, name, 8);

    read_syscall(request, &retcode);
    if (retcode == 0)
    {
        delete_syscall(request, &retcode);
        write_syscall(request, &retcode);

        if (retcode != 0)
        {
            puts("Unknown error.\n", 15, 0xF);
        }
        else
        {
            puts("File '", 8, 0xF);
            puts(request.name, 8, 0xF);
            puts(".", 1, 0xF);
            puts(request.ext, 3, 0xF);
            puts("' updated.\n", 12, 0xF);
        }
    }
    else if (retcode == 1)
    {
        puts("Not a file.\n", 12, 0xF);
    }
    else if (retcode == 3)
    {
        puts("Not found.\n", 11, 0xF);
    }
    else
    {
        puts("Unknown error", 13, 0xF);
    }
}

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
            while (true)
            {
                if (is_include(argument, '/'))
                {
                    char res[strlen(argument)];
                    split_by_first(argument, '/', res);
                    cd(res);
                }
                else
                {
                    cd(argument);
                    break;
                }
            }
        }
        else if (!memcmp(buf, "mkdir", 5))
        {
            char *argument = buf + 6;
            remove_newline(argument);
            if (strlen(argument) > 0)
            {
                mkdir(argument);
            }
        }
        else if (!memcmp(buf, "rm", 2))
        {
            char *argument = buf + 3;
            remove_newline(argument);
            if (strlen(argument) > 0)
            {
                rm(argument);
            }
        }
        else if (!memcmp(buf, "touch", 5))
        {
            char *argument = buf + 6;
            remove_newline(argument);
            if (strlen(argument) > 0)
            {
                touch(argument);
            }
        }
        else if (!memcmp(buf, "echo", 4))
        {
            char *argument = buf + 5;
            remove_newline(argument);
            if (strlen(argument) > 0)
            {
                echo(argument);
            }
        }
        else if (!memcmp(buf, "ls", 2))
        {
            request.buf = buf;
            syscall(10, (uint32_t)request.buf, cwd_cluster_number, 0);
            if (request.buf == 0)
            {
                puts("Directory Empty\n", 16, 0x4);
            }
            else
            {
                puts(request.buf, strlen(request.buf), 0xF);
            }
        }

    } while (true);

    return 0;
}