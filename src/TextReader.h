#pragma once

#include "Storage.h"

#include <FreeInkBook.h>
#include <FreeInkUIBookFont.h>
#include <FreeInkDisplay.h>
#include <ReaderFont.h>
#include <ReaderSmallFont.h>
#include <cache/PageCache.h>
#include <render/PageRenderer.h>

#include <memory>

class TextReader {
 public:
  enum class TextSize : uint8_t { Small = 0, Standard = 1 };

  bool open(SDCardManager& card, freeink::FreeInkDisplay& display, const char* path,
            uint32_t savedChar, uint16_t width, uint16_t height, TextSize textSize);
  void close();
  bool render(freeink::FreeInkDisplay& display);
  bool next() { return page_ + 1 < cacheReader_.pageCount() && (++page_, true); }
  bool previous() { return page_ > 0 && (--page_, true); }
  uint32_t page() const { return page_; }
  uint32_t pageCount() const { return cacheReader_.pageCount(); }
  uint32_t characterOffset() const { return cacheReader_.charStart(page_); }
  const char* path() const { return path_; }
  TextSize textSize() const { return textSize_; }

 private:
  static constexpr size_t INDEX_BYTES = 32 * 1024;
  static constexpr size_t PAGE_BYTES = 20 * 1024;
  static constexpr size_t BUILD_BYTES = 112 * 1024;
  bool prepareCache(freeink::FreeInkDisplay& display);
  SdBookSource source_;
  SdCache cache_;
  freeink::book::PageCacheReader cacheReader_;
  std::unique_ptr<uint8_t[]> indexMemory_;
  std::unique_ptr<uint8_t[]> pageMemory_;
  freeink::book::Arena indexArena_;
  freeink::book::Arena pageArena_;
  freeink::ui::BitmapBookFont standardFont_{freeink::ui::kReaderFont};
  freeink::ui::BitmapBookFont smallFont_{freeink::ui::kReaderSmallFont};
  TextSize textSize_ = TextSize::Standard;
  freeink::book::FontChain fonts_;
  freeink::book::LayoutParams layout_;
  char path_[128]{};
  char cacheName_[32]{};
  uint32_t generation_ = 0;
  uint32_t page_ = 0;
};
