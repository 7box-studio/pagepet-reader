#pragma once

#include <cstdint>

namespace strings {

enum class Language : uint32_t { English = 0, Russian = 1 };

enum class Text : uint8_t {
  Title,
  Books,
  NoCard,
  NoBooks,
  Companion,
  Language,
  HomeHint,
  RetryHint,
  BackHint,
  OpenError,
  PageError,
  PagesRead,
  Count,
};

inline const char* tr(Text key, Language language) {
  static constexpr const char* EN[] = {
      "PagePet Reader", "Books", "Insert a microSD card", "Add .txt books to the card root",
      "PagePet", "Language: English", "UP/DOWN  select   OK  open   BACK  resume",
      "OK  retry card", "BACK  books", "Could not open this book",
      "Could not load this page", "Pages read: %lu",
  };
  static constexpr const char* RU[] = {
      "PagePet Reader", "Книги", "Вставьте карту microSD", "Добавьте книги .txt в корень карты",
      "PagePet", "Язык: Русский", "ВВЕРХ/ВНИЗ  выбор   OK  открыть   НАЗАД  читать",
      "OK  повторить", "НАЗАД  к книгам", "Не удалось открыть книгу",
      "Не удалось загрузить страницу", "Прочитано страниц: %lu",
  };
  static_assert(sizeof(EN) / sizeof(EN[0]) == static_cast<unsigned>(Text::Count));
  static_assert(sizeof(RU) / sizeof(RU[0]) == static_cast<unsigned>(Text::Count));
  const unsigned index = static_cast<unsigned>(key);
  return index < static_cast<unsigned>(Text::Count)
             ? (language == Language::Russian ? RU[index] : EN[index])
             : "";
}

}  // namespace strings
