#include "fat.hpp"

#include <algorithm>
#include <cstring>
#include <cctype>
#include <utility>

namespace {

std::pair<const char*, bool>
NextPathElement(const char* path, char* path_elem) { // path_elem には現在の一番上のディレクトリを書きこむ。
  const char* next_slash = strchr(path, '/'); // next_slash は / を指している。
  // / がなく、末尾である時
  if (next_slash == nullptr) {
    strcpy(path_elem, path);
    return { nullptr, false };
  }

  const auto elem_len = next_slash - path;
  strncpy(path_elem, path, elem_len);
  path_elem[elem_len] = '\0';
  return { &next_slash[1], true };
}

}

namespace fat {

BPB* boot_volume_image;
unsigned long bytes_per_cluster;

void Initialize(void* volume_image) {
  boot_volume_image = reinterpret_cast<fat::BPB*>(volume_image);
  bytes_per_cluster =
    static_cast<unsigned long>(boot_volume_image->bytes_per_sector) *
    boot_volume_image->sectors_per_cluster;
}

// クラスタ番号をブロック位置へ変換する
uintptr_t GetClusterAddr(unsigned long cluster) {
  unsigned long sector_num =
    boot_volume_image->reserved_sector_count +
    boot_volume_image->num_fats * boot_volume_image->fat_size_32 +
    (cluster - 2) * boot_volume_image->sectors_per_cluster; // クラスタ番号からブロック数を計算
  uintptr_t offset = sector_num * boot_volume_image->bytes_per_sector;
  return reinterpret_cast<uintptr_t>(boot_volume_image) + offset;
}

void ReadName(const DirectoryEntry& entry, char* base, char* ext) {
  memcpy(base, &entry.name[0], 8);
  base[8] = 0;
  for (int i = 7; i >= 0 && base[i] == 0x20; --i) {
    base[i] = 0;
  }

  memcpy(ext, &entry.name[8], 3);
  ext[3] = 0;
  for (int i = 2; i >= 0 && ext[i] == 0x20; --i) {
    ext[i] = 0;
  }
}

// ディレクトリエントリのオブジェクト dir[i] をもとに呼び出し元の変数 dest に書き込む。
void FormatName(const DirectoryEntry& entry, char* dest) {
  char ext[5] = ".";
  ReadName(entry, dest, &ext[1]);
  if (ext[1]) {
    strcat(dest, ext);
  }
}

unsigned long NextCluster(unsigned long cluster) {
  uintptr_t fat_offset =
    boot_volume_image->reserved_sector_count *
    boot_volume_image->bytes_per_sector;
  uint32_t* fat = reinterpret_cast<uint32_t*>(
    reinterpret_cast<uintptr_t>(boot_volume_image) + fat_offset);
  uint32_t next = fat[cluster];
  if (next >= 0x0ffffff8ul) {
    return kEndOfClusterchain;
  }
  return next;
}

std::pair<DirectoryEntry*, bool>
FindFile(const char* path, unsigned long directory_cluster) {
  if (path[0] == '/') {
    directory_cluster = boot_volume_image->root_cluster;
    ++path;
  } else if (directory_cluster == 0) {
    directory_cluster = boot_volume_image->root_cluster;
  }

  char path_elem[13]; // 現在のパス
  const auto [ next_path, post_slash ] = NextPathElement(path, path_elem);
  const bool path_last = next_path == nullptr || next_path[0] == '\0';

  while (directory_cluster != kEndOfClusterchain) {
    auto dir = GetSectorByCluster<DirectoryEntry>(directory_cluster);
    for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); ++i) {
      // cat コマンドの第一引数から受け取ったファイル名とディレクトリエントリの名前を比較する
      // DirectoryEntry の構造体の先頭に name があるから、以下のような比較が可能？
      if (dir[i].name[0] == 0x00) {
        goto not_found;
      } else if (!NameIsEqual(dir[i], path_elem)) {
        continue;
      }

      if (dir[i].attr == Attribute::kDirectory && !path_last) {
        return FindFile(next_path, dir[i].FirstCluster());
      } else {
        return { &dir[i], post_slash };
      }
    }

    directory_cluster = NextCluster(directory_cluster);
  }

not_found:
  return { nullptr, post_slash };
}

// cat コマンドで受け取った引数 name をディレクトリエントリの形式 name83 に変換し、比較する。
bool NameIsEqual(const DirectoryEntry& entry, const char* name) {
  unsigned char name83[11];
  memset(name83, 0x20, sizeof(name83));

  int i = 0;
  int i83 = 0;
  for (; name[i] != 0 && i83 < sizeof(name83); ++i, ++i83) {
    if (name[i] == '.') {
      i83 = 7;
      continue;
    }
    name83[i83] = toupper(name[i]);
  }

  return memcmp(entry.name, name83, sizeof(name83)) == 0;
}

size_t LoadFile(void* buf, size_t len, const DirectoryEntry& entry) {
  auto is_valid_cluster = [](uint32_t c) {
    return c != 0 && c != fat::kEndOfClusterchain;
  };
  auto cluster = entry.FirstCluster();

  const auto buf_uint8 = reinterpret_cast<uint8_t*>(buf);
  const auto buf_end = buf_uint8 + len;
  auto p = buf_uint8;

  while (is_valid_cluster(cluster)) {
    if (bytes_per_cluster >= buf_end - p) {
      memcpy(p, GetSectorByCluster<uint8_t>(cluster), buf_end - p);
      return len;
    }
    memcpy(p, GetSectorByCluster<uint8_t>(cluster), bytes_per_cluster);
    p += bytes_per_cluster;
    cluster = NextCluster(cluster);
  }
  return p - buf_uint8;
}

FileDescriptor::FileDescriptor(DirectoryEntry& fat_entry)
    : fat_entry_{fat_entry} {}

// newlib_support.c の read 関数から呼び出されている。
// read 関数でちまちま呼び出す際に、len で指定した長さの文字列読み出す。
// そして、その際にそのエントリのどこまで読み出したかの情報を保持する必要がある。
size_t FileDescriptor::Read(void* buf, size_t len) {
  // rd_off_ : ファイル先頭からの論理的な読み込みオフセット (バイト単位)
  // rd_cluster_ : rd_off_ が指す位置に対応するクラスタ番号
  // rd_cluster_off_ : rd_off_ が指すクラスタ番号のクラスタの先頭からのオフセット (バイト単位)
  if (rd_cluster_ == 0) {
    rd_cluster_ = fat_entry_.FirstCluster();
  }
  uint8_t* buf8 = reinterpret_cast<uint8_t*>(buf);
  len = std::min(len, fat_entry_.file_size - rd_off_); // この長さの文字列を読み出す。

  size_t total = 0; // 今までに書き込んだバイト数
  while (total < len) { // 以下は cat コマンドの実装が参考になる。
    uint8_t* sec = GetSectorByCluster<uint8_t>(rd_cluster_);
    size_t n = std::min(len - total, bytes_per_cluster - rd_cluster_off_); // buf に書き込むためのこのセクタの文字列の長さを調べる。
    memcpy(&buf8[total], &sec[rd_cluster_off_], n);
    total += n;

    // クラスタチェーンに従って次のクラスタを参照できるようにする。
    rd_cluster_off_ += n;
    // 読み出しているセクタの末尾まで行くと以下の if に処理が移る。
    // そうでない時は、再び while 文の中に処理にが戻る。
    if (rd_cluster_off_ == bytes_per_cluster) {
      rd_cluster_ = NextCluster(rd_cluster_);
      rd_cluster_off_ = 0;
    }
  }

  rd_off_ += total;
  return total;
}

}
