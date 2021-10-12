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
  uint32_t next = GetFAT()[cluster];
  if (IsEndOfClusterchain(next)) {
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

size_t LoadFile(void* buf, size_t len, DirectoryEntry& entry) {
  return FileDescriptor{entry}.Read(buf, len);
}

bool IsEndOfClusterchain(unsigned long cluster) {
  return cluster >= 0x0ffffff8ul;
}

size_t FileDescriptor::Load(void* buf, size_t len, size_t offset) {
  FileDescriptor fd{fat_entry_};
  fd.rd_off_ = offset;

  unsigned long cluster = fat_entry_.FirstCluster();
  while (offset >= bytes_per_cluster) {
    offset -= bytes_per_cluster;
    cluster = NextCluster(cluster);
  }

  fd.rd_cluster_ = cluster;
  fd.rd_cluster_off_ = offset;
  return fd.Read(buf, len);
}

// FAT データ構造 (32 ビットの配列) の先頭ポインタを得る関数
uint32_t* GetFAT() {
  uintptr_t fat_offset =
    boot_volume_image->reserved_sector_count *
    boot_volume_image->bytes_per_sector;
  return reinterpret_cast<uint32_t*>(
    reinterpret_cast<uintptr_t>(boot_volume_image) + fat_offset);
}

unsigned long ExtendCluster(unsigned long eoc_cluster, size_t n) {
  uint32_t* fat = GetFAT();
  while (!IsEndOfClusterchain(fat[eoc_cluster])) {
    eoc_cluster = fat[eoc_cluster];
  }

  size_t num_allocated = 0;
  auto current = eoc_cluster;
  for (unsigned long candidate = 2; num_allocated < n; ++candidate) {
    if (fat[candidate] != 0) {
      continue;
    }
    // 空きのクラスタが見つかる。
    // この時点では、fat[current] は、EOC になっているので、それを新しい確保できるクラスタ番号に変更する。
    fat[current] = candidate;
    current = candidate;
    ++num_allocated;
  }
  fat[current] = kEndOfClusterchain;
  return current;
}

// 引数で渡されたディレクトリエントリのから未使用のディレクトリエントリを探し、存在する時は、それを割り当てる。
// そうでない時は、クラスタチェーンを拡張する。
DirectoryEntry* AllocateEntry(unsigned long dir_cluster) {
  while (true) {
    auto dir = GetSectorByCluster<DirectoryEntry>(dir_cluster); // クラスタ番号をブロック位置へ変換する
    for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); ++i) {
      if (dir[i].name[0] == 0 || dir[i].name[0] == 0xe5) {
        return &dir[i];
      }
    }
    auto next = NextCluster(dir_cluster);
    if (next == kEndOfClusterchain) { // 空きエントリが無い
      break;
    }
    dir_cluster = next;
  }

  dir_cluster = ExtendCluster(dir_cluster, 1);
  auto dir = GetSectorByCluster<DirectoryEntry>(dir_cluster);
  memset(dir, 0, bytes_per_cluster);
  return &dir[0];
}

// ユーザから受け取ったファイル名をディレクトリエントリに書き込む
void SetFileName(DirectoryEntry& entry, const char* name) {
  const char* dot_pos = strrchr(name, '.');
  memset(entry.name, ' ', 8 + 3); // 半角スペースで初期化する。
  if (dot_pos) {
    for (int i = 0; i < 8 && i < dot_pos - name; ++i) {
      entry.name[i] = toupper(name[i]);
    }
    for (int i = 0; i < 3 && dot_pos[i + 1]; ++i) {
      entry.name[8 + i] = toupper(dot_pos[i + 1]);
    }
  } else {
    for (int i = 0; i < 8 && name[i]; ++i) {
      entry.name[i] = toupper(name[i]);
    }
  }
}

WithError<DirectoryEntry*> CreateFile(const char* path) {
  auto parent_dir_cluster = fat::boot_volume_image->root_cluster;
  const char* filename = path;

  // この if 文ではユーザが指定した path に含まれるディレクトリが適切な場合、そのクラスタ番号を取得する。
  // ユーザから受け取った path 変数をディレクトリ名とファイル名に分割する。
  if (const char* slash_pos = strrchr(path, '/')) {
    filename = &slash_pos[1];
    if (slash_pos[1] == '\0') { // ファイル名がない時
      return { nullptr, MAKE_ERROR(Error::kIsDirectory) };
    }

    // ファイル名が存在する時、そのファイル名のディレクトリを保持する。
    char parent_dir_name[slash_pos - path + 1];
    strncpy(parent_dir_name, path, slash_pos - path);
    parent_dir_name[slash_pos - path] = '\0';

    if (parent_dir_name[0] != '\0') {
      auto [ parent_dir, post_slash2 ] = fat::FindFile(parent_dir_name);
      if (parent_dir == nullptr) {
        return { nullptr, MAKE_ERROR(Error::kNoSuchEntry) };
      }
      parent_dir_cluster = parent_dir->FirstCluster();
    }
  }

  auto dir = fat::AllocateEntry(parent_dir_cluster);
  if (dir == nullptr) {
    return { nullptr, MAKE_ERROR(Error::kNoEnoughMemory) };
  }
  fat::SetFileName(*dir, filename);
  dir->file_size = 0;
  return { dir, MAKE_ERROR(Error::kSuccess) };
}

unsigned long AllocateClusterChain(size_t n) {
  uint32_t* fat = GetFAT();
  unsigned long first_cluster;
  for (first_cluster = 2; ; ++first_cluster) {
    if (fat[first_cluster] == 0) {
      fat[first_cluster] = kEndOfClusterchain;
      break;
    }
  }

  if (n > 1) {
    ExtendCluster(first_cluster, n - 1);
  }
  return first_cluster;
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

size_t FileDescriptor::Write(const void* buf, size_t len) {
  auto num_cluster = [](size_t bytes) {
    return (bytes + bytes_per_cluster - 1) / bytes_per_cluster;
  };

  // wr_cluster_ に適切な値を設定する。
  // 初めてファイルを書き込む時
  if (wr_cluster_ == 0) {
    if (fat_entry_.FirstCluster() != 0) { // 元からファイルの内容が存在する時
      wr_cluster_ = fat_entry_.FirstCluster();
    } else { // 新規作成されたばかりの空ファイルである時
      wr_cluster_ = AllocateClusterChain(num_cluster(len));
      fat_entry_.first_cluster_low = wr_cluster_ & 0xffff;
      fat_entry_.first_cluster_high = (wr_cluster_ >> 16) & 0xffff;
    }
  }

  const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf);

  size_t total = 0;
  while (total < len) {
    // 読み出すクラスタのオフセット (wr_cluster_off_) を補償する
    if (wr_cluster_off_ == bytes_per_cluster) {
      const auto next_cluster = NextCluster(wr_cluster_);
      if (next_cluster == kEndOfClusterchain) { // 書き込む領域を拡張する。
        wr_cluster_ = ExtendCluster(wr_cluster_, num_cluster(len - total));
      } else {
        wr_cluster_ = next_cluster;
      }
      wr_cluster_off_ = 0;
    }

    uint8_t* sec = GetSectorByCluster<uint8_t>(wr_cluster_);
    size_t n = std::min(len, bytes_per_cluster - wr_cluster_off_);
    memcpy(&sec[wr_cluster_off_], &buf8[total], n);
    total += n;

    wr_cluster_off_ += n;
  }

  wr_off_ += total;
  fat_entry_.file_size = wr_off_;
  return total;
}

}
