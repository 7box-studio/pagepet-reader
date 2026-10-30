#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <fcntl.h>
#include <utility>
#include <vector>
#include <algorithm>
#include <cstring>

namespace hostsd {
inline std::filesystem::path root;
inline int64_t writeBudget = -1;
inline bool online = true;
inline uint32_t sourceReads = 0;
inline uint32_t fileOpens = 0;
}
class FsFile {
 public:
  FsFile() = default;
  explicit FsFile(const std::filesystem::path& path, int flags) {
    if (!hostsd::online) return;
    path_ = path;
    std::error_code error;
    directory_ = std::filesystem::is_directory(path, error);
    if (directory_) {
      for (std::filesystem::directory_iterator i(path, error), end; !error && i != end; i.increment(error)) {
        children_.push_back(i->path());
      }
      std::sort(children_.begin(), children_.end());
      return;
    }
    const bool truncate = flags & O_TRUNC;
    const bool writable = flags & (O_WRONLY | O_RDWR);
    file_.reset(fopen(path.c_str(), truncate ? "wb+" : writable ? "rb+" : "rb"));
    source_ = path.extension() == ".epub" || path.extension() == ".EPUB" || path.extension() == ".txt";
    ++hostsd::fileOpens;
  }
  explicit operator bool() const { return directory_ || file_ != nullptr; }
  void close() { file_.reset(); directory_ = false; children_.clear(); }
  bool isDirectory() const { return directory_; }
  uint8_t getError() const { return hostsd::online ? 0 : 1; }
  void rewindDirectory() { next_ = 0; }
  FsFile openNextFile() {
    return directory_ && next_ < children_.size() ? FsFile(children_[next_++], O_RDONLY) : FsFile{};
  }
  bool getName(char* out, size_t cap) const {
    const auto name = path_.filename().string();
    if (name.size() >= cap) return false;
    memcpy(out, name.c_str(), name.size() + 1);
    return true;
  }
  uint64_t size() {
    if (!file_) return 0;
    const long cursor = ftell(file_.get());
    fseek(file_.get(), 0, SEEK_END);
    const long size = ftell(file_.get());
    fseek(file_.get(), cursor, SEEK_SET);
    return size;
  }
  bool getModifyDateTime(uint16_t* date, uint16_t* time) { *date = 0; *time = 0; return true; }
  bool seekSet(uint64_t offset) { return hostsd::online && file_ && fseek(file_.get(), offset, SEEK_SET) == 0; }
  uint32_t curPosition() { return ftell(file_.get()); }
  int32_t read(void* data, uint32_t size) {
    if (!hostsd::online || !file_) return -1;
    if (source_) ++hostsd::sourceReads;
    return fread(data, 1, size, file_.get());
  }
  size_t write(const uint8_t* data, uint32_t size) {
    if (!hostsd::online || !file_) return 0;
    uint32_t want = size;
    if (hostsd::writeBudget >= 0) {
      if (static_cast<uint64_t>(hostsd::writeBudget) < want) want = hostsd::writeBudget;
      hostsd::writeBudget -= want;
    }
    return fwrite(data, 1, want, file_.get());
  }
  bool sync() { return hostsd::online && file_ && fflush(file_.get()) == 0; }
 private:
  struct Close { void operator()(FILE* f) const { fclose(f); } };
  std::unique_ptr<FILE, Close> file_;
  bool source_ = false;
  bool directory_ = false;
  std::filesystem::path path_;
  std::vector<std::filesystem::path> children_;
  size_t next_ = 0;
};
class SDCardManager {
 public:
  static SDCardManager& getInstance() { static SDCardManager card; return card; }
  bool begin() { return hostsd::online && std::filesystem::is_directory(hostsd::root); }
  void shutdown() {}
  FsFile open(const char* path, int flags) { return FsFile(resolve(path), flags); }
  bool exists(const char* path) { return hostsd::online && std::filesystem::exists(resolve(path)); }
  bool mkdir(const char* path, bool) { return hostsd::online && std::filesystem::create_directories(resolve(path)); }
  bool remove(const char* path) { return hostsd::online && std::filesystem::remove(resolve(path)); }
  bool rename(const char* from, const char* to) {
    if (!hostsd::online) return false;
    std::error_code error;
    std::filesystem::rename(resolve(from), resolve(to), error);
    return !error;
  }
 private:
  std::filesystem::path resolve(const char* path) { return hostsd::root / (path[0] == '/' ? path + 1 : path); }
};
