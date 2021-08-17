#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"

void* operator new(size_t size, void* buf) {
  return buf;
}

void operator delete(void* obj) noexcept {}

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

extern "C" void KernelMain(const struct FrameBufferConfig& frame_buffer_config) {
  switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)RGBResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)PixelBGRResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
  }

  Console console{*pixel_writer, {0, 0, 0}, {255, 255, 255}};
  for (int i = 0; i < 27; ++i) {
    char buf[128];
    // Kernel 内で sprintf をするためのロジックは、day05d で実装
    sprintf(buf, "line%d\n", i);
    console.PutString(buf);
  }

  while (1) __asm__("hlt");
}
