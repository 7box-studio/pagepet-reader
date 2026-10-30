#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

constexpr uint32_t READING_STATE_MAGIC = 0x50505431;
constexpr uint32_t READING_STATE_VERSION = 2;

struct ReadingState {
  uint32_t magic;
  uint32_t version;
  uint32_t characterOffset;
  uint32_t spine;
  char path[128];
  uint32_t checksum;
};

inline uint32_t readingStateChecksum(const void* data, size_t size) {
  uint32_t hash = 2166136261u;
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}

inline bool decodeReadingState(const void* bytes, size_t size, ReadingState* out) {
  // Preserve saved TXT positions from firmware using the original record.
  struct LegacyState {
    uint32_t magic, version, characterOffset;
    char path[128];
    uint32_t checksum;
  };
  ReadingState value{};
  if (size == sizeof(LegacyState)) {
    LegacyState old;
    memcpy(&old, bytes, size);
    if (old.magic != READING_STATE_MAGIC || old.version != 1 ||
        old.checksum != readingStateChecksum(&old, offsetof(LegacyState, checksum)) ||
        !memchr(old.path, '\0', sizeof(old.path))) return false;
    value.magic = old.magic;
    value.version = READING_STATE_VERSION;
    value.characterOffset = old.characterOffset;
    memcpy(value.path, old.path, sizeof(value.path));
    value.checksum = readingStateChecksum(&value, offsetof(ReadingState, checksum));
  } else if (size == sizeof(ReadingState)) {
    memcpy(&value, bytes, size);
    if (value.magic != READING_STATE_MAGIC || value.version != READING_STATE_VERSION ||
        value.checksum != readingStateChecksum(&value, offsetof(ReadingState, checksum)) ||
        !memchr(value.path, '\0', sizeof(value.path))) return false;
  } else return false;
  *out = value;
  return true;
}
