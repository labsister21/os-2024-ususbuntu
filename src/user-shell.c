#include <stdint.h>
#include "header/filesystem/fat32.h"
#include "header/command.h"
#include "header/stdlib/string.h"

struct ClusterBuffer cl[2] = { 0 };
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
char buf[2000];

char temp_buf[2000];

char cur_char;

bool is_entered = false;
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

void read_syscall(struct FAT32DriverRequest request, int32_t* retcode)
{
  syscall(0, (uint32_t)&request, (uint32_t)retcode, 0);
}

void read_dir_syscall(struct FAT32DriverRequest request, int32_t* retcode)
{
  syscall(1, (uint32_t)&request, (uint32_t)retcode, 0);
}

void write_syscall(struct FAT32DriverRequest request, int32_t* retcode)
{
  syscall(2, (uint32_t)&request, (uint32_t)retcode, 0);
}

void delete_syscall(struct FAT32DriverRequest request, int32_t* retcode)
{
  syscall(3, (uint32_t)&request, (uint32_t)retcode, 0);
}

// void get_user_input(char* buf)
// {
//   syscall(4, (uint32_t)buf, 0, 0);
// }

void get_user_input(char* buf, int32_t* retcode)
{
  syscall(4, (uint32_t)buf, (uint32_t)retcode, 0);
}

void putchar(char buf, uint32_t color)
{
  syscall(5, (uint32_t)buf, color, 0);
}

void puts(char* str, uint32_t len, uint32_t color)
{
  syscall(6, (uint32_t)str, len, color);
}

void activate_keyboard(void)
{
  syscall(7, 0, 0, 0);
}

void move_child_dir(struct FAT32DriverRequest request, int32_t* retcode)
{
  syscall(8, (uint32_t)&request, (uint32_t)retcode, 0);
}

void move_parent_dir(struct FAT32DriverRequest request, int32_t* retcode)
{
  syscall(9, (uint32_t)&request, (uint32_t)retcode, 0);
}

void command(char* current_dir)
{
  puts("UsusBuntu@OS-IF2230:", 21, 0xA);
  puts(current_dir, 255, 0x9);
  puts("$ ", 3, 0xF);
  // get_user_input(buf);
}

void remove_newline(char* str)
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

void split_by_first(char* pstr, char by, char* result)
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

int32_t is_include(char* str, char target)
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

void cd(char* argument)
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

void cp(char* argument)
{
  // Initiate source file
  char source[12];
  char source_name[8];
  char source_ext[3];

  // getting the source file name and extension
  split_by_first(argument, ' ', source);
  split_by_first(source, '.', source_name);

  memcpy(source_ext, source, strlen(source));

  // getting the destination path
  char dest[strlen(argument)];
  memcpy(dest, argument, strlen(argument));

  // random bullshit go
  request.buffer_size = CLUSTER_SIZE;
  request.buf = buf;

  // Ensure the argument is null-terminated and padded to 8 characters
  uint8_t name_len = strlen(source_name);
  while (name_len < 8)
  {
    source_name[name_len] = '\0';
    name_len++;
  }
  // Ensure the argument is null-terminated and padded to 3 characters
  uint8_t ext_len = strlen(source_ext);
  while (ext_len < 3)
  {
    source_ext[ext_len] = '\0';
    ext_len++;
  }

  // create request to read source file
  memcpy(request.ext, source_ext, 3);
  request.parent_cluster_number = cwd_cluster_number;
  memcpy(request.name, source_name, 8);

  // read source file
  read_syscall(request, &retcode);

  // copy the source content
  char source_content[strlen(request.buf)];
  memcpy(source_content, request.buf, strlen(request.buf));

  // Source file found, check destination, arguments holds the path
  if (retcode == 0)
  {
    if (is_include(dest, '.'))
    {
      // puts("Destination is a file\n", 23, 0x4);
      // Initiate target file
      char target[12];
      char target_name[8];
      char target_ext[3];

      split_by_first(dest, '.', target_name);
      char target_name_len = strlen(target_name);

      memcpy(target_ext, dest, strlen(dest));
      char target_ext_len = strlen(target_ext);

      while (target_name_len < 8)
      {
        target_name[target_name_len] = '\0';
        target_name_len++;
      }

      while (target_ext_len < 3)
      {
        target_ext[target_ext_len] = '\0';
        target_ext_len++;
      }

      // paste the source file
      memcpy(request.buf, source_content, strlen(source_content));
      memcpy(request.ext, target_ext, 3);
      memcpy(request.name, target_name, 8);
      request.parent_cluster_number = cwd_cluster_number;
      write_syscall(request, &retcode);
    }
    else
    {
      // puts("Destination is a directory\n", 28, 0x4);
      // saving current directory state
      uint32_t cd_count = 0;
      while (true)
      {
        cd_count++;
        if (is_include(dest, '/'))
        {
          char res[strlen(dest)];
          split_by_first(dest, '/', res);
          // Debug
          puts("res: ", 5, 0xF);
          puts(res, strlen(res), 0xF);
          puts("\n", 1, 0xF);

          cd(res);
        }
        else
        {
          cd(dest);
          break;
        }
      }

      // implement copying file
      memcpy(request.buf, source_content, strlen(source_content));
      memcpy(request.ext, source_ext, 3);
      memcpy(request.name, source_name, 8);
      request.parent_cluster_number = cwd_cluster_number;
      write_syscall(request, &retcode);

      // puts("Copying file\n", 14, 0xF);
      // puts("\n", 1, 0xF);

      for (uint32_t i = 0; i < cd_count; i++)
      {
        cd("..");
      }
    }
    // else
    // {
    //   puts("Invalid path\n", 15, 0x4);
    // }
  }
  else if (retcode == 3)
  {
    puts("Source file not found.\n", 24, 0x4);
  }
  else
  {
    puts("Unknown error.\n", 16, 0x4);
  }
}

void mkdir(char* argument)
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

void cat(char* argument)
{
  char filename[strlen(argument)];
  split_by_first(argument, '.', filename);

  uint8_t name_len = strlen(filename);
  request.buffer_size = CLUSTER_SIZE;
  request.buf = buf;
  while (name_len < 8)
  {
    filename[name_len] = '\0';
    name_len++;
  }
  memcpy(request.name, filename, name_len);
  memcpy(request.ext, argument, strlen(argument));
  request.parent_cluster_number = cwd_cluster_number;
  read_syscall(request, &retcode);
  if (retcode == 0)
  {
    puts(request.buf, strlen(request.buf), 0xF);
    puts("\n", 1, 0xF);
  }
  else if (retcode == 1)
  {
    puts("Not a file\n", 11, 0x4);
  }
  else if (retcode == 2)
  {
    puts("Not enough buffer\n", 18, 0x4);
  }
  else if (retcode == -1)
  {
    puts("Unknown error -1\n", 17, 0x4);
  }
  else
  {
    puts("Unknown error\n", 14, 0x4);
  }
}

void rm(char* argument)
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
    puts("Cannot remove: Folder '", 22, 0xF);
    puts(request.name, 8, 0xF);
    puts("' is not found.\n", 17, 0xF);
  }
  else if (retcode == 2)
  {
    puts("Cannot remove: Folder '", 22, 0xF);
    puts(request.name, 8, 0xF);
    puts("' is not empty.\n", 17, 0xF);
  }
  else if (retcode == -1)
  {
    puts("Unknown error.\n", 15, 0xF);
  }
}

void find(char* argument)
{
  uint8_t name_len = strlen(argument);
  request.buffer_size = CLUSTER_SIZE;
  request.buf = buf;

  // Ensure the argument is null-terminated and padded to 8 characters
  while (name_len < 8)
  {
    argument[name_len] = '\0';
    name_len++;
  }

  // Set the name and extension for the directory
  memcpy(request.name, argument, name_len);
  memcpy(request.ext, "dir", 3);
  request.parent_cluster_number = cwd_cluster_number;

  // Read the directory
  char directories[255];
  directories[0] = '\0';
  syscall(12, (uint32_t)directories, cwd_cluster_number, (uint32_t)argument);

  if (directories[0] == '\0')
  {
    puts("Directory Empty\n", 16, 0x4);
  }
  else
  {
    puts(directories, strlen(directories), 0xF);
  }

  // if (retcode == 0)
  // {
  //   // Directory found, iterate over its contents
  //   int32_t idx = 0;
  //   char path[255];
  //   memcpy(path, "shell\\", 6);
  //   int pathlen = 6;

  //   while (idx < strlen(request.name) && idx < 8)
  //   {
  //     path[pathlen] = request.name[idx];
  //     pathlen++;
  //     idx++;
  //   }
  //   path[pathlen] = '\\';
  //   pathlen++;

  //   // Print the directory path
  //   puts(path, pathlen, 0xF);
  //   puts("\n", 1, 0xF);
  // }
  // else if (retcode == 2)
  // {
  //   puts("Directory not found.\n", 21, 0xF);
  // }
  // else if (retcode == 1)
  // {
  //   puts("File process TBA!\n", 18, 0xF);
  //   // Not a directory, possibly a file
  // }
}

// argument semua teks setelah cp
// void cp(char *argument)
// {
//   char *source;
//   split(argument, ' ', source); // source berisi path file yang akan dicopy, argument berisi sisanya

//   char *source_name;
//   split_by_first(source, '.', source_name); // source_name berisi nama file yang akan dicopy, source berisi eksensinya
//   uint8_t source_name_len = strlen(source_name);

//   while (source_name_len < 8)
//   {
//     source_name[source_name_len] = '\0';
//     source_name_len++;
//   }

//   memcpy(request.name, source_name, source_name_len);
//   memcpy(request.ext, source, strlen(source));
//   request.parent_cluster_number = cwd_cluster_number;

//   read_syscall(request, &retcode);

//   if (retcode == 0)
//   {
//     puts("Source file found\n", 10, 0xF);
//     char *content;
//     memcpy(content, request.buf, strlen(request.buf)); // mengcopy isi dari source fi;e

//     if (!is_include(argument, '/')) // kondisi mencopy di cwd
//     {
//       char *target_name;
//       split(argument, '.', target_name); // target_name berisi nama file yang akan dicopy, argument berisi eksensinya
//       uint8_t target_name_len = strlen(target_name);
//       while (target_name_len < 8)
//       {
//         target_name[target_name_len] = '\0';
//         target_name_len++;
//       }

//       memcpy(request.name, target_name, target_name_len);
//       memcpy(request.ext, argument, strlen(argument));

//       read_syscall(request, &retcode);

//       if (retcode == 0)
//       {
//         puts("Deleting Existing file\n", 25, 0xF);
//         delete_syscall(request, &retcode);
//       }

//       memcpy(request.buf, content, strlen(content));
//       request.buffer_size = strlen(content);

//       write_syscall(request, &retcode);

//       if (retcode == 0)
//       {
//         puts("File copied successfully\n", 24, 0xF);
//       }
//       else
//       {
//         puts("Failed to copy file\n", 20, 0xF);
//       }
//     }
//     else // kondisi mencopy ke path tertentu
//     {
//       puts("Copying to path\n", 16, 0xF); // TBA
//     }
//   }
//   else
//   {
//     puts("Source file not found\n", 14, 0xF);
//   }
// }

// void mv(char *argument)
// {
//   char *temp;
//   memcpy(temp, argument, strlen(argument));
//   cp(temp);
//   rm(temp);
// }

void touch(char* argument)
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

void remove_space(char* str)
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

void remove_petik(char* str)
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

void echo(char* argument)
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

void clear()
{
  syscall(13, 0, 0, 0);
}

void clear_buf() {
  for (int i = 0; i < 256; i++)
  {
    buf[i] = '\0';
  }
}

void clear_temp_buffer() {
  for (int i = 0; i < 256; i++)
  {
    temp_buf[i] = '\0';
  }
}

void int_to_str(int num, char* str) {
  int i = 0;
  int is_negative = 0;

  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }

  if (num < 0) {
    is_negative = 1;
    num = -num;
  }

  while (num != 0) {
    int rem = num % 10;
    str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    num = num / 10;
  }

  if (is_negative)
    str[i++] = '-';

  str[i] = '\0';

  int start = 0;
  int end = i - 1;
  while (start < end) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
    start++;
    end--;
  }
}

void str_to_int(char* str, int* num) {
  int result = 0;
  int i = 0;
  int is_negative = 0;

  if (str[0] == '-') {
    is_negative = 1;
    i++;
  }

  while (str[i] != '\0') {
    result = result * 10 + str[i] - '0';
    i++;
  }

  if (is_negative)
    result = -result;

  *num = result;
}

void print_kaguya() {
  cat("kaguya.txt");
  puts("\n", 1, 0xF);
  puts("Welcome to UsusBuntu OS\n", 25, 0xF);
  puts("Type 'help' to see the list of available commands\n", 51, 0xF);
}

void exec(char* argument) {
  // Exec command to create new process
  char filename[8];
  split_by_first(argument, '.', filename);

  uint8_t name_len = strlen(filename);
  request.buffer_size = CLUSTER_SIZE;
  request.buf = buf;
  while (name_len < 8)
  {
    filename[name_len] = '\0';
    name_len++;
  }

  uint8_t ext_len = strlen(argument);
  while (ext_len < 3)
  {
    argument[ext_len] = '\0';
    ext_len++;
  }

  memcpy(request.ext, argument, 3);
  request.parent_cluster_number = cwd_cluster_number;
  memcpy(request.name, filename, 8);

  read_syscall(request, &retcode);
  if (retcode == 0)
  {
    puts("Executing '", 11, 0xF);
    puts(request.name, 8, 0xF);
    puts(".", 1, 0xF);
    puts(request.ext, 3, 0xF);
    puts("'\n", 2, 0xF);
    syscall(15, (uint32_t)&request, 0, 0);
  }
  else if (retcode == 3)
  {
    puts("File not found\n", 15, 0xF);
  }
  else
  {
    puts("Unknown error\n", 14, 0xF);
  }
}

void ps_syscall() {
  puts("Process list:\n", 14, 0xF);
  syscall(16, (uint32_t)buf, 0, 0);
}

void kill(char* argument) {
  int pid;
  str_to_int(argument, &pid);
  syscall(14, (uint32_t)pid, 0, 0);
}

int main(void)
{
  buf[2000] = '\0';
  read_syscall(request, &retcode);
  print_kaguya();
  clear_buf();
  clear_temp_buffer();
  command(current_dir);
  activate_keyboard();
  do
  {
    get_user_input(&cur_char, &retcode);

    if (retcode == -1) continue;

    if (!memcmp(&cur_char, "\n", 1))
    {
      memcpy(buf, temp_buf, strlen(temp_buf));
      memcpy(&cur_char, "", 1);
      clear_temp_buffer();
      is_entered = true;
    }
    else if (!memcmp(&cur_char, "\b", 1))
    {
      if (strlen(temp_buf) > 0)
      {
        temp_buf[strlen(temp_buf) - 1] = '\0';
      }
    }
    else
    {
      temp_buf[strlen(temp_buf)] = cur_char;
    }

    if (!is_entered) continue;


    if (!memcmp(buf, "cd", 2))
    {
      char* argument = buf + 3;
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

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "ls", 2))
    {
      buf[0] = '\0';
      syscall(10, (uint32_t)buf, cwd_cluster_number, 0);
      if (buf[0] == '\0')
      {
        puts("Directory Empty\n", 16, 0x4);
      }
      else
      {
        puts(buf, strlen(buf), 0xF);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "print", 5))
    {
      char directories[255];
      directories[0] = '\0';
      syscall(11, (uint32_t)directories, cwd_cluster_number, 0);
      if (directories[0] == '\0')
      {
        puts("Directory Empty\n", 16, 0x4);
      }
      else
      {
        puts(directories, strlen(directories), 0xF);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "mkdir", 5))
    {
      char* argument = buf + 6;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        mkdir(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "touch", 5))
    {
      char* argument = buf + 6;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        touch(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "echo", 4))
    {
      char* argument = buf + 5;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        echo(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "cat", 3))
    {
      char* argument = buf + 4;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        cat(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "rm", 2))
    {
      char* argument = buf + 3;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        rm(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "find", 4))
    {
      char directories[255];
      memset(directories, '\0', sizeof(directories));

      char* argument = buf + 5;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        find(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "cp", 2))
    {
      char* argument = buf + 3;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        cp(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    // else if (!memcmp(buf, "mv", 2))
    // {
    //   char *argument = buf + 3;
    //   remove_newline(argument);
    //   if (strlen(argument) > 0)
    //   {
    //     mv(argument);
    //   }  
    // }
    else if (!memcmp(buf, "ps", 2)) {
      ps_syscall();
      puts(buf, strlen(buf), 0xF);
      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "exec", 4))
    {
      char* argument = buf + 5;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        exec(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "kill", 4))
    {
      char* argument = buf + 5;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        kill(argument);
      }

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "clear", 5)) {
      clear();
      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else if (!memcmp(buf, "help", 4)) {
      puts("List of available commands:\n", 30, 0xF);
      puts("1.  cd [directory]\n", 20, 0xF);
      puts("2.  ls\n", 8, 0xF);
      puts("3.  print\n", 11, 0xF);
      puts("4.  mkdir [directory]\n", 23, 0xF);
      puts("5.  touch [file]\n", 18, 0xF);
      puts("6.  echo [text] > [file]\n", 26, 0xF);
      puts("7.  cat [file]\n", 16, 0xF);
      puts("8.  rm [file]\n", 15, 0xF);
      puts("9.  find [file]\n", 17, 0xF);
      puts("10. cp [source] [destination]\n", 31, 0xF);
      puts("11. clear\n", 11, 0xF);
      puts("12. help\n", 10, 0xF);

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }
    else
    {
      puts("Command not found.\n", 20, 0x4);

      clear_buf();
      command(current_dir);
      activate_keyboard();
    }

    retcode = -1;
    is_entered = false;
  } while (true);

  return 0;
}