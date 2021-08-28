#include "mouse.hpp"

#include "graphics.hpp"

namespace {
  const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
  };
}

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position) {
  for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
    for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
      if (mouse_cursor_shape[dy][dx] == '@') {
        // 黒色
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, {0, 0, 0});
      } else if (mouse_cursor_shape[dy][dx] == '.') {
        // 白色
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, {255, 255, 255});
      } else {
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, kMouseTransparentColor); // Window::DrawTo 関数で c != tc の条件文で書き込むか書き込まないかを決める。
      }
    }
  }
}
