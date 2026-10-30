#pragma once

#include "Storage.h"

#include <FreeInkBook.h>
#include <BookCatalog.h>
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
  enum class Move : uint8_t { Moved, End, Error };
  void setBuildCallback(void (*callback)()) { buildCallback_ = callback; }
  void setRenderCallback(void (*callback)()) { renderCallback_ = callback; }

  bool open(SDCardManager& card, freeink::FreeInkDisplay& display, const char* path,
            uint32_t savedChar, uint16_t width, uint16_t height, TextSize textSize,
            uint32_t savedSpine = 0);
  void close();
  bool render(freeink::FreeInkDisplay& display);
  Move next(freeink::FreeInkDisplay& display);
  Move previous(freeink::FreeInkDisplay& display);
  uint32_t page() const { return page_; }
  uint32_t pageCount() const { return cacheReader_.pageCount(); }
  uint32_t characterOffset() const { return character_; }
  uint32_t spine() const { return spine_; }
  uint32_t spineCount() const { return epub_ ? catalog_.spineCount() : 1; }
  uint32_t progressPage() const { return progressPage_; }
  freeink::book::BookStatus status() const { return status_; }
  const char* path() const { return path_; }
  TextSize textSize() const { return textSize_; }

 private:
  static constexpr size_t INDEX_BYTES = 32 * 1024;
  static constexpr size_t PAGE_BYTES = 20 * 1024;
  static constexpr size_t BUILD_BYTES = 112 * 1024;
  static constexpr size_t CATALOG_LIMIT = 64 * 1024;
  static constexpr size_t LAYOUT_BYTES = 60 * 1024;
  static constexpr size_t IMAGE_BYTES = 96 * 1024;
  bool prepareCatalog(freeink::FreeInkDisplay& display);
  bool loadCatalog();
  bool prepareCache(freeink::FreeInkDisplay& display, uint32_t chapter);
  bool openChapter(freeink::FreeInkDisplay& display, uint32_t spine, uint32_t character);
  Move moveChapter(freeink::FreeInkDisplay& display, bool forward);
  bool readIndex();
  void notifyBuild();
  SdBookSource source_;
  SdCache cache_;
  freeink::book::PageCacheReader cacheReader_;
  freeink::book::BookCatalog catalog_;
  std::unique_ptr<uint8_t[]> catalogMemory_;
  freeink::book::Arena catalogArena_;
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
  uint32_t spine_ = 0;
  uint32_t character_ = 0;
  uint32_t progressPage_ = 0;
  bool epub_ = false;
  bool buildShown_ = false;
  void (*buildCallback_)() = nullptr;
  void (*renderCallback_)() = nullptr;
  freeink::book::BookStatus status_ = freeink::book::BookStatus::Ok;
};
