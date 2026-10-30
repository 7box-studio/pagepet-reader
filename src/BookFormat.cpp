#include "BookFormat.h"

#include <epub/ZipCatalog.h>
#include <cstring>

namespace {
bool extension(const char* path, const char* suffix) {
  if (!path) return false;
  const size_t length = strlen(path), count = strlen(suffix);
  if (length < count) return false;
  for (size_t i = 0; i < count; ++i) {
    char ch = path[length - count + i];
    if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    if (ch != suffix[i]) return false;
  }
  return true;
}
}

bool isEpubPath(const char* path) { return extension(path, ".epub"); }
bool isBookPath(const char* path) { return isEpubPath(path) || extension(path, ".txt"); }

freeink::book::BookStatus epubFingerprint(freeink::book::BookSource& source, uint32_t* out) {
  using namespace freeink::book;
  uint32_t offset = 0, size = 0;
  uint16_t entries = 0;
  const BookStatus status = ZipCatalog::locateCentralDirectory(source, &offset, &size, &entries);
  if (status != BookStatus::Ok) return status;
  uint8_t buffer[1024];
  uint32_t hash = 2166136261u;
  uint32_t consumed = 0;
  while (consumed < size) {
    const uint32_t want = size - consumed < sizeof(buffer) ? size - consumed : sizeof(buffer);
    const int32_t got = source.readAt(static_cast<uint64_t>(offset) + consumed, buffer, want);
    if (got <= 0 || static_cast<uint32_t>(got) > want) return BookStatus::IoError;
    for (int32_t i = 0; i < got; ++i) hash = (hash ^ buffer[i]) * 16777619u;
    consumed += got;
  }
  *out = hash ^ offset ^ static_cast<uint32_t>(source.size()) ^
         static_cast<uint32_t>(source.size() >> 32) ^ entries;
  return BookStatus::Ok;
}
