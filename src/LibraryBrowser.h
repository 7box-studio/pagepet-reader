#pragma once

#include <SDCardManager.h>
#include <cstddef>
#include <cstdint>

// Only one screenful is resident. Never recurse through the SD tree or build
// an in-memory list proportional to the number of books.
class LibraryBrowser {
 public:
  static constexpr size_t PAGE_SIZE = 8;
  static constexpr size_t PATH_BYTES = 128;
  enum class Status : uint8_t { Ok, IoError, PathTooLong };
  struct Entry {
    char name[PATH_BYTES]{};
    bool directory = false;
    bool tooLong = false;
  };

  void reset();
  Status refresh(SDCardManager& card);
  Status move(SDCardManager& card, bool forward);
  Status enter(SDCardManager& card);
  Status parent(SDCardManager& card);
  Status focusBook(SDCardManager& card, const char* absolutePath);
  Status selectedPath(char* out, size_t cap) const;
  const Entry* selectedEntry() const;
  const Entry* row(size_t index) const { return index < rows_ ? &entries_[index] : nullptr; }
  const char* path() const { return path_; }
  bool atRoot() const { return path_[0] == '/' && path_[1] == '\0'; }
  uint32_t count() const { return count_; }
  uint32_t selection() const { return selection_; }
  uint32_t first() const { return first_; }
  size_t rows() const { return rows_; }

 private:
  Status scan(SDCardManager& card, uint32_t first, const char* focus = nullptr,
              uint32_t* found = nullptr);
  Status focusName(SDCardManager& card, const char* name);
  Status loadSelection(SDCardManager& card);
  Entry entries_[PAGE_SIZE]{};
  char path_[PATH_BYTES] = "/";
  uint32_t count_ = 0;
  uint32_t selection_ = 0;
  uint32_t first_ = 0;
  size_t rows_ = 0;
};
