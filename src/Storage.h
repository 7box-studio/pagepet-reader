#pragma once

#include <BookStorage.h>
#include <SDCardManager.h>

#include <cstdint>

class SdBookSource final : public freeink::book::BookSource {
 public:
  bool open(SDCardManager& card, const char* path);
  void close();
  int32_t readAt(uint64_t offset, void* dst, uint32_t len) override;
  uint64_t size() const override { return length_; }

 private:
  FsFile file_;
  uint64_t length_ = 0;
};

class SdCache final : public freeink::book::CacheStorage {
 public:
  bool open(SDCardManager& card, uint32_t bookId);
  bool exists(const char* name) override;
  bool remove(const char* name) override;
  int64_t fileSize(const char* name) override;
  int32_t readAt(const char* name, uint32_t offset, void* dst, uint32_t len) override;
  bool beginWrite(const char* name) override;
  bool write(const void* data, uint32_t len) override;
  bool endWrite() override;
  int32_t readBackAt(uint32_t offset, void* dst, uint32_t len) override;

 private:
  bool pathFor(const char* name, char* out, size_t cap) const;
  SDCardManager* card_ = nullptr;
  char dir_[48]{};
  char final_[96]{};
  char temp_[96]{};
  FsFile output_;
};

uint32_t bookIdFor(const char* path);
bool writeAtomic(SDCardManager& card, const char* path, const void* data, size_t size);
