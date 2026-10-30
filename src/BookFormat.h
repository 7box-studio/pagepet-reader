#pragma once

#include <BookStorage.h>
#include <BookTypes.h>

bool isEpubPath(const char* path);
bool isBookPath(const char* path);
// Hash only the CRC-bearing ZIP directory, never inflate book contents.
freeink::book::BookStatus epubFingerprint(freeink::book::BookSource& source, uint32_t* out);
