#include "TextReader.h"

#include <Arduino.h>
#include <FreeInkDisplay.h>
#include <layout/ChapterLayout.h>

#include <cstdio>
#include <cstring>
#include <new>

using namespace freeink::book;

bool TextReader::open(SDCardManager& card, freeink::FreeInkDisplay& display, const char* path,
                      uint32_t savedChar, uint16_t width, uint16_t height, TextSize textSize) {
  close();
  textSize_ = textSize;
  if (strlen(path) >= sizeof(path_) || !source_.open(card, path)) return false;
  memcpy(path_, path, strlen(path) + 1);
  if (!cache_.open(card, bookIdFor(path))) return false;

  fonts_ = FontChain{};
  auto* activeFont = textSize_ == TextSize::Small ? &smallFont_ : &standardFont_;
  if (!fonts_.add(activeFont)) return false;
  layout_ = LayoutParams{};
  layout_.pageWidth = width;
  layout_.pageHeight = height;
  layout_.marginLeft = 26;
  layout_.marginRight = 26;
  layout_.marginTop = 32;
  layout_.marginBottom = 46;
  layout_.baseSizePx = textSize_ == TextSize::Small ? 18 : 22;
  layout_.font = &fonts_;
  generation_ = layoutGenerationHash(layout_, 0x50455401u);
  if (!pageCacheName(0, generation_, cacheName_, sizeof(cacheName_)) || !prepareCache(display)) {
    close();
    return false;
  }
  pageMemory_.reset(new (std::nothrow) uint8_t[PAGE_BYTES]);
  if (!pageMemory_) {
    Serial.printf("[reader] out of memory: page arena\n");
    close();
    return false;
  }
  pageArena_.init(pageMemory_.get(), PAGE_BYTES);
  page_ = cacheReader_.pageForChar(savedChar);
  if (page_ >= cacheReader_.pageCount()) page_ = 0;
  return true;
}

bool TextReader::prepareCache(freeink::FreeInkDisplay& display) {
  indexMemory_.reset(new (std::nothrow) uint8_t[INDEX_BYTES]);
  if (!indexMemory_) {
    Serial.printf("[reader] out of memory: index arena\n");
    return false;
  }
  indexArena_.init(indexMemory_.get(), INDEX_BYTES);
  if (cacheReader_.open(cache_, cacheName_, generation_, indexArena_) == BookStatus::Ok &&
      cacheReader_.pageCount() > 0 && !cacheReader_.isPartial()) return true;
  cacheReader_ = PageCacheReader{};
  indexArena_.init(nullptr, 0);
  indexMemory_.reset();

  auto buildMemory = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[BUILD_BYTES]);
  if (!buildMemory) {
    Serial.printf("[reader] out of memory: layout arena\n");
    return false;
  }
  Arena buildArena(buildMemory.get(), BUILD_BYTES);
  uint32_t frameBytes = 0;
  uint8_t* frameStorage = display.lendBuildStorage(&frameBytes);
  if (!frameStorage) {
    Serial.printf("[reader] framebuffer loan unavailable\n");
    return false;
  }
  indexArena_.init(frameStorage, frameBytes);
  PageCacheWriter writer;
  if (!writer.begin(cache_, cacheName_, generation_, indexArena_)) {
    display.returnBuildStorage();
    return false;
  }
  uint32_t totalChars = 0;
  const auto status = ChapterLayout::layoutPlainText(source_, layout_, buildArena, writer, nullptr, &totalChars);
  if (status != BookStatus::Ok) {
    Serial.printf("[reader] layout failed: %d, used=%u, refused=%u\n", static_cast<int>(status),
                  static_cast<unsigned>(buildArena.highWater()),
                  static_cast<unsigned>(buildArena.failedAllocSize()));
    display.returnBuildStorage();
    return false;
  }
  writer.setTotalChars(totalChars);
  const bool finished = writer.finish();
  display.returnBuildStorage();
  if (!finished) return false;
  buildMemory.reset();
  indexMemory_.reset(new (std::nothrow) uint8_t[INDEX_BYTES]);
  if (!indexMemory_) {
    Serial.printf("[reader] out of memory: index reopen\n");
    return false;
  }
  indexArena_.init(indexMemory_.get(), INDEX_BYTES);
  return cacheReader_.open(cache_, cacheName_, generation_, indexArena_) == BookStatus::Ok &&
         cacheReader_.pageCount() > 0;
}

bool TextReader::render(freeink::FreeInkDisplay& display) {
  if (!display.getFrameBuffer()) return false;
  pageArena_.reset();
  Page page{};
  if (cacheReader_.readPage(page_, pageArena_, &page) != BookStatus::Ok) return false;
  display.clearScreen();
  FrameTarget target{display.getFrameBuffer(), static_cast<int16_t>(display.getDisplayWidth()),
                     static_cast<int16_t>(display.getDisplayHeight()),
                     static_cast<int16_t>(display.getDisplayWidthBytes()), FrameFormat::Mono1Dithered,
                     FrameRotation::Portrait};
  const auto start = millis();
  PageRenderer::renderText(page, fonts_, target);
  display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
  Serial.printf("[reader] page %lu/%lu display=%lums free=%u largest=%u\n",
                static_cast<unsigned long>(page_ + 1), static_cast<unsigned long>(cacheReader_.pageCount()),
                static_cast<unsigned long>(millis() - start), static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
  return true;
}

void TextReader::close() {
  source_.close();
  cacheReader_ = PageCacheReader{};
  pageArena_.init(nullptr, 0);
  indexArena_.init(nullptr, 0);
  pageMemory_.reset();
  indexMemory_.reset();
  path_[0] = '\0';
  page_ = 0;
}
