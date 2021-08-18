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
