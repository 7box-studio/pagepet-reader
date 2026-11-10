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
  OpenError,
  PageError,
  PagesRead,
  PetLevel,
  PetXp,
  SessionPages,
  Settings,
  TextSize,
  TextSizeSmall,
  TextSizeStandard,
  Rebuilding,
  EncryptedBook,
  BookTooComplex,
  HintResume,
  HintChoose,
  HintUp,
  HintDown,
  HintBooks,
  HintText,
  HintPrevious,
  HintNext,
  HintRead,
  HintApply,
  HintHome,
  HintRetry,
  Library,
  ContinueReading,
  EmptyFolder,
  FolderError,
  PathTooLong,
  HintParent,
  HintOpen,
  CurrentSize,
  Preview,
  PreviewPassage,
  PositionKept,
  AddBooks,
  CardRetry,
  Count,
};

inline const char* tr(Text key, Language language) {
  static constexpr const char* EN[] = {
      "PagePet Reader", "Books", "Insert a microSD card", "Add .txt or .epub books to the card",
      "PagePet", "Language: English", "Resume: %s", "No saved book",
      "Could not open this book",
      "Could not load this page", "Pages read: %lu", "Level: %u", "XP: %lu", "This session: %u", "Reading settings", "Text size", "Small",
      "Standard", "Rebuilding pages...",
      "DRM-protected books are unsupported", "This book exceeds the memory limit",
      "Resume", "Select", "Up", "Down", "Books", "Text", "Prev", "Next",
      "Read", "Apply", "Home", "Retry",
      "Library", "Continue reading", "No books in this folder", "Could not read this folder",
      "This path is too long", "Up folder", "Open",
      "Current", "Text preview",
      "The book lay open by the window. Outside, the trees moved in the wind. "
      "There was time for one more page.",
      "Your reading position will be kept.", "Add TXT or EPUB files here.",
      "Then press Retry.",
  };
  static constexpr const char* RU[] = {
      "PagePet Reader", "Книги", "Вставьте карту microSD", "Добавьте .txt или .epub на карту",
      "PagePet", "Язык: Русский", "Продолжить: %s", "Нет сохранённой книги",
      "Не удалось открыть книгу",
      "Не удалось загрузить страницу", "Прочитано страниц: %lu", "Уровень: %u", "Опыт: %lu", "За сеанс: %u", "Настройки чтения", "Размер текста",
      "Мелкий", "Стандартный", "Перестраиваем страницы...",
      "Книги с DRM не поддерживаются", "Книге не хватает памяти",
      "Читать", "Выбор", "Вверх", "Вниз", "Книги", "Текст", "Назад", "Вперёд",
      "Читать", "Принять", "Домой", "Повтор",
      "Библиотека", "Продолжить чтение", "В папке нет книг", "Не удалось прочитать папку",
      "Слишком длинный путь", "Выше", "Открыть",
      "Сейчас", "Образец текста",
      "Книга лежала у окна. За стеклом ветер тихо шевелил ветви деревьев. "
      "Можно было прочитать ещё одну страницу.",
      "Место в книге сохранится.", "Добавьте сюда файлы TXT или EPUB.",
      "Затем нажмите «Повтор».",
  };
  static_assert(sizeof(EN) / sizeof(EN[0]) == static_cast<unsigned>(Text::Count));
  static_assert(sizeof(RU) / sizeof(RU[0]) == static_cast<unsigned>(Text::Count));
  const unsigned index = static_cast<unsigned>(key);
  return index < static_cast<unsigned>(Text::Count)
             ? (language == Language::Russian ? RU[index] : EN[index])
             : "";
}

}  // namespace strings
