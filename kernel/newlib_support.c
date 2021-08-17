#include <errno.h>
#include <sys/types.h>

caddr_t sbrk(int incr) {
  // 改行を表現するために追加
  errno = ENOMEM;
  return (caddr_t)-1;
}
