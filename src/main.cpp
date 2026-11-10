#include <Arduino.h>
#include <BoardConfig.h>
#include <DragonSprite.h>
#include <FreeInkDisplay.h>
#include <FreeInkUIDisplayTarget.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <ReaderFont.h>
#include <SDCardManager.h>
#include <XteinkDetect.h>
#include <companion_engine.h>
#include <progression.h>

#include <cstdio>
#include <cstring>

#include "Storage.h"
#include "Strings.h"
#include "TextReader.h"
#include "BookFormat.h"
#include "ReadingState.h"
#include "ReaderChrome.h"
#include "LibraryBrowser.h"

namespace {
namespace ui = freeink::ui;

constexpr char STATE_PATH[] = "/.pagepet/state.dat";
constexpr char PET_PATH[] = "/.pagepet/pet.dat";
constexpr char SETTINGS_PATH[] = "/.pagepet/settings.dat";
constexpr uint32_t STATE_MAGIC = READING_STATE_MAGIC;
constexpr uint32_t STATE_VERSION = READING_STATE_VERSION;
constexpr uint32_t SETTINGS_MAGIC = 0x50504c31;
constexpr uint32_t SETTINGS_VERSION = 2;
enum HomeItem : uint8_t { ResumeItem, LibraryItem, PetItem, LanguageItem, HomeItemCount };

using SavedState = ReadingState;

struct SavedSettings {
  uint32_t magic;
  uint32_t version;
  uint32_t language;
  uint32_t textSize;
  uint32_t checksum;
};

uint32_t checksum(const void* data, size_t size) {
  uint32_t hash = 2166136261u;
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}

const auto& pins = BoardConfig::XTEINK_X3.display;
freeink::FreeInkDisplay display(pins.sclk, pins.mosi, pins.cs, pins.dc, pins.rst, pins.busy);
InputManager input;
SDCardManager& card = SDCardManager::getInstance();
TextReader reader;
ReaderChrome chrome;
LibraryBrowser library;
companion::CompanionEngine pet;

enum class Screen : uint8_t { Home, Library, Reading, ReadingSettings, Companion, Error };
Screen screen = Screen::Home;
bool cardReady = false;
size_t selection = LibraryItem;
SavedState saved{};
strings::Language language = strings::Language::English;
TextReader::TextSize textSize = TextReader::TextSize::Standard;
TextReader::TextSize pendingTextSize = TextReader::TextSize::Standard;
uint32_t lastStateWriteMs = 0;
bool wakePowerReleasePending = true;
char errorText[64]{};
char errorPath[sizeof(saved.path)]{};
bool errorCanRetry = false;
bool errorIsLibrary = false;

const char* uiText(strings::Text key) { return strings::tr(key, language); }

ui::DisplayTarget canvas() {
  ui::DisplayTarget target(display.getFrameBuffer(), display.getDisplayWidth(), display.getDisplayHeight(),
                           display.getDisplayWidthBytes());
  target.setFont(ui::kReaderFont);
  return target;
}

void label(ui::DisplayTarget& target, int x, int y, int w, const char* value,
           ui::TextAlign align = ui::TextAlign::Left) {
  const int available = target.logicalWidth() - x - 24;
  if (w > available) w = available;
  ui::TextStyle style{};
  style.align = align;
  target.text({static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), 30}, value, style);
}

bool previousReleased() {
  return input.wasReleased(InputManager::BTN_UP) || input.wasReleased(InputManager::BTN_LEFT);
}

bool nextReleased() {
  return input.wasReleased(InputManager::BTN_DOWN) || input.wasReleased(InputManager::BTN_RIGHT);
}

void drawChrome(Screen view) {
  ReaderButtonHints hints{};
  if (view == Screen::Home) {
    if (cardReady) {
      if (saved.path[0] && card.exists(saved.path)) hints.back = uiText(strings::Text::HintResume);
      hints.confirm = uiText(strings::Text::HintChoose);
      hints.previous = uiText(strings::Text::HintUp);
      hints.next = uiText(strings::Text::HintDown);
      hints.sides = true;
    } else hints.confirm = uiText(strings::Text::HintRetry);
  } else if (view == Screen::Library) {
    hints = {uiText(library.atRoot() ? strings::Text::HintHome : strings::Text::HintParent),
             library.count() ? uiText(strings::Text::HintOpen) : "",
             library.count() ? uiText(strings::Text::HintUp) : "",
             library.count() ? uiText(strings::Text::HintDown) : "", library.count() > 0};
  } else if (view == Screen::Reading) {
    hints = {uiText(strings::Text::HintBooks), uiText(strings::Text::HintText),
             uiText(strings::Text::HintPrevious), uiText(strings::Text::HintNext), true};
  } else if (view == Screen::ReadingSettings) {
    hints = {uiText(strings::Text::HintRead), uiText(strings::Text::HintApply),
             uiText(strings::Text::HintUp), uiText(strings::Text::HintDown), true};
  } else if (view == Screen::Companion) hints.back = uiText(strings::Text::HintHome);
  else if (view == Screen::Error) {
    hints.back = uiText(strings::Text::HintHome);
    if (errorCanRetry) hints.confirm = uiText(strings::Text::HintRetry);
  }
  chrome.draw(display, hints);
}

void showError(const char* message, bool canRetry = false, const char* path = nullptr) {
  errorIsLibrary = false;
  snprintf(errorText, sizeof(errorText), "%s", message);
  errorCanRetry = canRetry && path != nullptr && path[0] != '\0';
  if (errorCanRetry) snprintf(errorPath, sizeof(errorPath), "%s", path);
  else errorPath[0] = '\0';
  screen = Screen::Error;
  if (!display.getFrameBuffer()) return;
  display.clearScreen();
  auto target = canvas();
  label(target, 24, 48, 740, uiText(strings::Text::Title));
  label(target, 24, 150, 740, errorText);
  drawChrome(Screen::Error);
  display.displayBuffer(freeink::FreeInkDisplay::HALF_REFRESH);
}

bool canResume() { return cardReady && saved.path[0] && card.exists(saved.path); }

void smallLabel(ui::DisplayTarget& target, int x, int y, int width, const char* value,
                ui::TextAlign align = ui::TextAlign::Left) {
  target.setFont(ui::kReaderSmallFont);
  label(target, x, y, width, value, align);
  target.setFont(ui::kReaderFont);
}

void screenTitle(ui::DisplayTarget& target, const char* title) {
  label(target, 28, 34, 472, title);
  target.line({28, 78}, {500, 78}, 1, ui::Paint::solid(ui::Color::Black));
}

// A solid spine and outline stay legible on a one-bit panel without dithering
// the paper underneath the text. Keep room outside for the physical side keys.
void focusRow(ui::DisplayTarget& target, int16_t y, int16_t height) {
  const auto ink = ui::Paint::solid(ui::Color::Black);
  target.stroke({28, y, 472, height}, ink, 2);
  target.fill({28, y, 6, height}, ink);
}

void drawHome() {
  screen = Screen::Home;
  if (!display.getFrameBuffer()) return;
  display.clearScreen();
  auto target = canvas();
  screenTitle(target, uiText(strings::Text::Title));
  if (!cardReady) {
    label(target, 40, 138, 448, uiText(strings::Text::NoCard));
    smallLabel(target, 40, 184, 448, uiText(strings::Text::CardRetry));
  }
  else {
    const bool resumable = canResume();
    if (!resumable && selection == ResumeItem) selection = LibraryItem;
    constexpr int rowY[] = {126, 284, 368, 452};
    const strings::Text names[] = {strings::Text::ContinueReading, strings::Text::Library,
                                   strings::Text::Companion, strings::Text::Language};
    for (size_t i = 0; i < HomeItemCount; ++i) {
      if (i == ResumeItem) {
        target.stroke({28, 110, 472, 126}, ui::Paint::solid(ui::Color::Black), 1);
      }
      if (selection == i) focusRow(target, rowY[i] - 16, i == ResumeItem ? 126 : 64);
      label(target, 48, rowY[i], 432, uiText(names[i]));
    }
    const char* name = strrchr(saved.path, '/');
    smallLabel(target, 48, 176, 432, resumable ? (name ? name + 1 : saved.path)
                                            : uiText(strings::Text::NoResume));
  }
  drawChrome(Screen::Home);
  display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
}

void drawLibrary() {
  screen = Screen::Library;
  display.clearScreen();
  auto target = canvas();
  screenTitle(target, uiText(strings::Text::Library));
  smallLabel(target, 28, 96, 472, library.path());
  if (!library.count()) {
    label(target, 40, 170, 448, uiText(strings::Text::EmptyFolder));
    smallLabel(target, 40, 218, 448, uiText(strings::Text::AddBooks));
  }
  for (size_t row = 0; row < library.rows(); ++row) {
    const auto* entry = library.row(row);
    const int16_t y = 158 + row * 60;
    if (library.selection() == library.first() + row) {
      focusRow(target, y - 4, 48);
    }
    const auto ink = ui::Paint::solid(ui::Color::Black);
    if (entry->directory) {
      target.stroke({44, static_cast<int16_t>(y + 12), 20, 15}, ink, 1);
      target.stroke({44, static_cast<int16_t>(y + 8), 9, 5}, ink, 1);
    } else {
      target.stroke({46, static_cast<int16_t>(y + 7), 15, 22}, ink, 1);
      target.line({49, static_cast<int16_t>(y + 14)}, {58, static_cast<int16_t>(y + 14)}, 1, ink);
      target.line({49, static_cast<int16_t>(y + 19)}, {58, static_cast<int16_t>(y + 19)}, 1, ink);
    }
    label(target, 76, y + 3, 412, entry->name);
  }
  if (library.count()) {
    // Repeat the selected name in a smaller, two-line area so long filenames
    // can be distinguished without changing the eight-row browsing window.
    if (const auto* entry = library.selectedEntry()) {
      target.setFont(ui::kReaderSmallFont);
      ui::TextStyle detail{};
      detail.maxLines = 2;
      target.text({40, 650, 448, 48}, entry->name, detail);
      target.setFont(ui::kReaderFont);
    }
    char range[48];
    snprintf(range, sizeof(range), "%lu-%lu / %lu", static_cast<unsigned long>(library.first() + 1),
             static_cast<unsigned long>(library.first() + library.rows()),
             static_cast<unsigned long>(library.count()));
    smallLabel(target, 24, 706, 480, range, ui::TextAlign::Center);
  }
  drawChrome(Screen::Library);
  display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
}

void showLibraryError(LibraryBrowser::Status status) {
  if (status == LibraryBrowser::Status::PathTooLong) showError(uiText(strings::Text::PathTooLong));
  else {
    showError(uiText(strings::Text::FolderError), true, library.path());
    errorIsLibrary = true;
  }
}

void openLibrary() {
  const auto status = library.refresh(card);
  if (status == LibraryBrowser::Status::Ok) drawLibrary();
  else showLibraryError(status);
}

bool readState(const char* path) {
  FsFile file = card.open(path, O_RDONLY);
  uint8_t bytes[sizeof(SavedState)];
  SavedState candidate{};
  if (!file || file.size() > sizeof(bytes)) return false;
  const size_t size = file.size();
  if (file.read(bytes, size) != static_cast<int32_t>(size) ||
      !decodeReadingState(bytes, size, &candidate)) return false;
  saved = candidate;
  return true;
}

void loadState() {
  if (!readState(STATE_PATH)) {
    if (!readState("/.pagepet/state.dat.bak")) saved = {};
  }
}

bool readSettings(const char* path) {
  FsFile file = card.open(path, O_RDONLY);
  SavedSettings candidate{};
  if (!file || file.size() != sizeof(candidate) ||
      file.read(&candidate, sizeof(candidate)) != sizeof(candidate) ||
      candidate.magic != SETTINGS_MAGIC || candidate.version != SETTINGS_VERSION || candidate.language > 1 ||
      candidate.textSize > static_cast<uint32_t>(TextReader::TextSize::Standard) ||
      candidate.checksum != checksum(&candidate, offsetof(SavedSettings, checksum))) return false;
  language = static_cast<strings::Language>(candidate.language);
  textSize = static_cast<TextReader::TextSize>(candidate.textSize);
  return true;
}

void loadSettings() {
  if (!readSettings(SETTINGS_PATH)) readSettings("/.pagepet/settings.dat.bak");
}

void saveSettings() {
  if (!cardReady) return;
  SavedSettings value{};
  value.magic = SETTINGS_MAGIC;
  value.version = SETTINGS_VERSION;
  value.language = static_cast<uint32_t>(language);
  value.textSize = static_cast<uint32_t>(textSize);
  value.checksum = checksum(&value, offsetof(SavedSettings, checksum));
  if (!writeAtomic(card, SETTINGS_PATH, &value, sizeof(value))) {
    Serial.printf("[settings] save failed\n");
  }
}

void saveState(bool force = false) {
  if (!cardReady || (screen != Screen::Reading && screen != Screen::ReadingSettings)) return;
  if (!force && millis() - lastStateWriteMs < 15000) return;
  saved.magic = STATE_MAGIC;
  saved.version = STATE_VERSION;
  saved.characterOffset = reader.characterOffset();
  saved.spine = reader.spine();
  snprintf(saved.path, sizeof(saved.path), "%s", reader.path());
  saved.checksum = checksum(&saved, offsetof(SavedState, checksum));
  if (writeAtomic(card, STATE_PATH, &saved, sizeof(saved))) lastStateWriteMs = millis();
}

void loadPet() {
  uint8_t bytes[companion::CompanionEngine::kRecordSize];
  FsFile file = card.open(PET_PATH, O_RDONLY);
  if (file && file.size() == sizeof(bytes) && file.read(bytes, sizeof(bytes)) == sizeof(bytes) &&
      pet.decode(bytes, sizeof(bytes))) return;
  FsFile backup = card.open("/.pagepet/pet.dat.bak", O_RDONLY);
  if (backup && backup.size() == sizeof(bytes) && backup.read(bytes, sizeof(bytes)) == sizeof(bytes)) {
    if (!pet.decode(bytes, sizeof(bytes))) Serial.printf("[pet] invalid state\n");
  }
}

void savePet() {
  if (!pet.dirty() || !cardReady) return;
  uint8_t bytes[companion::CompanionEngine::kRecordSize];
  if (pet.encode(bytes, sizeof(bytes)) && writeAtomic(card, PET_PATH, bytes, sizeof(bytes))) pet.markFlushed();
}

void petRenderedPage() {
  companion::PageTurnEvent event{};
  event.book_id = bookIdFor(reader.path());
  event.page_index = reader.progressPage();
  event.monotonic_ms = millis();
  event.local_day = -1;
  event.local_hour = 255;
  event.weekday = 255;
  pet.onPageTurn(event);
  if (pet.flushDue(event.monotonic_ms)) savePet();
}

bool mountCard() {
  cardReady = card.begin();
  if (!cardReady) return false;
  if (!card.mkdir("/.pagepet", true) && !card.exists("/.pagepet")) {
    cardReady = false;
    return false;
  }
  loadSettings();
  loadState();
  loadPet();
  selection = canResume() ? ResumeItem : LibraryItem;
  return true;
}

void openBook(const char* path) {
  char absolute[128];
  if (snprintf(absolute, sizeof(absolute), "%s%s", path[0] == '/' ? "" : "/", path) >= static_cast<int>(sizeof(absolute))) {
    showError(uiText(strings::Text::OpenError));
    return;
  }
  const uint32_t offset = strcmp(saved.path, absolute) == 0 ? saved.characterOffset : 0;
  const uint32_t spine = strcmp(saved.path, absolute) == 0 ? saved.spine : 0;
  if (!reader.open(card, display, absolute, offset, display.getDisplayHeight(), display.getDisplayWidth(), textSize, spine)) {
    const auto status = reader.status();
    const auto key = status == freeink::book::BookStatus::Encrypted ? strings::Text::EncryptedBook
                   : status == freeink::book::BookStatus::OutOfMemory ? strings::Text::BookTooComplex
                   : strings::Text::OpenError;
    showError(uiText(key), true, absolute);
    return;
  }
  pet.beginSession(millis());
  screen = Screen::Reading;
  if (!reader.render(display)) {
    char failedPath[sizeof(saved.path)]{};
    snprintf(failedPath, sizeof(failedPath), "%s", reader.path());
    reader.close();
    showError(uiText(strings::Text::PageError), true, failedPath);
    return;
  }
  petRenderedPage();
  saveState(true);
}

const char* textSizeLabel(TextReader::TextSize value) {
  return uiText(value == TextReader::TextSize::Small ? strings::Text::TextSizeSmall
                                                     : strings::Text::TextSizeStandard);
}

void drawReadingSettings() {
  screen = Screen::ReadingSettings;
  display.clearScreen();
  auto target = canvas();
  screenTitle(target, uiText(strings::Text::Settings));
  smallLabel(target, 40, 108, 448, uiText(strings::Text::TextSize));
  const TextReader::TextSize sizes[] = {TextReader::TextSize::Small, TextReader::TextSize::Standard};
  for (size_t i = 0; i < 2; ++i) {
    const int16_t y = 154 + i * 76;
    if (pendingTextSize == sizes[i]) focusRow(target, y, 64);
    label(target, 48, y + 16, 300, textSizeLabel(sizes[i]));
    if (textSize == sizes[i]) {
      smallLabel(target, 348, y + 19, 132, uiText(strings::Text::CurrentSize), ui::TextAlign::Right);
    }
  }
  smallLabel(target, 40, 338, 448, uiText(strings::Text::Preview));
  target.line({40, 378}, {488, 378}, 1, ui::Paint::solid(ui::Color::Black));
  target.setFont(pendingTextSize == TextReader::TextSize::Small ? ui::kReaderSmallFont : ui::kReaderFont);
  ui::TextStyle preview{};
  preview.maxLines = 6;
  target.text({40, 402, 448, 200}, uiText(strings::Text::PreviewPassage), preview);
  target.setFont(ui::kReaderFont);
  smallLabel(target, 40, 664, 448, uiText(strings::Text::PositionKept));
  drawChrome(Screen::ReadingSettings);
  display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
}

bool applyTextSize() {
  saveState(true);
  const uint32_t offset = reader.characterOffset();
  const uint32_t spine = reader.spine();
  char path[sizeof(saved.path)]{};
  snprintf(path, sizeof(path), "%s", reader.path());
  reader.close();
  if (!reader.open(card, display, path, offset, display.getDisplayHeight(), display.getDisplayWidth(), pendingTextSize, spine)) {
    showError(uiText(strings::Text::OpenError), true, path);
    return false;
  }
  textSize = pendingTextSize;
  pet.beginSession(millis());
  saveSettings();
  screen = Screen::Reading;
  if (!reader.render(display)) {
    char failedPath[sizeof(saved.path)]{};
    snprintf(failedPath, sizeof(failedPath), "%s", reader.path());
    reader.close();
    showError(uiText(strings::Text::PageError), true, failedPath);
    return false;
  }
  saveState(true);
  return true;
}

void closeBook() {
  saveState(true);
  savePet();
  char path[sizeof(saved.path)];
  snprintf(path, sizeof(path), "%s", reader.path());
  reader.close();
  const auto status = library.focusBook(card, path);
  if (status == LibraryBrowser::Status::Ok) drawLibrary();
  else showLibraryError(status);
}

void retryError() {
  char path[sizeof(errorPath)]{};
  snprintf(path, sizeof(path), "%s", errorPath);
  const bool retryLibrary = errorIsLibrary;
  errorCanRetry = false;
  reader.close();
  card.shutdown();
  cardReady = false;
  if (!mountCard()) {
    showError(uiText(strings::Text::NoCard));
    return;
  }
  if (retryLibrary) openLibrary();
  else openBook(path);
}

void drawCompanion() {
  screen = Screen::Companion;
  display.clearScreen();
  auto target = canvas();
  label(target, 24, 24, 740, uiText(strings::Text::Companion));
  // Use the same logical portrait coordinates as the labels. Mask1 matches
  // the sprite's 1=white polarity and reads it directly from flash, with no
  // rotated image allocation or second framebuffer.
  const int16_t spriteX = (target.logicalWidth() - dragon_art::kWidth) / 2;
  target.bitmap({spriteX, 70, dragon_art::kWidth, dragon_art::kHeight},
                {dragon_art::kBitmap, dragon_art::kWidth, dragon_art::kHeight,
                 ui::BitmapFormat::Mask1, true}, ui::BitmapMode::Center);
  const int contentWidth = target.logicalWidth() - 48;
  char progress[64];
  snprintf(progress, sizeof(progress), uiText(strings::Text::PagesRead),
           static_cast<unsigned long>(pet.state().valid_pages));
  label(target, 24, 340, contentWidth, progress, ui::TextAlign::Center);
  snprintf(progress, sizeof(progress), uiText(strings::Text::PetLevel),
           static_cast<unsigned>(companion::levelForXp(pet.state().total_xp)));
  label(target, 24, 376, contentWidth, progress, ui::TextAlign::Center);
  snprintf(progress, sizeof(progress), uiText(strings::Text::PetXp),
           static_cast<unsigned long>(pet.state().total_xp));
  label(target, 24, 412, contentWidth, progress, ui::TextAlign::Center);
  snprintf(progress, sizeof(progress), uiText(strings::Text::SessionPages),
           static_cast<unsigned>(pet.state().session_pages));
  label(target, 24, 448, contentWidth, progress, ui::TextAlign::Center);
  drawChrome(Screen::Companion);
  display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
}

void enterSleep() {
  if (screen == Screen::Reading || screen == Screen::ReadingSettings) saveState(true);
  savePet();
  reader.close();
  display.deepSleep();
  card.shutdown();
  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  input.begin();
  reader.setRenderCallback([] { drawChrome(Screen::Reading); });
  reader.setBuildCallback([] {
    display.clearScreen();
    auto target = canvas();
    label(target, 24, 180, 740, uiText(strings::Text::Rebuilding), ui::TextAlign::Center);
    chrome.draw(display, {});
    display.displayBuffer(freeink::FreeInkDisplay::HALF_REFRESH);
  });
  freeink::applyXteinkDisplayController();
  display.setDisplayX3();
  display.begin();
  if (!display.getFrameBuffer()) {
    Serial.printf("[boot] framebuffer unavailable\n");
    return;
  }
  mountCard();
  drawHome();
  Serial.printf("[boot] free=%u largest=%u\n", static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
}

void loop() {
  input.update();
  if (wakePowerReleasePending) {
    if (!input.isPowerButtonPhysicallyPressed()) wakePowerReleasePending = false;
  } else if (input.wasReleased(InputManager::BTN_POWER)) {
    enterSleep();
  }
  if (screen == Screen::Home) {
    if (!cardReady) {
      if (input.wasReleased(InputManager::BTN_CONFIRM) && mountCard()) drawHome();
    } else if (previousReleased() || nextReleased()) {
      const bool forward = nextReleased();
      selection = forward ? (selection + 1) % HomeItemCount
                          : (selection + HomeItemCount - 1) % HomeItemCount;
      if (selection == ResumeItem && !canResume()) selection = forward ? LibraryItem : LanguageItem;
      drawHome();
    } else if (input.wasReleased(InputManager::BTN_CONFIRM)) {
      if (selection == LanguageItem) {
        language = language == strings::Language::English ? strings::Language::Russian : strings::Language::English;
        saveSettings();
        drawHome();
      } else if (selection == PetItem) drawCompanion();
      else if (selection == LibraryItem) openLibrary();
      else if (canResume()) openBook(saved.path);
    } else if (input.wasReleased(InputManager::BTN_BACK) && canResume()) openBook(saved.path);
  } else if (screen == Screen::Library) {
    if (input.wasReleased(InputManager::BTN_BACK)) {
      if (library.atRoot()) drawHome();
      else {
        const auto status = library.parent(card);
        if (status == LibraryBrowser::Status::Ok) drawLibrary();
        else showLibraryError(status);
      }
    } else if (previousReleased() || nextReleased()) {
      const auto status = library.move(card, nextReleased());
      if (status == LibraryBrowser::Status::Ok) drawLibrary();
      else showLibraryError(status);
    } else if (input.wasReleased(InputManager::BTN_CONFIRM) && library.count()) {
      const auto* entry = library.selectedEntry();
      if (entry && entry->directory) {
        const auto status = library.enter(card);
        if (status == LibraryBrowser::Status::Ok) drawLibrary();
        else showLibraryError(status);
      } else {
        char path[LibraryBrowser::PATH_BYTES];
        const auto status = library.selectedPath(path, sizeof(path));
        if (status == LibraryBrowser::Status::Ok) openBook(path);
        else showLibraryError(status);
      }
    }
  } else if (screen == Screen::Reading) {
    if (input.wasReleased(InputManager::BTN_BACK)) {
      closeBook();
    } else if (input.wasReleased(InputManager::BTN_CONFIRM)) {
      pendingTextSize = textSize;
      drawReadingSettings();
    } else if (previousReleased()) {
      const auto move = reader.previous(display);
      if (move == TextReader::Move::Error || (move == TextReader::Move::Moved && !reader.render(display))) {
        saveState(true);
        showError(uiText(strings::Text::PageError), true, reader.path());
      }
      saveState();
    } else if (nextReleased()) {
      const auto move = reader.next(display);
      if (move == TextReader::Move::Error) {
        saveState(true);
        showError(uiText(strings::Text::PageError), true, reader.path());
      } else if (move == TextReader::Move::Moved) {
        if (reader.render(display)) petRenderedPage();
        else {
          saveState(true);
          showError(uiText(strings::Text::PageError), true, reader.path());
        }
        saveState();
      }
    }
  } else if (screen == Screen::ReadingSettings) {
    if (input.wasReleased(InputManager::BTN_BACK)) {
      screen = Screen::Reading;
      if (!reader.render(display)) showError(uiText(strings::Text::PageError), true, reader.path());
    } else if (previousReleased() || nextReleased()) {
      pendingTextSize = pendingTextSize == TextReader::TextSize::Small ? TextReader::TextSize::Standard
                                                                        : TextReader::TextSize::Small;
      drawReadingSettings();
    } else if (input.wasReleased(InputManager::BTN_CONFIRM)) {
      applyTextSize();
    }
  } else if (screen == Screen::Error) {
    if (input.wasReleased(InputManager::BTN_CONFIRM) && errorCanRetry) {
      retryError();
    } else if (input.wasReleased(InputManager::BTN_BACK)) {
      reader.close();
      if (errorIsLibrary) library.reset();
      drawHome();
    }
  } else if (input.wasReleased(InputManager::BTN_BACK)) {
    reader.close();
    drawHome();
  }
  delay(10);
}
