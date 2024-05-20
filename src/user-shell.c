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

uint32_t sys_move(struct FAT32DriverRequest *request, struct FAT32DriverRequest *dst_request)
{
  uint32_t ret;
  syscall(13, (uint32_t)request, (uint32_t)&ret, (uint32_t)dst_request);
  return ret;
}

void get_user_input(char *buf)
{
  syscall(4, (uint32_t)buf, 0, 0);
}

void putchar(char buf, uint32_t color)
{
  syscall(5, (uint32_t)buf, color, 0);
}

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

void cp(char *argument)
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

void cat(char *argument)
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

void rm(char *argument)
{
  char filename[8];
  split_by_first(argument, '.', filename);
  request.buffer_size = 0;
  request.buf = buf;
  request.parent_cluster_number = cwd_cluster_number;
  if (strlen(filename) <= 8)
  {
    filename[strlen(filename)] = '\0';
  }
  filename[8] = '\0';

  memcpy(request.name, filename, strlen(filename));

  if (strlen(argument) <= 3)
  {
    argument[strlen(argument)] = '\0';
  }
  argument[3] = '\0';

  if (memcmp(argument, "", 0) == 0)
  {
    read_dir_syscall(request, &retcode);
    if (retcode == 0)
    {
      memcpy(request.ext, "dir", 3);
    }
    else if (retcode == 1)
    {
      memcpy(request.ext, "", 3);
    }
  }
  puts(request.ext, 3, 0xF);
  puts("\n", 1, 0xF);

  delete_syscall(request, &retcode);
  if (retcode == 0)
  {
    puts("Success ", 8, 0xF);
    if (memcmp(request.ext, "dir", 3) == 0)
    {
      puts("Folder '", 8, 0xF);
    }
    else
    {
      puts("File '", 6, 0xF);
    }
    puts(request.name, 8, 0xF);
    puts("' is deleted.\n", 15, 0xF);
  }
  else if (retcode == 1)
  {
    puts("Cannot remove: File '", 22, 0xF);
    if (memcmp(request.ext, "dir", 3))
    {
      puts("Folder '", 8, 0xF);
    }
    else
    {
      puts("File '", 6, 0xF);
    }
    puts("Not found", 9, 0xF);
  }
  else if (retcode == 2)
  {
    puts("Cannot remove: File '", 22, 0xF);
    puts(request.name, 8, 0xF);
    puts("' is not empty.\n", 17, 0xF);
  }
  else if (retcode == -1)
  {
    puts("Unknown error.\n", 15, 0xF);
  }
}

void find(char *argument)
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

void cast_int_char(uint32_t num, char *res)
{
  int idx = 0;
  while (num > 0)
  {
    res[idx] = num % 10 + '0';
    num /= 10;
    idx++;
  }
  res[idx] = '\0';
  int len = strlen(res);
  for (int i = 0; i < len / 2; i++)
  {
    char temp = res[i];
    res[i] = res[len - i - 1];
    res[len - i - 1] = temp;
  }
}

uint32_t search_cluster_resolve_path(uint32_t cluster_number, char *path)
{
  uint32_t initial_cluster = cluster_number;
  char initial_dir[255];
  memcpy(initial_dir, current_dir, strlen(current_dir));
  initial_dir[strlen(current_dir)] = '\0';
  uint8_t initial_curdirlen = currentdirlen;
  char initial_path[255];
  memcpy(initial_path, path, strlen(path));
  initial_path[strlen(path)] = '\0';
  while (true)
  {
    if (is_include(path, '/'))
    {
      char res[strlen(path)];
      split_by_first(path, '/', res);
      cd(res);
    }
    else
    {
      cluster_number = cwd_cluster_number;
      break;
    }
  }
  memcpy(current_dir, initial_dir, strlen(initial_dir));
  current_dir[strlen(initial_dir)] = '\0';
  currentdirlen = initial_curdirlen;
  memcpy(path, initial_path, strlen(initial_path));
  path[strlen(initial_path)] = '\0';
  cwd_cluster_number = initial_cluster;
  return cluster_number;
}

void get_last_name(char *path, char *name)
{
  int idx = 0;
  while (path[idx] != '\0')
  {
    idx++;
  }
  idx--;
  while (path[idx] != '/' && idx >= 0)
  {
    idx--;
  }
  idx++;
  int j = 0;
  while (path[idx] != '\0')
  {
    name[j] = path[idx];
    j++;
    idx++;
  }
  name[j] = '\0';
}

void mv(char *argument)
{
  char source[12];
  char last_name[12];
  char dest_last_name[12];
  uint32_t source_cluster_number;
  uint32_t dest_cluster_number;
  char source_buffer[256];
  char dest_buffer[256];

  struct FAT32DriverRequest src_req;

  struct FAT32DriverRequest dst_req;

  split_by_first(argument, ' ', source);
  if (strlen(source) < 256)
  {
    source[strlen(source)] = '\0';
  }
  else
  {
    source[255] = '\0';
  }

  if (strlen(argument) < 256)
  {
    argument[strlen(argument)] = '\0';
  }
  else
  {
    argument[255] = '\0';
  }

  memcpy(source_buffer, source, strlen(source));
  memcpy(dest_buffer, argument, strlen(argument));
  source_cluster_number = search_cluster_resolve_path(cwd_cluster_number, source);
  dest_cluster_number = search_cluster_resolve_path(cwd_cluster_number, argument);
  get_last_name(source, last_name);
  get_last_name(argument, dest_last_name);

  // Check extension
  char src_name[8];
  if (is_include(last_name, '.'))
  {
    char src_ext[3];
    split_by_first(last_name, '.', src_name);
    memcpy(src_ext, last_name, 3);
    memcpy(src_req.ext, src_ext, 3);
  }
  else
  {
    memcpy(src_req.ext, "dir", 3);
    memcpy(src_name, last_name, 8);
    read_dir_syscall(src_req, &retcode);
    if (retcode == 0)
    {
      memcpy(src_req.ext, "\0\0\0", 3);
      return;
    }
  }

  // Check extension
  char dst_name[8];
  if (is_include(dest_last_name, '.'))
  {
    char dst_ext[3];
    split_by_first(dest_last_name, '.', dst_name);
    memcpy(dst_ext, dest_last_name, 3);
    memcpy(dst_req.ext, dst_ext, 3);
  }
  else
  {
    memcpy(dst_req.ext, "dir", 3);
    memcpy(dst_name, dest_last_name, 8);
    read_dir_syscall(dst_req, &retcode);
    if (retcode == 0)
    {
      memcpy(dst_req.ext, "\0\0\0", 3);
      return;
    }
  }

  memcpy(src_req.name, src_name, 8);
  memcpy(dst_req.name, dst_name, 8);

  src_req.buffer_size = CLUSTER_SIZE;
  dst_req.buffer_size = CLUSTER_SIZE;
  src_req.buf = source_buffer;
  dst_req.buf = dest_buffer;
  puts(src_req.name, 8, 0xF);
  puts(".", 1, 0xF);
  puts(src_req.ext, 3, 0xF);
  puts("\n", 1, 0xF);
  puts(dst_req.name, 8, 0xF);
  puts(".", 1, 0xF);
  puts(dst_req.ext, 3, 0xF);

  sys_move(&src_req, &dst_req);
}

void touch(char *argument)
{
  char filename[8];
  split_by_first(argument, '.', filename);

  request.buffer_size = CLUSTER_SIZE;
  request.buf = "ini hasil tes touch" + '\0';
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

// void echo(char *argument)
// {
//   remove_space(argument);
//   remove_petik(argument);
//   char text[strlen(argument)];
//   split_by_first(argument, '>', text);

//   request.buffer_size = CLUSTER_SIZE;
//   text[strlen(text)] = '\0';

//   request.parent_cluster_number = cwd_cluster_number;
//   char name[8];
//   split_by_first(argument, '.', name);

//   if (strlen(argument) < 3)
//   {
//     argument[strlen(argument)] = '\0';
//   }
//   else
//   {
//     argument[8] = '\0';
//   }
//   if (strlen(name) < 3)
//   {
//     name[strlen(name)] = '\0';
//   }
//   else
//   {
//     name[8] = '\0';
//   }

//   request.buf = text;
//   memcpy(request.ext, argument, 3);
//   memcpy(request.name, name, 8);

//   read_syscall(request, &retcode);
//   if (retcode == 0)
//   {
//     delete_syscall(request, &retcode);
//     write_syscall(request, &retcode);

//     if (retcode != 0)
//     {
//       puts("Unknown error.\n", 15, 0xF);
//     }
//     else
//     {
//       puts("File '", 8, 0xF);
//       puts(request.name, 8, 0xF);
//       puts(".", 1, 0xF);
//       puts(request.ext, 3, 0xF);
//       puts("' updated.\n", 12, 0xF);
//     }
//   }
//   else if (retcode == 1)
//   {
//     puts("Not a file.\n", 12, 0xF);
//   }
//   else if (retcode == 3)
//   {
//     puts("Not found.\n", 11, 0xF);
//   }
//   else
//   {
//     puts("Unknown error", 13, 0xF);
//   }
// }

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
    else if (!memcmp(buf, "touch", 5))
    {
      char *argument = buf + 6;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        touch(argument);
      }
    }
    // else if (!memcmp(buf, "echo", 4))
    // {
    //   char *argument = buf + 5;
    //   remove_newline(argument);
    //   if (strlen(argument) > 0)
    //   {
    //     echo(argument);
    //   }
    // }
    else if (!memcmp(buf, "cat", 3))
    {
      char *argument = buf + 4;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        cat(argument);
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
    else if (!memcmp(buf, "find", 4))
    {
      char directories[255];
      memset(directories, '\0', sizeof(directories));

      char *argument = buf + 5;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        find(argument);
      }
    }
    else if (!memcmp(buf, "cp", 2))
    {
      char *argument = buf + 3;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        cp(argument);
      }
    }
    else if (!memcmp(buf, "mv", 2))
    {
      char *argument = buf + 3;
      remove_newline(argument);
      if (strlen(argument) > 0)
      {
        mv(argument);
      }
    }
    else if (!memcmp(buf, "exit", 4))
    {
      break;
    }
    else
    {
      puts("Command not found.\n", 20, 0x4);
    }
  } while (true);

  return 0;
}