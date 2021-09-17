#include <cstring>
#include <cstdlib>

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

extern "C" int main(int argc, char** argv) {
  stack_ptr = -1;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
    } else {
      long a = atol(argv[i]);
      Push(a);
    }
  }
  if (stack_ptr < 0) {
    return 0;
  }
  return static_cast<int>(Pop());
}
