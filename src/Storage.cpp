#include "Storage.h"

#include <cstdio>
#include <cstring>
#include <limits>

uint32_t bookIdFor(const char* path) {
  uint32_t hash = 2166136261u;
  for (const auto* p = reinterpret_cast<const uint8_t*>(path); *p; ++p) hash = (hash ^ *p) * 16777619u;
  return hash;
}

bool SdBookSource::open(SDCardManager& card, const char* path) {
  close();
  file_ = card.open(path, O_RDONLY);
  if (!file_) return false;
  length_ = file_.size();
  return true;
}

void SdBookSource::close() {
  if (file_) file_.close();
  length_ = 0;
}

int32_t SdBookSource::readAt(uint64_t offset, void* dst, uint32_t len) {
  if (!file_ || offset > std::numeric_limits<uint32_t>::max() || len > INT32_MAX || !file_.seekSet(offset)) return -1;
  return file_.read(dst, len);
}

bool SdCache::open(SDCardManager& card, uint32_t bookId) {
  card_ = &card;
  if (!card.mkdir("/.pagepet", true) && !card.exists("/.pagepet")) return false;
  if (!card.mkdir("/.pagepet/cache", true) && !card.exists("/.pagepet/cache")) return false;
  snprintf(dir_, sizeof(dir_), "/.pagepet/cache/%08lx", static_cast<unsigned long>(bookId));
  return card.mkdir(dir_, true) || card.exists(dir_);
}

bool SdCache::pathFor(const char* name, char* out, size_t cap) const {
  if (!card_ || !name || strchr(name, '/') || strchr(name, '\\') || strlen(name) > 32) return false;
  const int n = snprintf(out, cap, "%s/%s", dir_, name);
  return n > 0 && static_cast<size_t>(n) < cap;
}

bool SdCache::exists(const char* name) {
  char path[96];
  return pathFor(name, path, sizeof(path)) && card_->exists(path);
}

bool SdCache::remove(const char* name) {
  char path[96];
  return pathFor(name, path, sizeof(path)) && card_->remove(path);
}

int64_t SdCache::fileSize(const char* name) {
  char path[96];
  if (!pathFor(name, path, sizeof(path))) return -1;
  FsFile file = card_->open(path, O_RDONLY);
  return file ? static_cast<int64_t>(file.size()) : -1;
}

int32_t SdCache::readAt(const char* name, uint32_t offset, void* dst, uint32_t len) {
  char path[96];
  if (!pathFor(name, path, sizeof(path)) || len > INT32_MAX) return -1;
  FsFile file = card_->open(path, O_RDONLY);
  if (!file || !file.seekSet(offset)) return -1;
  return file.read(dst, len);
}

bool SdCache::beginWrite(const char* name) {
  if (output_) output_.close();
  if (!pathFor(name, final_, sizeof(final_))) return false;
  const int n = snprintf(temp_, sizeof(temp_), "%s.part", final_);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(temp_)) return false;
  if (card_->exists(temp_)) card_->remove(temp_);
  output_ = card_->open(temp_, O_RDWR | O_CREAT | O_TRUNC);
  return static_cast<bool>(output_);
}

bool SdCache::write(const void* data, uint32_t len) {
  return output_ && output_.write(static_cast<const uint8_t*>(data), len) == len;
}

int32_t SdCache::readBackAt(uint32_t offset, void* dst, uint32_t len) {
  if (!output_ || len > INT32_MAX) return -1;
  const uint32_t cursor = output_.curPosition();
  if (!output_.seekSet(offset)) return -1;
  const int32_t got = output_.read(dst, len);
  if (!output_.seekSet(cursor)) return -1;
  return got;
}

bool SdCache::endWrite() {
  if (!output_) return false;
  const bool synced = output_.sync();
  output_.close();
  if (!synced) return false;
  if (card_->exists(final_) && !card_->remove(final_)) return false;
  return card_->rename(temp_, final_);
}

bool writeAtomic(SDCardManager& card, const char* path, const void* data, size_t size) {
  char temp[96];
  char backup[96];
  const int n = snprintf(temp, sizeof(temp), "%s.part", path);
  const int b = snprintf(backup, sizeof(backup), "%s.bak", path);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(temp) || b <= 0 ||
      static_cast<size_t>(b) >= sizeof(backup)) return false;
  if (card.exists(temp)) card.remove(temp);
  FsFile file = card.open(temp, O_RDWR | O_CREAT | O_TRUNC);
  if (!file) return false;
  const bool okay = file.write(static_cast<const uint8_t*>(data), size) == size && file.sync();
  file.close();
  if (!okay) return false;
  const bool hadPrevious = card.exists(path);
  if (hadPrevious) {
    if (card.exists(backup) && !card.remove(backup)) return false;
    if (!card.rename(path, backup)) return false;
  }
  if (!card.rename(temp, path)) {
    if (hadPrevious) card.rename(backup, path);
    return false;
  }
  if (card.exists(backup)) card.remove(backup);
  return true;
}
