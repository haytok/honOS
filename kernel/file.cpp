#include "file.hpp"

#include <cstdio>

size_t PrintToFD(FileDescriptor& fd, const char* format, ...) {
  va_list ap;
  int result;
  char s[128];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  fd.Write(s, result);
  return result;
}

// 指定された文字をヌル文字に変換する。
size_t ReadDelim(FileDescriptor& fd, char delim, char* buf, size_t len) {
  size_t i = 0; // 指定された文字 delim までのインデックスを格納する。
  for (; i < len - 1; ++i) {
    if (fd.Read(&buf[i], 1) == 0) {
      break;
    }
    if (buf[i] == delim) {
      ++i;
      break;
    }
  }
  buf[i] = '\0';
  return i;
}
