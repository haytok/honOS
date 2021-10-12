#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include "../syscall.h"

extern "C" void main(int argc, char** argv) {
  SyscallResult res = SyscallOpenFile("/memmap", O_RDONLY);
  if (res.error) {
    exit(res.error);
  }
  const int fd = res.value;
  size_t file_size;
  res = SyscallMapFile(fd, &file_size, 0); // struct SyscallResult SyscallMapFile(int fd, size_t* file_size, int flags);
  if (res.error) {
    exit(res.error);
  }

  char* p = reinterpret_cast<char*>(res.value);
  // 以下の for 文をコメントアウトすると、PF 時に呼び出される HandlePageFault が呼び出されない。
  // つまり、PF が発生した時にページキャシュが割り当てられていることが確認できる。
  for (size_t i = 0; i < file_size; ++i) {
    printf("%c", p[i]);
  }
  printf("\nread from mapped file (%lu bytes)\n", file_size);

  exit(0);
}
