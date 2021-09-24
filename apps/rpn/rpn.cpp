#include <cstring>
#include <cstdlib>
#include <cstdint>

#include "../../kernel/logger.hpp"

int stack_ptr;
long stack[100];

long Pop() {
  long value = stack[stack_ptr];
  --stack_ptr;
  return value;
}

// スタックポインタをインクリメントしてその先に値を格納する。
void Push(long value) {
  ++stack_ptr;
  stack[stack_ptr] = value;
}

// この関数 SyscallLogString 自体は apps/rpn/syscall.asm の中で定義されている。
// この関数の中で syscall が呼び出され、sycall_table に登録されたシステムコール番号に応じた関数が実行される。
extern "C" int64_t SyscallLogString(LogLevel, const char*);

extern "C" int main(int argc, char** argv) {
  stack_ptr = -1;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
      SyscallLogString(kWarn, "+");
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
      SyscallLogString(kWarn, "-");
    } else {
      long a = atol(argv[i]);
      Push(a);
      SyscallLogString(kWarn, "#");
    }
  }

  if (stack_ptr < 0) {
    return 0;
  }
  SyscallLogString(kWarn, "\nhello, this is rpn\n");
  while (1);
  // return static_cast<int>(Pop());
}
