# 概要

- 自作 OS をするのログを残す。

## 開発環境のセットアップ

- この Ansible の箇所は飛ばした。ここを飛ばしても MikanOS は起動したので、問題ないと思われる。

```bash
TASK [check whether qemu-system-gui exists] *********************************************************************************************************************************
fatal: [localhost]: FAILED! => {"changed": true, "cmd": ["dpkg-query", "--show", "qemu-system-gui"], "delta": "0:00:00.034209", "end": "2021-08-14 00:53:29.920960", "msg": "non-zero return code", "rc": 1, "start": "2021-08-14 00:53:29.886751", "stderr": "dpkg-query: qemu-system-gui に一致するパッケージが見つかりません", "stderr_lines": ["dpkg-query: qemu-system-gui に一致するパッケージが見つかりません"], "stdout": "", "stdout_lines": []}
...ignoring
```

### 参考

- [mikanos-build](https://github.com/uchan-nos/mikanos-build)

## ch01

- `sum` コマンドを使用すると、ファイルのチェックサムを計算することができる。

```bash
clang -target x86_64-pc-win32-coff -mno-red-zone -fno-stack-protector -fshort-wchar -Wall -c hello.c
```

```bash
lld-link /subsystem:efi_application /entry:EfiMain /out:hello.efi hello.o
```

```bash
qemu-img create -f raw disk.img 200M
mkfs.fat -n 'HONOS' -s 2 -f 2 -R 32 -F 32 disk.img
mkdir -p mnt
sudo mount -o loop disk.img mnt
sudo mkdir -p mnt/EFI/BOOT
sudo cp BOOTX64.EFI mnt/EFI/BOOT/BOOTX64.EFI
sudo umount mnt

qemu-system-x86_64 \
-drive if=pflash,file=$HOME/osbook/devenv/OVMF_CODE.fd \
-drive if=pflash,file=$HOME/osbook/devenv/OVMF_VARS.fd \
-hda disk.img
```

## ch02

- QEMU を起動させるためのスクリプトを作成する。
- 以下のように `$1` などで引数を取ることができる。

```bash
(3.7.0) h-kiwata honOS [main]
> cat run_qemu.sh
(3.7.0) h-kiwata honOS [main]
> bash run_qemu.sh this is
Hello run_qemu.sh is
```

```bash
qemu-img create -f raw disk.img 200M
mkfs.fat -n 'HONOS' -s 2 -f 2 -R 32 -F 32 disk.img
mkdir -p mnt
sudo mount -o loop disk.img mnt
sudo mkdir -p mnt/EFI/BOOT
sudo cp /home/h-kiwata/edk2/Build/HonoLoaderX64/DEBUG_CLANG38/X64/Loader.efi mnt/EFI/BOOT/BOOTX64.EFI
sudo umount mnt

qemu-system-x86_64 \
-drive if=pflash,file=$HOME/osbook/devenv/OVMF_CODE.fd \
-drive if=pflash,file=$HOME/osbook/devenv/OVMF_VARS.fd \
-hda disk.img
```

- [UEFI の仕様書](https://uefi.org/specifications)

- `EFI_MEMORY_DESCRIPTOR` 構造体の定義を確認する。

```bash
> find ~/edk2/ | grep MdePkg/Include/Uefi/UefiSpec.h
/home/h-kiwata/edk2/MdePkg/Include/Uefi/UefiSpec.h
```

```c
///
/// Definition of an EFI memory descriptor.
///
typedef struct {
  ///
  /// Type of the memory region.
  /// Type EFI_MEMORY_TYPE is defined in the
  /// AllocatePages() function description.
  ///
  UINT32                Type;
  ///
  /// Physical address of the first byte in the memory region. PhysicalStart must be
  /// aligned on a 4 KiB boundary, and must not be above 0xfffffffffffff000. Type
  /// EFI_PHYSICAL_ADDRESS is defined in the AllocatePages() function description
  ///
  EFI_PHYSICAL_ADDRESS  PhysicalStart;
  ///
  /// Virtual address of the first byte in the memory region.
  /// VirtualStart must be aligned on a 4 KiB boundary,
  /// and must not be above 0xfffffffffffff000.
  ///
  EFI_VIRTUAL_ADDRESS   VirtualStart;
  ///
  /// NumberOfPagesNumber of 4 KiB pages in the memory region.
  /// NumberOfPages must not be 0, and must not be any value
  /// that would represent a memory page with a start address,
  /// either physical or virtual, above 0xfffffffffffff000.
  ///
  UINT64                NumberOfPages;
  ///
  /// Attributes of the memory region that describe the bit mask of capabilities
  /// for that memory region, and not necessarily the current settings for that
  /// memory region.
  ///
  UINT64                Attribute;
} EFI_MEMORY_DESCRIPTOR;
```

- メモリマップを確認する。

```bash
sudo mount -o loop disk.img mnt
ls mnt
cat mnt/memmap
sudo umount mnt
```

- edk2 のセットアップ

```bash
> source edksetup.sh
Loading previous configuration from /home/h-kiwata/edk2/Conf/BuildEnv.sh
Using EDK2 in-source Basetools
WORKSPACE: /home/h-kiwata/edk2
EDK_TOOLS_PATH: /home/h-kiwata/edk2/BaseTools
CONF_PATH: /home/h-kiwata/edk2/Conf
```

## ch03

- Kernel のコンパイルとオブジェクトファイルのリンク

```bash
clang++ -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17 -c main.cpp

ld.lld --entry KernelMain -z norelro --image-base 0x100000 --static -o kernel.elf main.o
```

- `gEfiFileInfoGuid` を使う際には、`Loader.inf` の `Guids` にも追加で記述する必要がある。

- day03 にして公開されている `run_qemu.sh` を使用することにした。

```bash
$HOME/osbook/devenv/run_qemu.sh $HOME/edk2/Build/HonoLoaderX64/DEBUG_CLANG38/X64/Loader.efi $HOME/honOS/kernel/kernel.elf
```

- `kernel.elf` が kernel ディレクトリにある C++ のプログラムをコンパイルしてリンクした結果の実行ファイルである。また、`Loader.efi` は、edk2 で build コマンドを実行した際に生成されるファイルである。

- Clang の環境変数の設定

```bash

```

## 環境のセットアップとビルドコマンド

- EDK2 のセットアップ

```bash
source edksetup.sh
```

- ビルド (`$HOME/edk2` のディレクトリで実行)

```bash
build
```

- Clang の環境変数の設定
- day05 で Newlib の `sprintf` を実装する際にも再度実行する必要がある。

```bash
source /home/h-kiwata/osbook/devenv/buildenv.sh
```

- Kernel のコンパイルとリンク (honOS/kernel ディレクトリで実行する。)

```bash
# 前
clang++ $CPPFLAGS -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17 -c main.cpp

ld.lld --entry KernelMain -z norelro --image-base 0x100000 --static -o kernel.elf main.o

# 後
clang++ $CPPFLAGS -O2 -Wall -g --target=x86_64-elf -fno-exceptions -ffreestanding -c main.cpp

ld.lld $LDFLAGS --entry KernelMain -z norelro --image-base 0x100000 --static -o kernel.elf main.o
```

- QEMU の起動

```bash
$HOME/osbook/devenv/run_qemu.sh $HOME/edk2/Build/HonoLoaderX64/DEBUG_CLANG38/X64/Loader.efi $HOME/honOS/kernel/kernel.elf
```

# day04

- [C言語のインクルードガードはpragma onceを使う](https://kaworu.jpn.org/c/C%E8%A8%80%E8%AA%9E%E3%81%AE%E3%82%A4%E3%83%B3%E3%82%AF%E3%83%AB%E3%83%BC%E3%83%89%E3%82%AC%E3%83%BC%E3%83%89%E3%81%AFpragma_once%E3%82%92%E4%BD%BF%E3%81%86)

- シンボリックリンクを貼るのに手こずった。`/home/h-kiwata/honOS/HonoLoaderPkg/Main.c` から `/home/h-kiwata/honOS/kernelframe_buffer_config.hpp` をインクルードする必要があった。したがって、`/home/h-kiwata/honOS/HonoLoaderPkg/` のディレクトリで以下のコマンドを実行する。

```bash
ln -s ../kernel/frame_buffer_config.hpp .
```

- [シンボリックリンクの作成と削除](https://qiita.com/colorrabbit/items/2e99304bd92201261c60)

- [C++ autoの使いどころ・使わない方が良い場面](https://uchan.hateblo.jp/entry/2019/02/22/105622)

- `HonoLoaderPkg` のディレクトリで `kernel/elf.hpp` のシンボリックリンクを張る。

```bash
ln -s ../kernel/elf.hpp .
```

- `elf.hpp` で定義される `Elf64_Ehdr` は、`man elf` でマニュアルを確認しながら実装すると良い。

# day05

- フォントは以下から取得する。
  - [東雲 ビットマップフォントファミリー](http://openlab.ring.gr.jp/efont/shinonome/)
- 取得した hankaku.txt から hankaku.bin を作成する。

```bash
python ../tools/makefont.py -o hankaku.bin hankaku.txt
```

- hankaku.bin からオブジェクトファイル hankaku.o を作成する。

```bash
objcopy -I binary -O elf64-x86-64 -B i386:x86-64 hankaku.bin hankaku.o
```

- Clang の環境変数の設定
- day05 で Newlib の `sprintf` を実装する際にも再度実行する必要がある。
- 忘れやすいので注意をする。

```bash
source /home/h-kiwata/osbook/devenv/buildenv.sh
```

- `Console クラス` を実装したので、`配置 new` でグローバル変数 `console` を宣言するように実装を変更した。

```bash
Console console{*pixel_writer, {0, 0, 0}, {255, 255, 255}};
console = new(console)Console{*pixel_writer, {0, 0, 0}, {255, 255, 255}};
```

- make コマンドを実行して以下のエラーが生じる時は、`source /home/h-kiwata/osbook/devenv/buildenv.sh` を実行するのを忘れている。

```bash
clang++  -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17 -c main.cpp
main.cpp:1:10: fatal error: 'cstdint' file not found
#include <cstdint>
         ^~~~~~~~~
1 error generated.
Makefile:22: recipe for target 'main.o' failed
make: *** [main.o] Error 1
```

- ポインタと参照のプログラムの文法の違いに関して

- `graphics.cpp` の中で `Write` 関数を呼び出す時

```c
void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& color) {
    for (int dx = 0; dx < size.x; ++dx) {
      writer.Write(pos.x + dx, pos.y, color);
      writer.Write(pos.x + dx, pos.y + size.y - 1, color);
    }
    for (int dy = 0; dy < size.y; ++dy) {
      writer.Write(pos.x, pos.y + dy, color);
      writer.Write(pos.x + size.x - 1, pos.y + dy, color);
    }
}
```

- `main.cpp` の中で呼び出す時

```c
pixel_writer->Write(200 + dx, 100 + dy, {255, 255, 255});
```

# day06

- C 言語からアセンブリでリンクされて関数を呼び出すのに参考になる。
  - [前節のプログラムを修正して 32 bit 用でコンパイルして実行してみた](https://github.com/dilmnqvovpnmlib/LowLevelProgramming/tree/main/log/20210508#%E5%89%8D%E7%AF%80%E3%81%AE%E3%83%97%E3%83%AD%E3%82%B0%E3%83%A9%E3%83%A0%E3%82%92%E4%BF%AE%E6%AD%A3%E3%81%97%E3%81%A6-32-bit-%E7%94%A8%E3%81%A7%E3%82%B3%E3%83%B3%E3%83%91%E3%82%A4%E3%83%AB%E3%81%97%E3%81%A6%E5%AE%9F%E8%A1%8C%E3%81%97%E3%81%A6%E3%81%BF%E3%81%9F)

- C++ で `std::array` で作成した配列のサイズを計算する。
  - [配列データを格納してサイズを計算するには std::array コンテナを使用する](https://www.delftstack.com/ja/howto/cpp/how-to-find-length-of-an-array-in-cpp/#%E9%85%8D%E5%88%97%E3%83%87%E3%83%BC%E3%82%BF%E3%82%92%E6%A0%BC%E7%B4%8D%E3%81%97%E3%81%A6%E3%82%B5%E3%82%A4%E3%82%BA%E3%82%92%E8%A8%88%E7%AE%97%E3%81%99%E3%82%8B%E3%81%AB%E3%81%AF-std-array-%E3%82%B3%E3%83%B3%E3%83%86%E3%83%8A%E3%82%92%E4%BD%BF%E7%94%A8%E3%81%99%E3%82%8B)
- 変数におけるconstexprは、#define などで作っていたようなコンパイル時定数を作るためのキーワードである。
  - [C++、constexprのまとめ](https://qiita.com/KRiver1/items/ef7731467b5ca83850cb#%E5%A4%89%E6%95%B0%E3%81%AEconstexpr)
- メンバ関数の宣言に const キーワードを指定するとそのメンバ関数はオブジェクトの持つメンバ変数を変更できなくなります。
  - [オブジェクトの変更拒否](http://wisdom.sakura.ne.jp/programming/cpp/cpp42.html)
- [明示的な型変換演算子のオーバーロード](https://cpprefjp.github.io/lang/cpp11/explicit_conversion_operator.html)
- [C++：列挙型enum](http://tomame0se.blog.shinobi.jp/c--/c--%EF%BC%9A%E5%88%97%E6%8C%99%E5%9E%8Benum)

- day06c のポーリングでマウス入力の節でリスト 6.18 のプログラムを実装して実行すると、思ったような挙動をしなかった。そこで、6.4 で紹介されている Log 関数を実装して、ScannAll 関数を起点に printk デバッグを行った。すると、`error.h` の以下の箇所が間違っていることに気づけた。

- 修正前

```c
operator bool() const {
  return this->code_ == kSuccess;
}
```

- 修正後

```c
operator bool() const {
  return this->code_ != kSuccess;
}
```

- 本来通るはずのない箇所に処理が行っていたので、トラブルシューティングを成功させることができた。以下のプログラムの if 文の中を本来は通らないのだが、上述の間違いにより `kSuccess` の時に中を通ってしまっていた。

```c
Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function) {
  auto class_code = ReadClassCode(bus, device, function);
  auto header_type = ReadHeaderType(bus, device, function);
  Device dev{bus, device, function, header_type, class_code};
  if (auto err = AddDevice(dev)) {
    return err;
  }
...
}
```

# day07

- 割り込みの機能を実装する。実装の方針は以下である。
- 事前準備
  - イベント発生時に実行する割り込みハンドラを準備する。
  - 割り込みハンドラを割り込み記述子テーブル IDT に登録する。
- イベント発生時
  - ハードウェアがイベントを CPU に通知する。
  - CPU は現在の処理を中断して、イベントの種類に応じて登録された割り込みハンドラに処理を移す。
  - 割り込みハンドラの処理が終わると、中断していた処理を再開する。

- [Linuxでアセンブラ](https://zenn.dev/inc/scraps/7321a9fe6b1be6)

- C++ で . と -> の使い分けが全くわからん。

# day08

- UEFI 側に `memory_map.hpp` のシンボリックリンクを貼る。

```bash
ln -s ../kernel/memory_map.hpp .
```

- UEFI から渡されたメモリマップを参照して空いているメモリ領域を探し出し、メモリ割り当て要求に応じて必要なメモリを領域を払い出すのが目的である。
- [第5回 APICと割り込み/第6回 メモリ管理](https://www.youtube.com/watch?v=sZbUQouuYho&t=6303s)

- tag 名で branch を移動する。
  - [gitでタグをチェックアウトする](https://h2ham.net/git-tag-checkout/)

```bash
git checkout refs/tags/<tag 名>
```

- `::func()` に関する解説が書かれている。いわゆる名前空間のアクセス方法に関して解説されている。
  - [名前空間 | Programming Place Plus　Modern C++編【言語解説】　第４章](https://programming-place.net/ppp/contents/modern_cpp/language/004.html)

- `explicit` は、コンパイラが暗黙の型変換をしないための修飾子である。

- `constexpr` は、コンパイル時に値が決定する  定数などを定義できる。C 言語で言うところのマクロとかかな？
  - [constexpr](https://cpprefjp.github.io/lang/cpp11/constexpr.html)

- `reinterpret_cast` は、あるポインタ型の変数を他のポインタ型の変数に強制的に変換するための関数である。
  - [reinterpret_cast](https://www.yunabe.jp/docs/cpp_casts.html#reinterpret_cast)

- `alignas` は、コンパイラがキリの良い値でアライメントするように命令するための修飾子である。
  - [alignas](https://cpprefjp.github.io/lang/cpp11/alignas.html)

- `git add -N <ファイル名>` をすると、ファイルの差分を確認することができる。
  - [新規にファイルを作成した後で git add -N (–intent-to-add) した際の git diff の挙動](https://reboooot.net/post/how-to-check-changes-with-git/)

- C や C++ での `__FILE__` や `__LINE__` は、ファイル名や行数のデバッグ情報が含まれる。
  - [C言語入門：__FILE__](https://www.geekpage.jp/programming/c/__file__.php)

- [【C++】無名名前空間とは【目的と用途、活用例】](https://marycore.jp/prog/cpp/unnamed-namespace/#%E7%84%A1%E5%90%8D%E5%90%8D%E5%89%8D%E7%A9%BA%E9%96%93%E3%81%AE%E7%89%B9%E5%BE%B4%E3%81%A8%E7%9B%AE%E7%9A%84)

# day09

- [Newlibc の sbrk のドキュメント](https://sourceware.org/newlib/libc.html#index-sbrk)

- optional 型の値は value() で取り出すことができる。
  - [std::optional](https://cpprefjp.github.io/reference/optional/optional.html)

- std::vector::emplace_back は、vector の末尾に要素を追加する。C++ 17 以降での返り値は構築した要素への参照が返る。
  - [std::vector::emplace_back](https://cpprefjp.github.io/reference/vector/vector/emplace_back.html)

- std::find_if の返り値は、対象となるイテレータが存在しない時は [first, last] の last のイテレータを返す。
  - [std::find_if](https://cpprefjp.github.io/reference/algorithm/find_if.html)

- std::find  の返り値は、対象となるイテレータが存在しない時は [first, last] の last のイテレータを返す。
  - [std::find](https://cpprefjp.github.io/reference/algorithm/find.html)

- std::vector::end() は、末尾の次を指すイテレータを取得する。例えば文字列なら Null が返る？
  - [std::vector::end](https://cpprefjp.github.io/reference/vector/vector/end.html)

- global 変数で定義している `mouse_layer_id` に `auto` を付けて代入すると、マウスが動かずバグった。
- 現時点での実装だと、かなりマウスがチラつく。また、マウスを上下に移動させると、移動させた分だけ戻さないと再度マウスが表示されない。そして、左右にマウスを移動させると、反対側からマウスが現れるバグがある。

- `std::vector::data` は、配列の先頭へのポインタを返す。
  - [std::vector::data](https://cpprefjp.github.io/reference/vector/vector/data.html)

# day10

- 描画範囲を限定して再描画を行うようにして、チラツキを解消する。
  - `LayerManager::Draw` に描画領域を指定するロジックを追加する。

## day10d

- 背景を描いてからウィンドウを描いているので、ウィンドウが表示されていない時間帯が存在する。
- そこで、描画範囲を制限し、ウィンドウだけを再描画するようにリファクタリングすれば、ウィンドウが常に画面に描画され、チラツキが解消される。

## day10e

- マウスをカウンタの上に重ねると、チラつくのは、カウンタを描画した後にマウスを描いているので、マウスが表示されていない期間が存在するからである。そこで、このチラツキを解消するためにダブルバッファリングという仕組みを活用する。
  - [【OpenGLでゲームを作る】ダブルバッファリングとは](https://nn-hokuson.hatenablog.com/entry/2014/01/15/164232)

# day11

## osbook_day11a

- リファクタする全体像を把握し、一通り diff のコードに目を通す。
- main 関数と切り出す処理を順次リファクタしていく。

1. graphics.hpp and graphics.cpp
2. console.hpp and console.cpp
3. segment.hpp and segment.cpp
4. paging.hpp and paging.cpp
5. memory_manager.hpp and memory_manager.cpp
6. message.hpp
7. interrupt.hpp and interrupt.cpp
8. xhci.hpp and xhci.cpp の更新と main 関数から削除
9. pci.hpp and pci.cpp
10. layer.hpp and layer.cpp
11. window.hpp and window.cpp
12. mosue.hpp and mosue.cpp


- 本日のドライバを変更しても修正が反映されない問題が生じた。これは Makefile を `osbook_day11a` の tag のブランチのものに書き換えると変更が反映された。おそらく、ドライバを修正してからビルドが反映されていなかった？

# osbook_day17a

- 以下のファイル `test.sh` を用意する。

```bash
echo Start
echo $0
echo $1
echo End
```

- `bash test.sh hoge` を実行すると、その結果は以下のようになる。

```bash
Start
test.sh
hoge
End
```

# osbook_day18b

- 以下の内容を記述した `test.sh` を用意する。

```bash
echo ${1:-}
```

- このファイルを以下のように実行すると、次のような結果が得られる。

```bash
> bash test.sh

> bash test.sh hello
hello
```

# osbook_day18c

- `rpn.cpp` を実行した結果

```bash
> clang++ rpn.cpp
> ./a.out
> echo $?
0
> ./a.out 1 2 +
> echo $?
3
```

- `std::vector::push_back` は新たな要素を末尾に追加する。
  - [std::vector::push_back](https://cpprefjp.github.io/reference/vector/vector/push_back.html)

# osbook_day20b

- `SetupSegments` は、`GDT` を再構築するための関数である。

# osbook_day26a

- 特定のタグに移動する。

```bash
git checkout refs/tags/osbook_day26a
```
