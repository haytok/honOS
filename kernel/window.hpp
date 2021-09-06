#pragma once

#include <vector>
#include <optional>
#include <string>
#include "graphics.hpp"
#include "frame_buffer.hpp"

// 基底クラスになる
class Window {
 public:
  class WindowWriter : public PixelWriter {
   public:
    WindowWriter(Window& window) : window_{window} {}
    virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
      window_.Write(pos, c);
    }

    virtual int Width() const override { return window_.Width(); }
    virtual int Height() const override { return window_.Height(); }

   private:
    Window& window_;
  };

  Window(int width, int height, PixelFormat shadow_format);
  virtual ~Window() = default;
  // この delete の意味を理解できていない
  Window(const Window& rhs) = delete;
  Window& operator=(const Window& rhs) = delete;

  // 与えられた PixelWriter にウィンドウの表示領域を描画する
  void DrawTo(FrameBuffer& dst, Vector2D<int> pos, const Rectangle<int>& area);
  void SetTransparentColor(std::optional<PixelColor> c);
  WindowWriter* Writer();

  // 指定した位置のピクセルを返す
  const PixelColor& At(Vector2D<int> pos) const;
  void Write(Vector2D<int> pos, PixelColor c);

  int Width() const;
  int Height() const;
  Vector2D<int> Size() const;

  void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

  virtual void Activate() {};
  virtual void Deactivate() {};

 private:
  int width_, height_;
  std::vector<std::vector<PixelColor>> data_{};
  WindowWriter writer_{*this}; // 普段は初期値を与えて実装していないけど、与えるとこのような書き方にになる
  std::optional<PixelColor> transparent_color_{std::nullopt};

  FrameBuffer shadow_buffer_{};
};

class ToplevelWindow : public Window {
 public:
  static constexpr Vector2D<int> kTopLeftMargin{4, 24};
  static constexpr Vector2D<int> kBottomRightMargin{4, 4};

  // ただ単に始点の座標を Layer ベースではなく、書き込む領域ベースにする。
  class InnerAreaWriter : public PixelWriter {
   public:
    InnerAreaWriter(ToplevelWindow& window) : window_{window} {};
    virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
      window_.Write(pos + kTopLeftMargin, c);
    };
    virtual int Width() const override {
      return window_.Width() - kTopLeftMargin.x - kBottomRightMargin.x; };
    virtual int Height() const override {
      return window_.Height() - kTopLeftMargin.y - kBottomRightMargin.y; };

   private:
    ToplevelWindow& window_;
  };

  ToplevelWindow(int width, int height, PixelFormat shadow_format,
                 const std::string& title);

  virtual void Activate() override;
  virtual void Deactivate() override;

  InnerAreaWriter* InnerWriter() { return &inner_writer_; };
  Vector2D<int> InnerSize() const;

 private:
  std::string title_;
  InnerAreaWriter inner_writer_{*this};
};

void DrawWindow(PixelWriter& writer, const char* title);
void DrawTextbox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size);
void DrawWindowTitle(PixelWriter& writer, const char* title, bool active);
