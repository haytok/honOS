#include "terminal.hpp"

#include "font.hpp"
#include "layer.hpp"
#include "pci.hpp"
#include "asmfunc.h"
#include "elf.hpp"

#include <cstring>


namespace {
// C 言語の main(int argc, char** argv) の argv のオブジェクトを作成するイメージ。
// argv にはその実行ファイル名とそれに続く引数が格納されている。
std::vector<char*> MakeArgVector(char* command, char* first_arg) {
  std::vector<char*> argv;
  argv.push_back(command);

  if (!first_arg) {
    return argv;
  }

  char* p = first_arg;
  while (true) {
    // 引数の一番初めの空白スペースをスキップする。
    while (isspace(p[0])) {
      ++p;
    }
    if (p[0] == 0) {
      break;
    }
    // この時点でポインタ p は引数の文字列の先頭にある。
    argv.push_back(p);

    while (p[0] != 0 && !isspace(p[0])) {
      ++p;
    }
    if (p[0] == 0) {
      break;
    }
    p[0] = 0;
    ++p;
  }

  return argv;
}
}

Terminal::Terminal() {
  window_ = std::make_shared<ToplevelWindow>(
    kColumns * 8 + 8 + ToplevelWindow::kMarginX,
    kRows * 16 + 8 + ToplevelWindow::kMarginY,
    screen_config.pixel_format,
    "HonoTerm");
  DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

  layer_id_ = layer_manager->NewLayer()
    .SetWindow(window_)
    .SetDraggable(true)
    .ID();

  Print(">");
  cmd_history_.resize(8);
}

Rectangle<int> Terminal::BlinkCursor() {
  cursor_visible_ = !cursor_visible_;
  DrawCursor(cursor_visible_);

  return {CalcCursorPos(), {7, 15}};
}

Rectangle<int> Terminal::InputKey(
    uint8_t modifier, uint8_t keycode, char ascii) {
  DrawCursor(false);

  Rectangle<int> draw_area{CalcCursorPos(), {8 * 2, 16}};

  if (ascii == '\n') {
    linebuf_[linebuf_index_] = 0;
    if (linebuf_index_ > 0) {
      cmd_history_.pop_back();
      cmd_history_.push_front(linebuf_);
    }
    linebuf_index_ = 0;
    cmd_history_index_ = -1;
    cursor_.x = 0;
    if (cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      Scroll1();
    }
    ExecuteLine();
    Print(">");
    draw_area.pos = ToplevelWindow::kTopLeftMargin;
    draw_area.size = window_->InnerSize();
  } else if (ascii == '\b') {
    if (cursor_.x > 0) {
      --cursor_.x;
      FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
      draw_area.pos = CalcCursorPos();

      if (linebuf_index_ > 0) {
        --linebuf_index_;
      }
    }
  } else if (ascii != 0) {
    if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
      linebuf_[linebuf_index_] = ascii;
      ++linebuf_index_;
      WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
      ++cursor_.x;
    }
  } else if (keycode == 0x51) { // 下矢印 (新しい履歴を遡る)
    draw_area = HistoryUpDown(-1);
  } else if (keycode == 0x52) { // 上矢印 (古い履歴を遡る)
    draw_area = HistoryUpDown(1);
  }

  DrawCursor(true);

  return draw_area;
}

void Terminal::DrawCursor(bool visible) {
  const auto color = visible ? ToColor(0xffffff) : ToColor(0);
  FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
}

Vector2D<int> Terminal::CalcCursorPos() const {
  return ToplevelWindow::kTopLeftMargin +
      Vector2D<int>{4 + 8 * cursor_.x, 4 + 16 * cursor_.y};
}

void Terminal::Scroll1() {
  Rectangle<int> move_src{
    ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
    {8 * kColumns, 16 * (kRows - 1)}
  };
  window_->Move(ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);
  FillRectangle(*window_->InnerWriter(),
                {4, 4 + 16 * cursor_.y}, {8 * kColumns, 16}, {0, 0, 0});
}

void Terminal::ExecuteLine() {
  char* command = &linebuf_[0];
  char* first_arg = strchr(&linebuf_[0], ' '); // 一番初め見つかる空白文字へのポインタを返す。
  if (first_arg) {
    *first_arg = 0; // 一番初めに見つかった空白文字に塗る文字を入れて、文字列の終端を表す。
    ++first_arg; // command の空白スペースが空いた次の引数が入る。
  }
  if (strcmp(command, "echo") == 0) {
    if (first_arg) {
      Print(first_arg);
    }
    Print("\n");
  } else if (strcmp(command, "clear") == 0) {
    FillRectangle(*window_->InnerWriter(),
                  {4, 4}, {8 * kColumns, 16 * kRows}, {0, 0, 0});
    cursor_.y = 0; // ExecuteLine の呼び出し元の直前で cursor_x = 0; が実行されているので、cursor_.y = 0; のみで良い。
  } else if (strcmp(command, "lspci") == 0) {
    char s[64];
    for (int i = 0; i < pci::num_device; ++i) {
      const auto& dev = pci::devices[i];
      auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
      sprintf(s, "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
          dev.bus, dev.device, dev.function, vendor_id, dev.header_type,
          dev.class_code.base, dev.class_code.sub, dev.class_code.interface);
      Print(s);
    }
  } else if (strcmp(command, "ls") == 0) {
    auto root_dir_entries = fat::GetSectorByCluster<fat::DirectoryEntry>(
      fat::boot_volume_image->root_cluster);
    auto entries_per_cluster =
        fat::bytes_per_cluster / sizeof(fat::DirectoryEntry);
    char base[9], ext[4];
    char s[64];
    for (int i = 0; i < entries_per_cluster; ++i) {
      ReadName(root_dir_entries[i], base, ext);
      if (base[0] == 0x00) {
        break;
      } else if (static_cast<uint8_t>(base[0]) == 0xe5) {
        continue;
      } else if (root_dir_entries[i].attr == fat::Attribute::kLongName) {
        continue;
      }

      if (ext[0]) {
        sprintf(s, "%s.%s\n", base, ext);
      } else {
        sprintf(s, "%s\n", base);
      }
      Print(s);
    }
  } else if (strcmp(command, "cat") == 0) { // cat コマンドはファイルの中身を標準出力に出すコマンドである。
    char s[64];

    auto file_entry = fat::FindFile(first_arg);
    if (!file_entry) {
      sprintf(s, "no such file: %s\n", first_arg);
      Print(s);
    } else {
      auto cluster = file_entry->FirstCluster();
      auto remain_bytes = file_entry->file_size;

      DrawCursor(false);
      // クラスタに跨るファイルの実体を出力する
      while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
        char* p = fat::GetSectorByCluster<char>(cluster); // この p の先頭からファイルの実体が存在する。

        int i = 0;
        for (; i < fat::bytes_per_cluster && i < remain_bytes; ++i) {
          Print(*p);
          ++p;
        }
        remain_bytes -= i;
        cluster = fat::NextCluster(cluster);
      }
      DrawCursor(true);
    }
  } else if (command[0] != 0) {
    auto file_entry = fat::FindFile(command);
    if (!file_entry) {
      Print("no such command: ");
      Print(command);
      Print("\n");
    } else {
      ExecuteFile(*file_entry, command, first_arg);
    }
  }
}

// cat コマンドでファイルを読み出すのと処理が似ている。
void Terminal::ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg) {
  auto cluster = file_entry.FirstCluster();
  auto remain_bytes = file_entry.file_size;

  std::vector<uint8_t> file_buf(remain_bytes);
  auto p = &file_buf[0];

  while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
    // ファイルの中身を file_buf にコピーする
    const auto copy_bytes = fat::bytes_per_cluster < remain_bytes ?
      fat::bytes_per_cluster : remain_bytes;
    memcpy(p, fat::GetSectorByCluster<uint8_t>(cluster), copy_bytes);

    remain_bytes -= copy_bytes;
    p += copy_bytes;
    cluster = fat::NextCluster(cluster);
  }

  auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
  // ELF 形式のフォーマットで無い時
  if (memcmp(elf_header->e_ident, "\x7f" "ELF", 4) != 0) {
    using Func = void ();
    auto f = reinterpret_cast<Func*>(&file_buf[0]);
    f();
    return;
  }

  auto argv = MakeArgVector(command, first_arg);

  auto entry_addr = elf_header->e_entry;
  entry_addr += reinterpret_cast<uintptr_t>(&file_buf[0]);
  using Func = int (int, char**);
  auto f = reinterpret_cast<Func*>(entry_addr);
  auto ret = f(argv.size(), &argv[0]);

  char s[64];
  sprintf(s, "app exited. ret = %d\n", ret);
  Print(s);
}

void Terminal::Print(char c) {
  auto newline = [this]() {
    cursor_.x = 0;
    if (cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      Scroll1();
    }
  };

  if (c == '\n') {
    newline();
  } else {
    WriteAscii(*window_->Writer(), CalcCursorPos(), c, {255, 255, 255});
    if (cursor_.x == kColumns - 1) {
      newline();
    } else {
      ++cursor_.x;
    }
  }
}

void Terminal::Print(const char* s) {
  DrawCursor(false);

  while (*s) {
    Print(*s);
    ++s;
  }

  DrawCursor(true);
}

Rectangle<int> Terminal::HistoryUpDown(int direction) {
  if (direction == -1 && cmd_history_index_ >= 0) {
    --cmd_history_index_;
  } else if (direction == 1 && cmd_history_index_ + 1 < cmd_history_.size()) {
    ++cmd_history_index_;
  }
  cursor_.x = 1;
  const auto first_pos = CalcCursorPos();

  Rectangle<int> draw_area{first_pos, {8 * (kColumns - 1), 16}};
  FillRectangle(*window_->Writer(), draw_area.pos, draw_area.size, {0, 0, 0}); // 一旦黒で塗りつぶす。

  const char* history = "";
  if (cmd_history_index_ >= 0) {
    history = &cmd_history_[cmd_history_index_][0];
  }

  strcpy(&linebuf_[0], history);
  linebuf_index_ = strlen(history);

  WriteString(*window_->Writer(), first_pos, history, {255, 255, 255});
  cursor_.x = linebuf_index_ + 1;
  return draw_area;
}

void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  Terminal* terminal = new Terminal;
  layer_manager->Move(terminal->LayerID(), {100, 200});
  active_layer->Activate(terminal->LayerID());
  layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
  __asm__("sti");

  while (true) {
    __asm__("cli");
    auto msg = task.ReceiveMessage(); // main 関数の task_manager->SendMessage(task_terminal_id, *msg); から飛んでくる。
    if (!msg) {
      task.Sleep();
      __asm__("sti");
      continue;
    }

    __asm__("sti");

    switch (msg->type) {
    case Message::kTimerTimeout:
      {
        const auto area = terminal->BlinkCursor();
        Message msg = MakeLayerMessage(
          task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");
      }
      break;
    case Message::kKeyPush:
      {
        const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                             msg->arg.keyboard.keycode,
                                             msg->arg.keyboard.ascii);
        Message msg = MakeLayerMessage(
          task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");
      }
      break;
    default:
      break;
    }
  }
}
