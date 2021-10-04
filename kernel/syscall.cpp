#include "syscall.hpp"

#include <array>
#include <cstdint>
#include <cerrno>
#include <cmath>
#include <fcntl.h>

#include "asmfunc.h"
#include "msr.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "font.hpp"
#include "timer.hpp"
#include "keyboard.hpp"
#include "app_event.hpp"

namespace syscall {
  struct Result {
    uint64_t value;
    int error;
  };

#define SYSCALL(name) \
  Result name( \
      uint64_t arg1, uint64_t arg2, uint64_t arg3, \
      uint64_t arg4, uint64_t arg5, uint64_t arg6)

SYSCALL(LogString) {
  if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
    return { 0, EPERM };
  }
  const char* s = reinterpret_cast<const char*>(arg2);
  const auto len = strlen(s);
  if (len > 1024) {
    return { 0, E2BIG };
  }
  Log(static_cast<LogLevel>(arg1), "%s", s);
  return { len, 0 };
}

SYSCALL(PutString) {
  const auto fd = arg1;
  const char* s = reinterpret_cast<const char*>(arg2);
  const auto len = arg3;
  if (len > 1024) {
    return { 0, E2BIG };
  }

  if (fd == 1) {
    const auto task_id = task_manager->CurrentTask().ID(); // このシステムコールを実行しているタスクの ID がわかると、どのターミナルから実行されているかがわかる。
    (*terminals)[task_id]->Print(s, len);
    return { len, 0 };
  }
  return { 0, EBADF };
}

SYSCALL(Exit) {
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");
  // アセンブリでシステムコール番号が 0x80000002 の処理が呼び出された時に役立つ返り値である。
  // rax に task.OSStackPointer() が rdx に static_cast<int>(arg1) が引き渡される。
  return { task.OSStackPointer(), static_cast<int>(arg1) };
}

SYSCALL(OpenWindow) {
  const int w = arg1, h = arg2, x = arg3, y = arg4;
  const auto title = reinterpret_cast<const char*>(arg5);
  const auto win = std::make_shared<ToplevelWindow>(
      w, h, screen_config.pixel_format, title);

  __asm__("cli");
  const auto layer_id = layer_manager->NewLayer()
    .SetWindow(win)
    .SetDraggable(true)
    .Move({x, y})
    .ID();
  active_layer->Activate(layer_id);

  // アクティブなウィンドウにメッセージを送信できるようにする。
  const auto task_id = task_manager->CurrentTask().ID();
  layer_task_map->insert(std::make_pair(layer_id, task_id));
  __asm__("sti");

  return { layer_id, 0 };
}

namespace {
  template <class Func, class... Args>
  Result DoWinFunc(Func f, uint64_t layer_id_flags, Args... args) {
    const uint32_t layer_flags = layer_id_flags >> 32;
    const unsigned int layer_id = layer_id_flags & 0xffffffff;

    __asm__("cli");
    auto layer = layer_manager->FindLayer(layer_id);
    __asm__("sti");
    if (layer == nullptr) {
      return { 0, EBADF };
    }

    // arg2 以降が args に格納されている。
    // これが、f の第二引数以降つまり x, y, color, s に格納されて f 呼び出される。
    const auto res = f(*layer->GetWindow(), args...);
    if (res.error) {
      return res;
    }

    // ビット 0 が 1 の時に再描画をしないような実装にする。
    // つまり、ビット 0 が 0 の時は再描画する。
    if ((layer_flags & 1) == 0) {
      __asm__("cli");
      layer_manager->Draw(layer_id);
      __asm__("sti");
    }

    return res;
  }
}

SYSCALL(WinWriteString) {
  return DoWinFunc(
      [](Window& win,
         int x, int y, uint32_t color, const char* s) {
        WriteString(*win.Writer(), {x, y}, s, ToColor(color));
        return Result{ 0, 0 };
      }, arg1, arg2, arg3, arg4, reinterpret_cast<const char*>(arg5));
}

SYSCALL(WinFillRectangle) {
  return DoWinFunc(
      [](Window& win,
         int x, int y, int w, int h, uint32_t color) {
        FillRectangle(*win.Writer(), {x, y}, {w, h}, ToColor(color));
        return Result{ 0, 0 };
      }, arg1, arg2, arg3, arg4, arg5, arg6);
}

SYSCALL(GetCurrentTick) {
  return { timer_manager->CurrentTick(), kTimerFreq };
}

SYSCALL(WinRedraw) {
  return DoWinFunc(
      [](Window&) {
        return Result{ 0, 0 };
      }, arg1);
}

// struct SyscallResult SyscallWinDrawLine(
//     uint64_t layer_id_flags, int x0, int y0, int x1, int y1, uint32_t color);
SYSCALL(WinDrawLine) {
  return DoWinFunc(
      // このラムダ関数には DoWinFunc 内部で関数 f を呼び出す際の引数を渡す。
      // つまり、この関数自体 (SYSCALL(WinDrawLine)) を呼び出す際に渡す第二引数以降を f に渡せば良いとも言える。
      [](Window& win,
         int x0, int y0, int x1, int y1, uint32_t color) {
        auto sign = [](int x) {
          return (x > 0) ? 1 : (x < 0) ? -1 : 0;
        };
        const int dx = x1 - x0 + sign(x1 - x0);
        const int dy = y1 - y0 + sign(y1 - y0);

        // 点をプロットする
        if (dx == 0 && dy == 0) {
          win.Writer()->Write({x0, y0}, ToColor(color));
          return Result{ 0, 0 };
        }

        const auto floord = static_cast<double(*)(double)>(floor);
        const auto ceild = static_cast<double(*)(double)>(ceil);

        // 傾きが 1 よりも小さい時
        if (abs(dx) >= abs(dy)) {
          if (dx < 0) {
            std::swap(x0, x1);
            std::swap(y0, y1);
          }
          const auto roundish = y1 >= y0 ? floord : ceild;
          const double m = static_cast<double>(dy) / dx;// 傾き
          for (int x = x0; x <= x1; ++x) {
            const int y = roundish(m * (x - x0) + y0);
            win.Writer()->Write({x, y}, ToColor(color));
          }
        } else { // この転置的な処理の実装がよくわからん。
          if (dy < 0) {
            std::swap(x0, x1);
            std::swap(y0, y1);
          }
          const auto roundish = x1 >= x0 ? floord : ceild;
          const double m = static_cast<double>(dx) / dy;
          for (int y = y0; y <= y1; ++y) {
            const int x = roundish(m * (y - y0) + x0);
            win.Writer()->Write({x, y}, ToColor(color));
          }
        }
        return Result{ 0, 0 };
      }, arg1, arg2, arg3, arg4, arg5, arg6);
}

SYSCALL(CloseWindow) {
  const unsigned int layer_id = arg1 & 0xffffffff;
  const auto layer = layer_manager->FindLayer(layer_id);

  if (layer == nullptr) {
    return { EBADF, 0 };
  }

  const auto layer_pos = layer->GetPosition();
  const auto win_size = layer->GetWindow()->Size();

  __asm__("cli");
  active_layer->Activate(0);
  layer_manager->RemoveLayer(layer_id);
  layer_manager->Draw({layer_pos, win_size});
  layer_task_map->erase(layer_id);
  __asm__("sti");

  return { 0, 0 };
}

// winhello アプリケーション内の while (true) で呼び出される。
// events オブジェクトにメッセージの内容を書きこむことで、アプリケーション側でそのメッセージを読み出す。
// struct SyscallResult SyscallReadEvent(struct AppEvent* events, size_t len);
SYSCALL(ReadEvent) {
  if (arg1 < 0x8000'0000'0000'0000) {
    return { 0, EFAULT };
  }
  const auto app_events = reinterpret_cast<AppEvent*>(arg1);
  const size_t len = arg2;

  __asm__("cli");
  auto& task = task_manager->CurrentTask(); // アプリケーションを動かしているターミナルを指す。
  __asm__("sti");
  size_t i = 0;

  while (i < len) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg && i == 0) {
      task.Sleep();
      continue;
    }
    __asm__("sti");

    if (!msg) {
      break;
    }

    switch (msg->type) {
    case Message::kKeyPush:
      if (msg->arg.keyboard.keycode == 20 /* Q key */ &&
          msg->arg.keyboard.modifier & (kLControlBitMask | kRControlBitMask)) {
        // SyscallReadEvent の第一引数で渡ってきた events (この関数内ではキャストして app_event と言う変数名になっている。) に書きこむ。
        // このシステムコール側で書き込んだ変数を呼び出し元の winhello 関数で読み出す。
        app_events[i].type = AppEvent::kQuit;
        ++i;
      } else {
        app_events[i].type = AppEvent::kKeyPush;
        app_events[i].arg.keypush.modifier = msg->arg.keyboard.modifier;
        app_events[i].arg.keypush.keycode = msg->arg.keyboard.keycode;
        app_events[i].arg.keypush.ascii = msg->arg.keyboard.ascii;
        app_events[i].arg.keypush.press = msg->arg.keyboard.press;
        ++i;
      }
      break;
    case Message::kMouseMove:
      app_events[i].type = AppEvent::kMouseMove;
      app_events[i].arg.mouse_move.x = msg->arg.mouse_move.x;
      app_events[i].arg.mouse_move.y = msg->arg.mouse_move.y;
      app_events[i].arg.mouse_move.dx = msg->arg.mouse_move.dx;
      app_events[i].arg.mouse_move.dy = msg->arg.mouse_move.dy;
      app_events[i].arg.mouse_move.buttons = msg->arg.mouse_move.buttons;
      ++i;
      break;
    case Message::kMouseButton:
      app_events[i].type = AppEvent::kMouseButton;
      app_events[i].arg.mouse_button.x = msg->arg.mouse_button.x;
      app_events[i].arg.mouse_button.y = msg->arg.mouse_button.y;
      app_events[i].arg.mouse_button.press = msg->arg.mouse_button.press;
      app_events[i].arg.mouse_button.button = msg->arg.mouse_button.button;
      ++i;
      break;
    case Message::kTimerTimeout:
      if (msg->arg.timer.value < 0) {
        app_events[i].type = AppEvent::kTimerTimeout;
        app_events[i].arg.timer.timeout = msg->arg.timer.timeout;
        app_events[i].arg.timer.value = -msg->arg.timer.value; // アプリケーションから受け取った値を反転させる。
        ++i;
      }
      break;
    default:
      Log(kInfo, "uncaught event type; %u\n", msg->type);
    }
  }

  return { i, 0 };
}

// struct SyscallResult SyscallCreateTimer(unsigned int type, int timer_value, unsigned long timeout_ms);
SYSCALL(CreateTimer) {
  const unsigned int mode = arg1;
  const int timer_value = arg2;
  if (timer_value <= 0) {
    return { 0, EINVAL };
  }

  __asm__("cli");
  const uint64_t task_id = task_manager->CurrentTask().ID();
  __asm__("sti");

  unsigned long timeout = arg3 * kTimerFreq / 1000;
  if (mode & 1) {
    timeout += timer_manager->CurrentTick();
  }

  __asm__("cli");
  timer_manager->AddTimer(Timer{timeout, -timer_value, task_id}); // timer_value を負の値にすることでアプリケーションが生成したタイマと区別する。
  __asm__("sti");
  return { timeout * 1000 / kTimerFreq, 0 };
}

namespace {
  size_t AllocateFD(Task& task) {
    const size_t num_files = task.Files().size();
    for (size_t i = 0; i < num_files; ++i) {
      if (!task.Files()[i]) {
        return i;
      }
    }
    task.Files().emplace_back();
    return num_files;
  }
} // namespace

// newlib_support.c 内で呼び出される。
// int open(const char* path, int flags) {
// struct SyscallResult res = SyscallOpenFile(path, flags);
SYSCALL(OpenFile) {
  const char* path = reinterpret_cast<const char*>(arg1);
  const int flags = arg2;
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  if (strcmp(path, "@stdin") == 0) {
    return { 0, 0 }; // エラーが生じず fd が 0 になるので、呼び出し元の箇所の後で ReadFile システムコールが呼び出されると、標準入力を読み出す。
  }

  if ((flags & O_ACCMODE) == O_WRONLY) { // 書き込み専用のモードの時
    return { 0, EINVAL };
  }

  auto [ dir, post_slash ] = fat::FindFile(path);
  if (dir == nullptr) {
    return { 0, ENOENT };
  } else if (dir->attr != fat::Attribute::kDirectory && post_slash) { // ディレクトリ出ないにも関わらず末尾に / があるケース
    return { 0, ENOENT };
  }

  size_t fd = AllocateFD(task);
  task.Files()[fd] = std::make_unique<fat::FileDescriptor>(*dir); // ファイルディスクリプタのテーブルには、ディレクトリエントリの実体を格納する。
  return { fd, 0 };
}

// newlib_support.c 内で呼び出される。
// ssize_t read(int fd, void* buf, size_t count) {
// struct SyscallResult res = SyscallReadFile(fd, buf, count);
SYSCALL(ReadFile) {
  const int fd = arg1;
  void* buf = reinterpret_cast<void*>(arg2);
  size_t count = arg3;
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  if (fd < 0 || task.Files().size() <= fd || !task.Files()[fd]) {
    return { 0, EBADF };
  }
  return { task.Files()[fd]->Read(buf, count), 0 };
}

#undef SYSCALL

} // namespace syscall

using SyscallFuncType = syscall::Result (uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);

extern "C" std::array<SyscallFuncType*, 0xe> syscall_table{
  /* 0x00 */ syscall::LogString,
  /* 0x01 */ syscall::PutString,
  /* 0x02 */ syscall::Exit,
  /* 0x03 */ syscall::OpenWindow,
  /* 0x04 */ syscall::WinWriteString,
  /* 0x05 */ syscall::WinFillRectangle,
  /* 0x06 */ syscall::GetCurrentTick,
  /* 0x07 */ syscall::WinRedraw,
  /* 0x08 */ syscall::WinDrawLine,
  /* 0x09 */ syscall::CloseWindow,
  /* 0x0a */ syscall::ReadEvent,
  /* 0x0b */ syscall::CreateTimer,
  /* 0x0c */ syscall::OpenFile,
  /* 0x0d */ syscall::ReadFile,
};

void InitializeSyscall() {
  WriteMSR(kIA32_EFER, 0x0501u); // syscall を使えるようにするための設定
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry)); // syscall が呼び出された時にシステムコール番号に従って呼び出す関数を登録するための場所
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 |
                        static_cast<uint64_t>(16 | 3) << 48);
  WriteMSR(kIA32_FMASK, 0);
}
