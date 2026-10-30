#include "TextReader.h"
#include "BookFormat.h"
#include "ReadingState.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using Status = freeink::book::BookStatus;
using Move = TextReader::Move;
namespace fs = std::filesystem;

static fs::path cacheDir(const char* name) {
  char hash[16];
  snprintf(hash, sizeof(hash), "%08x", bookIdFor(name));
  return hostsd::root / ".pagepet/cache" / hash;
}
static bool cached(const char* name, uint32_t spine) {
  char prefix[16];
  snprintf(prefix, sizeof(prefix), "s%04u-", spine);
  for (auto& entry : fs::directory_iterator(cacheDir(name))) {
    if (entry.path().extension() == ".fibp" && entry.path().filename().string().rfind(prefix, 0) == 0) return true;
  }
  return false;
}
static void noParts() {
  for (auto& entry : fs::recursive_directory_iterator(hostsd::root)) assert(entry.path().extension() != ".part");
}
static void states() {
  assert(isBookPath("/a.EpUb") && isBookPath("/a.TXT") && !isBookPath("/a.epub.zip") && !isBookPath(nullptr));
  ReadingState value{READING_STATE_MAGIC, READING_STATE_VERSION, 1567, 7, "/book.epub", 0};
  value.checksum = readingStateChecksum(&value, offsetof(ReadingState, checksum));
  ReadingState decoded{};
  assert(decodeReadingState(&value, sizeof(value), &decoded));
  assert(decoded.spine == 7 && decoded.characterOffset == 1567);
  value.path[5] ^= 1;
  assert(!decodeReadingState(&value, sizeof(value), &decoded));
  struct Legacy { uint32_t magic, version, character; char path[128]; uint32_t checksum; };
  Legacy old{READING_STATE_MAGIC, 1, 9876, "/plain.txt", 0};
  old.checksum = readingStateChecksum(&old, offsetof(Legacy, checksum));
  assert(decodeReadingState(&old, sizeof(old), &decoded));
  assert(decoded.spine == 0 && decoded.characterOffset == 9876 && decoded.version == 2);
  assert(!decodeReadingState(&old, sizeof(old) - 1, &decoded));
}
int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  assert(argc == 2);
  hostsd::root = argv[1];
  states();
  SDCardManager card;
  freeink::FreeInkDisplay display;
  TextReader reader;
  const auto open = [&](const char* name, uint32_t character = 0, uint32_t spine = 0,
                        TextReader::TextSize size = TextReader::TextSize::Standard) {
    const bool okay = reader.open(card, display, name, character, 528, 792, size, spine);
    assert(!display.lent);
    return okay;
  };
  for (const char* name : {"/book.epub", "/stored.EPUB"}) {
    assert(open(name));
    assert(reader.spine() == 1 && reader.spineCount() == 5 && reader.pageCount() > 2);
    assert(cached(name, 0) && cached(name, 1) && !cached(name, 3));
    assert(reader.render(display));
    uint32_t loans = display.loans, reads = hostsd::sourceReads;
    const uint32_t opens = hostsd::fileOpens;
    assert(reader.next(display) == Move::Moved && reader.next(display) == Move::Moved);
    assert(reader.render(display));
    assert(display.loans == loans && hostsd::sourceReads == reads);
    assert(hostsd::fileOpens == opens);  // consecutive cache reads reuse the file handle
    const uint32_t offset = reader.characterOffset();
    assert(offset > 0);
    const uint32_t savedPage = reader.page();
    reader.close();
    assert(open(name, offset, 1) && reader.page() == savedPage);
    assert(display.loans == loans); // warm open does no layout or catalog build
    reader.close();
    assert(open(name, offset, 1, TextReader::TextSize::Small));
    assert(reader.spine() == 1 && reader.characterOffset() <= offset);
    const uint32_t lastPage = reader.pageCount() - 1;
    while (reader.spine() == 1) assert(reader.next(display) == Move::Moved);
    assert(reader.spine() == 3 && reader.page() == 0 && cached(name, 3));
    assert(reader.previous(display) == Move::Moved);
    assert(reader.spine() == 1 && reader.page() == lastPage);
    assert(reader.next(display) == Move::Moved && reader.spine() == 3);
    assert(reader.next(display) == Move::End && reader.spine() == 3);
    assert(reader.render(display));
    assert(open(name, 0, 1));
    assert(reader.previous(display) == Move::End && reader.spine() == 1);
    reader.close();
  }
  assert(open("/replace.epub") && reader.render(display));
  const auto oldPixels = display.pixels;
  reader.close();
  assert(fs::file_size(hostsd::root / "replace.epub") == fs::file_size(hostsd::root / "replace-new.epub"));
  fs::copy_file(hostsd::root / "replace-new.epub", hostsd::root / "replace.epub", fs::copy_options::overwrite_existing);
  assert(open("/replace.epub") && reader.render(display));
  assert(display.pixels != oldPixels);
  reader.close();

  assert(open("/plain.txt"));
  assert(reader.spineCount() == 1 && reader.pageCount() > 5);
  assert(reader.next(display) == Move::Moved && reader.render(display));
  reader.close();
  assert(!open("/invalid.epub"));
  assert(!open("/encrypted.epub") && reader.status() == Status::Encrypted);
  assert(open("/broken.epub"));
  const uint32_t oldSpine = reader.spine(), oldCharacter = reader.characterOffset();
  assert(reader.next(display) == Move::Error && reader.status() == Status::ParseError);
  assert(reader.spine() == oldSpine && reader.characterOffset() == oldCharacter && !display.lent);
  reader.close();
  noParts();

  // An interrupted write must not become a warm cache or leave the display lent.
  fs::remove_all(cacheDir("/plain.txt"));
  hostsd::writeBudget = 256;
  assert(!open("/plain.txt"));
  assert(reader.status() == Status::IoError && !display.lent);
  hostsd::writeBudget = -1;
  noParts();
  assert(open("/plain.txt") && reader.render(display));
  reader.close();
  // A torn final file and leftover write from a power cut must rebuild.
  fs::path plainCache;
  for (auto& entry : fs::directory_iterator(cacheDir("/plain.txt"))) {
    if (entry.path().extension() == ".fibp") plainCache = entry.path();
  }
  assert(!plainCache.empty());
  fs::resize_file(plainCache, 10);
  std::ofstream(plainCache.string() + ".part") << "interrupted";
  assert(open("/plain.txt") && reader.render(display));
  reader.close();
  noParts();
  // Keep a valid index/footer but damage a run's text length inside page 0.
  {
    std::fstream damaged(plainCache, std::ios::in | std::ios::out | std::ios::binary);
    damaged.seekp(30); // header (12), page header (10), run text-length (8)
    damaged.put('\xff');
    damaged.put('\xff');
  }
  assert(open("/plain.txt"));
  assert(!reader.render(display) && reader.status() == Status::Stale);
  reader.close();
  assert(open("/plain.txt") && reader.render(display));
  reader.close();
  fs::resize_file(cacheDir("/book.epub") / "catalog.fibc", 10);
  assert(open("/book.epub", 0, 1) && reader.render(display));
  reader.close();
  assert(open("/book.epub", 0, 1));
  hostsd::online = false;
  assert(!reader.render(display) && reader.status() == Status::IoError);
  hostsd::online = true;
  reader.close();
  assert(open("/book.epub", 0, 1) && reader.render(display));
  reader.close();

  assert(open("/large.epub") && reader.pageCount() > 100 && reader.render(display));
  reader.close();
  assert(open("/many.epub") && reader.spineCount() == 1700 && reader.render(display));
  reader.close();
  assert(!open("/too-many.epub") && reader.status() == Status::OutOfMemory);
  assert(!display.lent);
  assert(open("/rich.epub") && reader.render(display));
  reader.close();
  const uint32_t warmLoans = display.loans;
  assert(open("/rich.epub") && reader.render(display) && display.loans == warmLoans);
  reader.close();
  // The cache retains illustration placement, links and anchors for future
  // button-driven note navigation; layout must not flatten the EPUB to TXT.
  SdCache cache;
  assert(cache.open(card, bookIdFor("/rich.epub")));
  std::string pageName;
  for (auto& entry : fs::directory_iterator(cacheDir("/rich.epub"))) {
    if (entry.path().extension() == ".fibp") pageName = entry.path().filename().string();
  }
  assert(!pageName.empty());
  uint32_t generation;
  assert(cache.readAt(pageName.c_str(), 8, &generation, sizeof(generation)) == sizeof(generation));
  uint8_t indexBytes[32 * 1024], pageBytes[20 * 1024];
  freeink::book::Arena index(indexBytes, sizeof(indexBytes)), pages(pageBytes, sizeof(pageBytes));
  freeink::book::PageCacheReader rich;
  assert(rich.open(cache, pageName.c_str(), generation, index) == Status::Ok);
  bool image = false, link = false;
  for (uint32_t p = 0; p < rich.pageCount(); ++p) {
    pages.reset();
    freeink::book::Page page{};
    assert(rich.readPage(p, pages, &page) == Status::Ok);
    image = image || page.imageCount > 0;
    link = link || page.linkCount > 0;
  }
  uint32_t noteCharacter = 0;
  assert(image && link && rich.charForAnchor(freeink::book::ZipCatalog::hashPath("note"), &noteCharacter));
  assert(noteCharacter > 0);
  cache.close();
  noParts();
  std::cout << "Reader integration checks passed (real EPUB parser, layout, fonts, SD cache and state).\n";
}
