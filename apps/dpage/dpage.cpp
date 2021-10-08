#include <cstdio>
#include <cstdlib>
#include "../syscall.h"

// dpage <path> <num>
extern "C" void main(int argc, char** argv) {
  const char* filename = "/memmap";
  int ch = '\n';
  if (argc >= 3) {
    filename = argv[1];
    ch = atoi(argv[2]);
  }
  FILE* fp = fopen(filename, "r");
  if (!fp) {
    printf("failed to open %s\n", filename);
    exit(1);
  }

  // ひとまず 1 ページ分のメモリ領域を確保する。
  SyscallResult res = SyscallDemandPages(1, 0);
  if (res.error) {
    exit(1);
  }
  char* buf = reinterpret_cast<char*>(res.value); // head を進めていく
  char* buf0 = buf;

  size_t total = 0;
  size_t n;
  while ((n = fread(buf, 1, 4096, fp)) == 4096) {
    total += n;
    if (res = SyscallDemandPages(1, 0); res.error) {
      exit(1);
    }
    buf += 4096; // 4096 バイトを読み出すと、再度 SyscallDemandPages システムコールでメモリ領域を確保する。
  }
  total += n; // 読み出すのが 4096 バイトより少ない時は、fread で読み出した結果、while 文の条件式が false になって、while 文を抜ける。
  printf("size of %s = %lu bytes\n", filename, total);

  size_t num = 0;
  for (int i = 0; i < total; ++i) {
    if (buf0[i] == ch) {
      ++num;
    }
  }
  printf("the number of '%c' (0x%02x) = %lu\n", ch, ch, num);
  exit(0);
}
