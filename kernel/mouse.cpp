#include "mouse.hpp"

#include <limits>
#include <memory>
#include "graphics.hpp"
#include "layer.hpp"
#include "usb/classdriver/mouse.hpp"
#include "task.hpp"

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

  std::tuple<Layer*, uint64_t> FindActiveLayerTask() {
    const auto act = active_layer->GetActive();
    if (!act) {
      return { nullptr, 0 };
    }
    const auto layer = layer_manager->FindLayer(act);
    if (!layer) {
      return { nullptr, 0 };
    }

    const auto task_it = layer_task_map->find(act);
    if (task_it == layer_task_map->end()) {
      return { layer, 0 };
    }
    return { layer, task_it->second };
  }

  // マウスが動いていると、アクティブなレイヤのタスクにメッセージが飛ぶ。
  void SendMouseMessage(Vector2D<int> newpos, Vector2D<int> posdiff,
                        uint8_t buttons, uint8_t previous_buttons) {
    const auto [ layer, task_id ] = FindActiveLayerTask();
    if (!layer || !task_id) {
      return;
    }

    const auto relpos = newpos - layer->GetPosition();
    if (posdiff.x != 0 || posdiff.y != 0) {
      // マウスの移動の相対座標を求める。
      Message msg{Message::kMouseMove};
      msg.arg.mouse_move.x = relpos.x;
      msg.arg.mouse_move.y = relpos.y;
      msg.arg.mouse_move.dx = posdiff.x;
      msg.arg.mouse_move.dy = posdiff.y;
      msg.arg.mouse_move.buttons = buttons;
      // layer_task_map = new std::map<unsigned int, uint64_t>;
      // この定義より、task_it->second が後ろの値の task_id であることがわかる。
      task_manager->SendMessage(task_id, msg);
    }

    if (previous_buttons != buttons) {
      const auto diff = previous_buttons ^ buttons; // ボタンの状態が違うビットだけ立つ。
      for (int i = 0; i < 8; ++i) {
        if ((diff >> i) & 1) {
          Message msg{Message::kMouseButton};
          msg.arg.mouse_button.x = relpos.x;
          msg.arg.mouse_button.y = relpos.y;
          msg.arg.mouse_button.press = (buttons >> i) & 1; // ボタンが押されているかどうかを判定するフラグ。
          msg.arg.mouse_button.button = i; // 左ボタンが 0 に該当する。
          task_manager->SendMessage(task_id, msg);
        }
      }
    }
  }

  // 閉じるボタンをクリックした時にそのタスクにメッセージを投げるための関数
  void SendCloseMessage() {
    const auto [ layer, task_id ] = FindActiveLayerTask();
    if (!layer || !task_id) {
      return;
    }

    Message msg{Message::kWindowClose};
    msg.arg.window_close.layer_id = layer->ID();
    task_manager->SendMessage(task_id, msg);
  }
} // namespace

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

Mouse::Mouse(unsigned int layer_id) : layer_id_{layer_id} {}

void Mouse::SetPosition(Vector2D<int> position) {
  position_ = position;
  layer_manager->Move(layer_id_, position_);
}

void Mouse::OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  const auto oldpos = position_;
  // 絶対座標でマウスを移動させるように変更する
  auto newpos = position_ + Vector2D<int>{displacement_x, displacement_y};
  newpos = ElementMin(newpos, ScreenSize() + Vector2D<int>{-1, -1});
  position_ = ElementMax(newpos, {0, 0});

  const auto posdiff = position_ - oldpos;

  layer_manager->Move(layer_id_, position_); // layer_manager->Move(); の中で Draw() を呼び出すようになったから、次行にあった layer_manager->Draw() を消す。

  unsigned int close_layer_id = 0;

  // マウスだけではなく、ウィンドウも動かす処理を実装する。
  const bool previous_left_pressed = (previous_buttons_ & 0x01); // ボタンが押され続けているかを確認
  const bool left_pressed = (buttons & 0x01);
  // 始めて押された時
  if (!previous_left_pressed && left_pressed) {
    // 押された瞬間の if 文なので、第一引数には、マウスの位置を表す変数を渡して呼び出す。
    // ポイントは、押した瞬間のマウスの位置が優先されることである。
    auto layer = layer_manager->FindLayerByPosition(position_, layer_id_);
    if (layer && layer->IsDraggable()) {
      // カーソルの場所によってする行う処理を変える。
      const auto pos_layer = position_ - layer->GetPosition();
      switch (layer->GetWindow()->GetWindowRegion(pos_layer)) {
      case WindowRegion::kTitleBar:
        drag_layer_id_ = layer->ID();
        break;
      case WindowRegion::kCloseButton: // タイトルバーをクリックした時ドラッグできるようにする。
        close_layer_id = layer->ID();
        break;
      default:
        break;
      }
      active_layer->Activate(layer->ID());
    } else {
      active_layer->Activate(0); // 一番最下層のレイヤをアクティブにする。
    }
  } else if (previous_left_pressed && left_pressed) {
    if (drag_layer_id_ > 0) {
      layer_manager->MoveRelative(drag_layer_id_, posdiff);
    }
  } else if (previous_left_pressed && !left_pressed) {
    drag_layer_id_ = 0;
  }

  // 特に何もドラッグしていずにマウスを動かしているケース
  if (drag_layer_id_ == 0) {
    if (close_layer_id == 0) {
      SendMouseMessage(newpos, posdiff, buttons, previous_buttons_);
    } else {
      SendCloseMessage();
    }
  }
  previous_buttons_ = buttons;
}

void InitializeMouse() {
  auto mouse_window = std::make_shared<Window>(
    kMouseCursorWidth, kMouseCursorHeight, screen_config.pixel_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

  auto mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window) // この後で main 関数で実装していたように Move メソッドを呼び出すのではなく、一旦位置情報を Mouse クラスの変数 position_ に格納してから、割り込みハンドラで Move メソッドを呼び出すように修正する。
    .ID();

  auto mouse = std::make_shared<Mouse>(mouse_layer_id);
  mouse->SetPosition({200, 200});
  layer_manager->UpDown(mouse->LayerID(), std::numeric_limits<int>::max());

  usb::HIDMouseDriver::default_observer =
    [mouse](uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
      mouse->OnInterrupt(buttons, displacement_x, displacement_y);
    };

  active_layer->SetMouseLayer(mouse_layer_id);
}
