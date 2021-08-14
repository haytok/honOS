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
