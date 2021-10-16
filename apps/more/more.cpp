#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../syscall.h"

AppEvent WaitKey() {
  AppEvent events[1];
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      fprintf(stderr, "ReadEvent failed: %s\n", strerror(err));
      exit(1);
    }
    if (events[0].type == AppEvent::kQuit) {
      exit(0);
    }
    if (events[0].type == AppEvent::kKeyPush &&
        events[0].arg.keypush.press) {
      return events[0];
    }
  }
}

// more -3 [hoge.txt] のような入力を想定
extern "C" void main(int argc, char** argv) {
  int page_size = 10;
  int arg_file = 1; // argv におけるファイル名のインデックス
  if (argc >= 2 && argv[1][0] == '-' && isdigit(argv[1][1])) {
    page_size = atoi(&argv[1][1]);
    ++arg_file;
  }

  FILE* fp = stdin;
  if (argc > arg_file) {
    fp = fopen(argv[arg_file], "r");
    if (fp == nullptr) {
      fprintf(stderr, "failed to open '%s'\n", argv[arg_file]);
      exit(1);
    }
  }

  std::vector<std::string> lines{};
  char line[256];
  while (fgets(line, sizeof(line), fp)) {
    lines.emplace_back(line);
  }

  for (int i = 0; i < lines.size(); ++i) {
    // 表示件数を page_size 行を表示すると、一旦処理を止めて、キーボードからの入力を待つ。
    // 入力が WaitKey から return されると、さらに page_size 件のファイルの中身が表示される。
    if (i > 0 && (i % page_size) == 0) {
      fputs("---more---\n", stderr);
      WaitKey();
    }
    fputs(lines[i].c_str(), stdout);
  }
  exit(0);
}
