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
  Resume,
  NoResume,
  HomeHint,
  RetryHint,
  BackHint,
  OpenError,
  PageError,
  PagesRead,
  Settings,
  TextSize,
  TextSizeSmall,
  TextSizeStandard,
  SettingsHint,
  Rebuilding,
  ErrorHint,
  Count,
};

inline const char* tr(Text key, Language language) {
  static constexpr const char* EN[] = {
      "PagePet Reader", "Books", "Insert a microSD card", "Add .txt books to the card root",
      "PagePet", "Language: English", "Resume: %s", "No saved book",
      "UP/DOWN  select   OK  open   BACK  resume", "OK  retry card", "BACK  books", "Could not open this book",
      "Could not load this page", "Pages read: %lu", "Reading settings", "Text size", "Small",
      "Standard", "UP/DOWN  change   OK  apply   BACK  reading", "Rebuilding pages...",
      "OK  retry   BACK  home",
  };
  static constexpr const char* RU[] = {
      "PagePet Reader", "Книги", "Вставьте карту microSD", "Добавьте книги .txt в корень карты",
      "PagePet", "Язык: Русский", "Продолжить: %s", "Нет сохранённой книги",
      "ВВЕРХ/ВНИЗ  выбор   OK  открыть   НАЗАД  читать", "OK  повторить", "НАЗАД  к книгам", "Не удалось открыть книгу",
      "Не удалось загрузить страницу", "Прочитано страниц: %lu", "Настройки чтения", "Размер текста",
      "Мелкий", "Стандартный", "ВВЕРХ/ВНИЗ  изменить   OK  применить   НАЗАД  книга", "Перестраиваем страницы...",
      "OK  повторить   НАЗАД  домой",
  };
  static_assert(sizeof(EN) / sizeof(EN[0]) == static_cast<unsigned>(Text::Count));
  static_assert(sizeof(RU) / sizeof(RU[0]) == static_cast<unsigned>(Text::Count));
  const unsigned index = static_cast<unsigned>(key);
  return index < static_cast<unsigned>(Text::Count)
             ? (language == Language::Russian ? RU[index] : EN[index])
             : "";
}

}  // namespace strings
