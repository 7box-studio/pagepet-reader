#include "TextReader.h"
#include "BookFormat.h"

#include <Arduino.h>
#include <FreeInkDisplay.h>
#include <layout/ChapterLayout.h>

#include <cstdio>
#include <cstring>
#include <new>

using namespace freeink::book;

namespace {
// Always return the framebuffer, including after failed SD writes or parses.
class FrameLoan {
 public:
  explicit FrameLoan(freeink::FreeInkDisplay& display) : display_(display) {
    data = display_.lendBuildStorage(&size);
  }
  ~FrameLoan() { if (data) display_.returnBuildStorage(); }
  uint8_t* data;
  uint32_t size = 0;
 private:
  freeink::FreeInkDisplay& display_;
};

// Long chapters still give the ESP scheduler/watchdog time between pages.
class YieldingSink final : public PageSink {
 public:
  explicit YieldingSink(PageCacheWriter& writer) : writer_(writer) {}
  bool onPage(const Page& page) override {
    const bool okay = writer_.onPage(page);
    delay(1);
    return okay;
  }
  void onAnchor(uint32_t hash, uint32_t character) override { writer_.onAnchor(hash, character); }
 private:
  PageCacheWriter& writer_;
};
}

void TextReader::notifyBuild() {
  if (!buildShown_ && buildCallback_) buildCallback_();
  buildShown_ = true;
}

bool TextReader::open(SDCardManager& card, freeink::FreeInkDisplay& display, const char* path,
                      uint32_t savedChar, uint16_t width, uint16_t height, TextSize textSize,
                      uint32_t savedSpine) {
  close();
  buildShown_ = false;
  status_ = BookStatus::Ok;
  textSize_ = textSize;
  if (!path || strlen(path) >= sizeof(path_) || !isBookPath(path)) {
    status_ = BookStatus::Unsupported;
    return false;
  }
  if (!source_.open(card, path) || !cache_.open(card, bookIdFor(path))) {
    status_ = BookStatus::IoError;
    close();
    return false;
  }
  memcpy(path_, path, strlen(path) + 1);
  epub_ = isEpubPath(path);

  fonts_ = FontChain{};
  auto* activeFont = textSize_ == TextSize::Small ? &smallFont_ : &standardFont_;
  if (!fonts_.add(activeFont)) { close(); return false; }
  layout_ = LayoutParams{};
  layout_.pageWidth = width;
  layout_.pageHeight = height;
  layout_.marginLeft = 26;
  layout_.marginRight = 26;
  layout_.marginTop = 32;
  layout_.marginBottom = 46;
  layout_.baseSizePx = textSize_ == TextSize::Small ? 18 : 22;
  layout_.font = &fonts_;

  uint32_t fingerprint = source_.revision();
  if (epub_) {
    status_ = epubFingerprint(source_, &fingerprint);
    if (status_ != BookStatus::Ok || !prepareCatalog(display)) { close(); return false; }
    layout_.stylesheet = catalog_.stylesheet();
    if (catalog_.metadata().language[0]) layout_.language = catalog_.metadata().language;
  }
  // Include the container's CRC-bearing directory: replacing a book at the
  // same path must invalidate chapter pages even if its byte size is unchanged.
  generation_ = layoutGenerationHash(layout_, 0x50455402u ^ fingerprint);
  const uint32_t firstSpine = savedSpine < spineCount() ? savedSpine : 0;
  if (!openChapter(display, firstSpine, firstSpine == savedSpine ? savedChar : 0)) {
    close();
    return false;
  }
  // Empty XHTML (common in covers/front matter) is valid. Find readable content.
  if (cacheReader_.pageCount() == 0 && moveChapter(display, true) != Move::Moved) {
    if (status_ == BookStatus::Ok) status_ = BookStatus::NotFound;
    close();
    return false;
  }
  progressPage_ = page_;
  return true;
}

bool TextReader::loadCatalog() {
  size_t bytes = 0;
  status_ = BookCatalog::residentBytes(cache_, &bytes);
  if (status_ != BookStatus::Ok) return false;
  if (bytes > CATALOG_LIMIT) { status_ = BookStatus::OutOfMemory; return false; }
  catalogMemory_.reset(new (std::nothrow) uint8_t[bytes]);
  if (!catalogMemory_) { status_ = BookStatus::OutOfMemory; return false; }
  catalogArena_.init(catalogMemory_.get(), bytes);
  uint8_t fingerprintBuffer[2048];
  Arena scratch(fingerprintBuffer, sizeof(fingerprintBuffer));
  status_ = catalog_.open(source_, cache_, catalogArena_, scratch);
  return status_ == BookStatus::Ok && catalog_.spineCount() > 0;
}

bool TextReader::prepareCatalog(freeink::FreeInkDisplay& display) {
  if (loadCatalog()) return true;
  // A valid catalog exceeding the device budget will not improve on rebuild.
  if (status_ == BookStatus::OutOfMemory) return false;
  catalog_ = BookCatalog{};
  catalogArena_.init(nullptr, 0);
  catalogMemory_.reset();
  {
    auto memory = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[BUILD_BYTES]);
    if (!memory) { status_ = BookStatus::OutOfMemory; return false; }
    notifyBuild();
    FrameLoan frame(display);
    if (!frame.data) { status_ = BookStatus::OutOfMemory; return false; }
    Arena tables(memory.get(), BUILD_BYTES);
    Arena parse(frame.data, frame.size);
    const uint32_t start = millis();
    status_ = BookCatalog::build(source_, cache_, tables, &parse);
    Serial.printf("[reader] catalog status=%s time=%lums tables=%u parse=%u\n",
                  bookStatusName(status_), static_cast<unsigned long>(millis() - start),
                  static_cast<unsigned>(tables.highWater()), static_cast<unsigned>(parse.highWater()));
    if (status_ != BookStatus::Ok) { cache_.abortWrite(); return false; }
  }
  return loadCatalog();
}

bool TextReader::readIndex() {
  indexMemory_.reset(new (std::nothrow) uint8_t[INDEX_BYTES]);
  if (!indexMemory_) { status_ = BookStatus::OutOfMemory; return false; }
  indexArena_.init(indexMemory_.get(), INDEX_BYTES);
  status_ = cacheReader_.open(cache_, cacheName_, generation_, indexArena_);
  return status_ == BookStatus::Ok && !cacheReader_.isPartial();
}

bool TextReader::prepareCache(freeink::FreeInkDisplay& display, uint32_t chapter) {
  if (readIndex()) return true;
  cacheReader_ = PageCacheReader{};
  indexArena_.init(nullptr, 0);
  indexMemory_.reset();
  {
    auto buildMemory = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[BUILD_BYTES]);
    if (!buildMemory) { status_ = BookStatus::OutOfMemory; return false; }
    notifyBuild();
    FrameLoan frame(display);
    if (!frame.data) { status_ = BookStatus::OutOfMemory; return false; }
    Arena index(frame.data, frame.size);
    PageCacheWriter writer;
    if (!writer.begin(cache_, cacheName_, generation_, index)) {
      status_ = BookStatus::IoError;
      cache_.abortWrite();
      return false;
    }
    YieldingSink sink(writer);
    uint32_t totalChars = 0;
    const uint32_t start = millis();
    Arena buildArena(buildMemory.get(), epub_ ? LAYOUT_BYTES : BUILD_BYTES);
    if (epub_) {
      ZipEntry entry;
      char href[512];
      status_ = catalog_.spineEntry(chapter, &entry);
      if (status_ == BookStatus::Ok) status_ = catalog_.spineHref(chapter, href, sizeof(href));
      if (status_ == BookStatus::Ok) {
        Arena parse(buildMemory.get() + LAYOUT_BYTES, BUILD_BYTES - LAYOUT_BYTES);
        ChapterLayoutSession session;
        status_ = session.begin(source_, &catalog_.zip(), source_, entry, href, layout_, buildArena, sink, &parse);
        while (status_ == BookStatus::Ok && !session.done()) {
          status_ = session.step(4);
          delay(1);
        }
        totalChars = session.totalChars();
      }
    } else {
      status_ = ChapterLayout::layoutPlainText(source_, layout_, buildArena, sink, nullptr, &totalChars);
    }
    Serial.printf("[reader] layout spine=%s status=%s time=%lums arena=%u index=%u\n",
                  cacheName_, bookStatusName(status_), static_cast<unsigned long>(millis() - start),
                  static_cast<unsigned>(buildArena.highWater()), static_cast<unsigned>(index.highWater()));
    if (status_ != BookStatus::Ok || writer.failed()) {
      if (writer.failed()) status_ = BookStatus::IoError;
      cache_.abortWrite();
      return false;
    }
    writer.setTotalChars(totalChars);
    if (!writer.finish()) { status_ = BookStatus::IoError; cache_.abortWrite(); return false; }
  }
  return readIndex();
}

bool TextReader::openChapter(freeink::FreeInkDisplay& display, uint32_t chapter, uint32_t character) {
  // Build memory and reading memory never coexist. Keep the logical position
  // intact until the new chapter has opened, so an SD failure saves the old one.
  cacheReader_ = PageCacheReader{};
  indexArena_.init(nullptr, 0);
  pageArena_.init(nullptr, 0);
  indexMemory_.reset();
  pageMemory_.reset();
  if (!pageCacheName(chapter, generation_, cacheName_, sizeof(cacheName_))) {
    status_ = BookStatus::Unsupported;
    return false;
  }
  if (!prepareCache(display, chapter)) return false;
  pageMemory_.reset(new (std::nothrow) uint8_t[PAGE_BYTES]);
  if (!pageMemory_) { status_ = BookStatus::OutOfMemory; return false; }
  pageArena_.init(pageMemory_.get(), PAGE_BYTES);
  spine_ = chapter;
  page_ = cacheReader_.pageForChar(character);
  character_ = cacheReader_.charStart(page_);
  return true;
}

TextReader::Move TextReader::moveChapter(freeink::FreeInkDisplay& display, bool forward) {
  uint32_t chapter = spine_;
  const uint32_t oldSpine = spine_, oldCharacter = character_, oldPage = page_;
  while (forward ? chapter + 1 < spineCount() : chapter > 0) {
    chapter = forward ? chapter + 1 : chapter - 1;
    if (!openChapter(display, chapter, forward ? 0 : UINT32_MAX)) {
      spine_ = oldSpine;
      character_ = oldCharacter;
      page_ = oldPage;
      return Move::Error;
    }
    if (cacheReader_.pageCount() > 0) return Move::Moved;
  }
  // Trailing/leading empty chapters must not strand the reader on an empty page.
  if (spine_ != oldSpine && !openChapter(display, oldSpine, oldCharacter)) {
    spine_ = oldSpine;
    character_ = oldCharacter;
    page_ = oldPage;
    return Move::Error;
  }
  return Move::End;
}

TextReader::Move TextReader::next(freeink::FreeInkDisplay& display) {
  buildShown_ = false;
  Move result = Move::Moved;
  if (page_ + 1 < pageCount()) ++page_;
  else result = moveChapter(display, true);
  if (result == Move::Moved) { character_ = cacheReader_.charStart(page_); ++progressPage_; }
  if (result == Move::End && buildShown_ && !render(display)) return Move::Error;
  return result;
}

TextReader::Move TextReader::previous(freeink::FreeInkDisplay& display) {
  buildShown_ = false;
  Move result = Move::Moved;
  if (page_ > 0) --page_;
  else result = moveChapter(display, false);
  if (result == Move::Moved) { character_ = cacheReader_.charStart(page_); --progressPage_; }
  if (result == Move::End && buildShown_ && !render(display)) return Move::Error;
  return result;
}

bool TextReader::render(freeink::FreeInkDisplay& display) {
  if (!display.getFrameBuffer()) return false;
  const uint32_t start = millis();
  pageArena_.reset();
  Page page{};
  status_ = cacheReader_.readPage(page_, pageArena_, &page);
  if (status_ != BookStatus::Ok) {
    // A valid footer can still surround a damaged page blob. Retrying must
    // rebuild it instead of repeatedly accepting the same broken index.
    if (status_ == BookStatus::Stale) cache_.remove(cacheName_);
    return false;
  }
  display.clearScreen();
  FrameTarget target{display.getFrameBuffer(), static_cast<int16_t>(display.getDisplayWidth()),
                     static_cast<int16_t>(display.getDisplayHeight()),
                     static_cast<int16_t>(display.getDisplayWidthBytes()), FrameFormat::Mono1Dithered,
                     FrameRotation::Portrait};
  PageRenderer::renderText(page, fonts_, target);
  if (epub_ && page.imageCount) {
    // No resident decoder on text pages. Large/unsupported illustrations may
    // be omitted, but an actual SD read error must enter the normal retry path.
    auto imageMemory = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[IMAGE_BYTES]);
    BookStatus imageStatus = BookStatus::OutOfMemory;
    if (imageMemory) {
      Arena images(imageMemory.get(), IMAGE_BYTES);
      imageStatus = PageRenderer::renderImages(page, source_, catalog_.zip(), images, target);
    }
    if (imageStatus == BookStatus::IoError || imageStatus == BookStatus::Truncated) {
      status_ = imageStatus;
      return false;
    }
    if (imageStatus != BookStatus::Ok) Serial.printf("[reader] image skipped: %s\n", bookStatusName(imageStatus));
  }
  if (renderCallback_) renderCallback_();
  display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
  Serial.printf("[reader] chapter %lu/%lu page %lu/%lu render=%lums free=%u largest=%u\n",
                static_cast<unsigned long>(spine_ + 1), static_cast<unsigned long>(spineCount()),
                static_cast<unsigned long>(page_ + 1), static_cast<unsigned long>(pageCount()),
                static_cast<unsigned long>(millis() - start), static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
  return true;
}

void TextReader::close() {
  source_.close();
  cache_.close();
  cacheReader_ = PageCacheReader{};
  catalog_ = BookCatalog{};
  layout_ = LayoutParams{};
  pageArena_.init(nullptr, 0);
  indexArena_.init(nullptr, 0);
  catalogArena_.init(nullptr, 0);
  pageMemory_.reset();
  indexMemory_.reset();
  catalogMemory_.reset();
  path_[0] = '\0';
  page_ = spine_ = character_ = progressPage_ = 0;
  epub_ = false;
}
