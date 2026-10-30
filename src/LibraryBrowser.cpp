#include "LibraryBrowser.h"
#include "BookFormat.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

void LibraryBrowser::reset() {
  path_[0] = '/';
  path_[1] = '\0';
  count_ = selection_ = first_ = 0;
  rows_ = 0;
}

LibraryBrowser::Status LibraryBrowser::scan(SDCardManager& card, uint32_t first,
                                            const char* focus, uint32_t* found) {
  first_ = first;
  count_ = 0;
  rows_ = 0;
  if (found) *found = UINT32_MAX;
  FsFile directory = card.open(path_, O_RDONLY);
  if (!directory || !directory.isDirectory()) return Status::IoError;
  uint32_t lastYield = millis();
  // Two sequential passes put folders first without sorting/retaining every
  // filename. Within each group use the card's directory order.
  for (unsigned pass = 0; pass < 2; ++pass) {
    directory.rewindDirectory();
    for (FsFile file = directory.openNextFile(); file; file = directory.openNextFile()) {
      if (millis() - lastYield >= 20) { delay(1); lastYield = millis(); }
      const bool folder = file.isDirectory();
      if (folder != (pass == 0)) continue;
      char name[PATH_BYTES + 1]{};
      if (!file.getName(name, sizeof(name)) || !name[0] || name[0] == '.') continue;
      if (!folder && !isBookPath(name)) continue;
      if (focus && strcmp(name, focus) == 0 && found) *found = count_;
      if (count_ >= first_ && rows_ < PAGE_SIZE) {
        Entry& entry = entries_[rows_++];
        const size_t length = strlen(name);
        entry.tooLong = length >= sizeof(entry.name);
        size_t bytes = entry.tooLong ? sizeof(entry.name) - 1 : length;
        // Keep a shortened, unopenable row valid UTF-8.
        while (bytes && (static_cast<uint8_t>(name[bytes]) & 0xc0) == 0x80) --bytes;
        memcpy(entry.name, name, bytes);
        entry.name[bytes] = '\0';
        entry.directory = folder;
      }
      ++count_;
    }
    if (directory.getError()) return Status::IoError;
  }
  return Status::Ok;
}

LibraryBrowser::Status LibraryBrowser::loadSelection(SDCardManager& card) {
  auto status = scan(card, selection_ / PAGE_SIZE * PAGE_SIZE);
  if (status != Status::Ok) return status;
  if (!count_) { selection_ = 0; return Status::Ok; }
  if (selection_ >= count_) selection_ = count_ - 1;
  const uint32_t wanted = selection_ / PAGE_SIZE * PAGE_SIZE;
  return wanted == first_ ? Status::Ok : scan(card, wanted);
}

LibraryBrowser::Status LibraryBrowser::refresh(SDCardManager& card) { return loadSelection(card); }

LibraryBrowser::Status LibraryBrowser::move(SDCardManager& card, bool forward) {
  if (!count_) return Status::Ok;
  if (forward) selection_ = selection_ + 1 < count_ ? selection_ + 1 : 0;
  else selection_ = selection_ ? selection_ - 1 : count_ - 1;
  return selection_ / PAGE_SIZE * PAGE_SIZE == first_ ? Status::Ok : loadSelection(card);
}

const LibraryBrowser::Entry* LibraryBrowser::selectedEntry() const {
  return selection_ >= first_ ? row(selection_ - first_) : nullptr;
}

LibraryBrowser::Status LibraryBrowser::selectedPath(char* out, size_t cap) const {
  const auto* entry = selectedEntry();
  if (!entry) return Status::IoError;
  if (entry->tooLong) return Status::PathTooLong;
  const int n = snprintf(out, cap, "%s%s%s", path_, atRoot() ? "" : "/", entry->name);
  return n > 0 && static_cast<size_t>(n) < cap ? Status::Ok : Status::PathTooLong;
}

LibraryBrowser::Status LibraryBrowser::enter(SDCardManager& card) {
  const auto* entry = selectedEntry();
  if (!entry || !entry->directory) return Status::IoError;
  char next[PATH_BYTES];
  const auto status = selectedPath(next, sizeof(next));
  if (status != Status::Ok) return status;
  memcpy(path_, next, strlen(next) + 1);
  selection_ = 0;
  return loadSelection(card);
}

LibraryBrowser::Status LibraryBrowser::focusName(SDCardManager& card, const char* name) {
  uint32_t found = UINT32_MAX;
  selection_ = 0;
  const auto status = scan(card, 0, name, &found);
  if (status != Status::Ok) return status;
  if (found == UINT32_MAX) return Status::Ok;
  selection_ = found;
  return found < PAGE_SIZE ? Status::Ok : loadSelection(card);
}

LibraryBrowser::Status LibraryBrowser::parent(SDCardManager& card) {
  if (atRoot()) return Status::Ok;
  char child[PATH_BYTES];
  char* slash = strrchr(path_, '/');
  memcpy(child, slash + 1, strlen(slash + 1) + 1);
  if (slash == path_) path_[1] = '\0';
  else *slash = '\0';
  return focusName(card, child);
}

LibraryBrowser::Status LibraryBrowser::focusBook(SDCardManager& card, const char* absolutePath) {
  if (!absolutePath || absolutePath[0] != '/' || strlen(absolutePath) >= sizeof(path_)) {
    return Status::PathTooLong;
  }
  const char* slash = strrchr(absolutePath, '/');
  const size_t bytes = slash == absolutePath ? 1 : static_cast<size_t>(slash - absolutePath);
  memcpy(path_, absolutePath, bytes);
  path_[bytes] = '\0';
  return focusName(card, slash + 1);
}
