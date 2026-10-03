#include <Arduino.h>
#include <BoardConfig.h>
#include <FreeInkDisplay.h>
#include <FreeInkUIDisplayTarget.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <ReaderFont.h>
#include <SDCardManager.h>
#include <XteinkDetect.h>
#include <character_art.h>
#include <companion_engine.h>

#include <cstdio>
#include <cstring>

#include "Storage.h"
#include "Strings.h"
#include "TextReader.h"

namespace {
namespace ui = freeink::ui;

constexpr char STATE_PATH[] = "/.pagepet/state.dat";
constexpr char PET_PATH[] = "/.pagepet/pet.dat";
constexpr char SETTINGS_PATH[] = "/.pagepet/settings.dat";
constexpr uint32_t STATE_MAGIC = 0x50505431;
constexpr uint32_t STATE_VERSION = 1;
constexpr uint32_t SETTINGS_MAGIC = 0x50504c31;
constexpr size_t MAX_BOOKS = 14;
constexpr size_t VISIBLE_ITEMS = 8;
constexpr size_t NAME_BYTES = 96;

struct SavedState {
  uint32_t magic;
  uint32_t version;
  uint32_t characterOffset;
  char path[128];
  uint32_t checksum;
};

struct SavedSettings {
  uint32_t magic;
  uint32_t version;
  uint32_t language;
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
companion::CompanionEngine pet;

enum class Screen : uint8_t { Home, Reading, Companion, Error };
Screen screen = Screen::Home;
bool cardReady = false;
char bookNames[MAX_BOOKS][NAME_BYTES]{};
size_t bookCount = 0;
size_t selection = 0;
SavedState saved{};
strings::Language language = strings::Language::English;
uint32_t lastStateWriteMs = 0;
bool wakePowerReleasePending = true;
char errorText[64]{};

const char* uiText(strings::Text key) { return strings::tr(key, language); }

ui::DisplayTarget canvas() {
  ui::DisplayTarget target(display.getFrameBuffer(), display.getDisplayWidth(), display.getDisplayHeight(),
                           display.getDisplayWidthBytes());
  target.setFont(ui::kReaderFont);
  return target;
}

void label(ui::DisplayTarget& target, int x, int y, int w, const char* value,
           ui::TextAlign align = ui::TextAlign::Left) {
  ui::TextStyle style{};
  style.align = align;
  target.text({static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), 30}, value, style);
}

void showError(const char* message) {
  snprintf(errorText, sizeof(errorText), "%s", message);
  screen = Screen::Error;
  if (!display.getFrameBuffer()) return;
  display.clearScreen();
  auto target = canvas();
  label(target, 24, 48, 740, uiText(strings::Text::Title));
  label(target, 24, 150, 740, errorText);
  label(target, 24, 482, 740, uiText(strings::Text::BackHint));
  display.displayBuffer(freeink::FreeInkDisplay::HALF_REFRESH);
}

bool hasTxtExtension(const char* name) {
  const size_t len = strlen(name);
  if (len < 4) return false;
  const char* suffix = name + len - 4;
  return (suffix[0] == '.' && (suffix[1] == 't' || suffix[1] == 'T') &&
          (suffix[2] == 'x' || suffix[2] == 'X') && (suffix[3] == 't' || suffix[3] == 'T'));
}

void scanBooks() {
  bookCount = 0;
  FsFile root = card.open("/", O_RDONLY);
  if (!root || !root.isDirectory()) return;
  root.rewindDirectory();
  for (FsFile entry = root.openNextFile(); entry && bookCount < MAX_BOOKS; entry = root.openNextFile()) {
    if (entry.isDirectory()) continue;
    char name[NAME_BYTES]{};
    if (!entry.getName(name, sizeof(name)) || !hasTxtExtension(name)) continue;
    memcpy(bookNames[bookCount++], name, sizeof(name));
  }
  if (selection > bookCount + 1) selection = 0;
}

void drawHome() {
  screen = Screen::Home;
  if (!display.getFrameBuffer()) return;
  display.clearScreen();
  auto target = canvas();
  label(target, 24, 24, 740, uiText(strings::Text::Title));
  target.line({24, 60}, {768, 60}, 1, ui::Paint::solid(ui::Color::Black));
  if (!cardReady) {
    label(target, 24, 118, 740, uiText(strings::Text::NoCard));
    label(target, 24, 482, 740, uiText(strings::Text::RetryHint));
  } else {
    label(target, 24, 82, 740, uiText(strings::Text::Books));
    if (bookCount == 0) label(target, 24, 220, 740, uiText(strings::Text::NoBooks));
    const size_t first = (selection / VISIBLE_ITEMS) * VISIBLE_ITEMS;
    for (size_t i = first; i < bookCount && i < first + VISIBLE_ITEMS; ++i) {
      const int y = 128 + static_cast<int>(i - first) * 40;
      if (i == selection) target.fill({18, static_cast<int16_t>(y - 2), 750, 34},
                                      ui::Paint::solid(ui::Color::LightGray));
      label(target, 28, y, 720, bookNames[i]);
    }
    if (bookCount >= first && bookCount < first + VISIBLE_ITEMS) {
      const int petY = 128 + static_cast<int>(bookCount - first) * 40;
      if (selection == bookCount) target.fill({18, static_cast<int16_t>(petY - 2), 750, 34},
                                               ui::Paint::solid(ui::Color::LightGray));
      label(target, 28, petY, 720, uiText(strings::Text::Companion));
    }
    if (bookCount + 1 >= first && bookCount + 1 < first + VISIBLE_ITEMS) {
      const int languageY = 128 + static_cast<int>(bookCount + 1 - first) * 40;
      if (selection == bookCount + 1) target.fill({18, static_cast<int16_t>(languageY - 2), 750, 34},
                                                   ui::Paint::solid(ui::Color::LightGray));
      label(target, 28, languageY, 720, uiText(strings::Text::Language));
    }
    label(target, 12, 482, 760, uiText(strings::Text::HomeHint));
  }
  display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
}

bool readState(const char* path) {
  FsFile file = card.open(path, O_RDONLY);
  SavedState candidate{};
  if (!file || file.size() != sizeof(candidate) || file.read(&candidate, sizeof(candidate)) != sizeof(candidate) ||
      candidate.magic != STATE_MAGIC || candidate.version != STATE_VERSION ||
      candidate.checksum != checksum(&candidate, offsetof(SavedState, checksum)) ||
      !memchr(candidate.path, '\0', sizeof(candidate.path))) return false;
  saved = candidate;
  return true;
}

void loadState() {
  if (!readState(STATE_PATH)) {
    if (!readState("/.pagepet/state.dat.bak")) saved = {};
  }
}

bool readLanguage(const char* path) {
  FsFile file = card.open(path, O_RDONLY);
  SavedSettings candidate{};
  if (!file || file.size() != sizeof(candidate) ||
      file.read(&candidate, sizeof(candidate)) != sizeof(candidate) ||
      candidate.magic != SETTINGS_MAGIC || candidate.version != 1 || candidate.language > 1 ||
      candidate.checksum != checksum(&candidate, offsetof(SavedSettings, checksum))) return false;
  language = static_cast<strings::Language>(candidate.language);
  return true;
}

void loadLanguage() {
  if (!readLanguage(SETTINGS_PATH)) readLanguage("/.pagepet/settings.dat.bak");
}

void saveLanguage() {
  if (!cardReady) return;
  SavedSettings value{};
  value.magic = SETTINGS_MAGIC;
  value.version = 1;
  value.language = static_cast<uint32_t>(language);
  value.checksum = checksum(&value, offsetof(SavedSettings, checksum));
  if (!writeAtomic(card, SETTINGS_PATH, &value, sizeof(value))) {
    Serial.printf("[settings] language save failed\n");
  }
}

void saveState(bool force = false) {
  if (!cardReady || screen != Screen::Reading) return;
  if (!force && millis() - lastStateWriteMs < 15000) return;
  saved.magic = STATE_MAGIC;
  saved.version = STATE_VERSION;
  saved.characterOffset = reader.characterOffset();
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
  event.page_index = reader.page();
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
  loadLanguage();
  loadState();
  loadPet();
  scanBooks();
  return true;
}

void openBook(const char* path) {
  char absolute[128];
  if (snprintf(absolute, sizeof(absolute), "/%s", path) >= static_cast<int>(sizeof(absolute))) {
    showError(uiText(strings::Text::OpenError));
    return;
  }
  const uint32_t offset = strcmp(saved.path, absolute) == 0 ? saved.characterOffset : 0;
  if (!reader.open(card, display, absolute, offset, display.getDisplayHeight(), display.getDisplayWidth())) {
    showError(uiText(strings::Text::OpenError));
    return;
  }
  pet.beginSession(millis());
  screen = Screen::Reading;
  if (!reader.render(display)) {
    reader.close();
    showError(uiText(strings::Text::PageError));
    return;
  }
  petRenderedPage();
  saveState(true);
}

void closeBook() {
  saveState(true);
  savePet();
  reader.close();
  scanBooks();
  drawHome();
}

void drawCompanion() {
  screen = Screen::Companion;
  display.clearScreen();
  auto target = canvas();
  label(target, 24, 24, 740, uiText(strings::Text::Companion));
  uint8_t sprite[companion::kSpriteBytes]{};
  if (companion::renderCharacter(pet.state().character_class, sprite, sizeof(sprite))) {
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 16; ++x) {
        if (sprite[y * 2 + x / 8] & (0x80u >> (x & 7))) {
          target.fill({static_cast<int16_t>(348 + x * 6), static_cast<int16_t>(140 + y * 6), 6, 6},
                      ui::Paint::solid(ui::Color::Black));
        }
      }
    }
  }
  char progress[64];
  snprintf(progress, sizeof(progress), uiText(strings::Text::PagesRead),
           static_cast<unsigned long>(pet.state().valid_pages));
  label(target, 24, 306, 740, progress, ui::TextAlign::Center);
  label(target, 24, 482, 740, uiText(strings::Text::BackHint));
  display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
}

void enterSleep() {
  if (screen == Screen::Reading) saveState(true);
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
    } else if (input.wasReleased(InputManager::BTN_UP)) {
      selection = selection == 0 ? bookCount + 1 : selection - 1;
      drawHome();
    } else if (input.wasReleased(InputManager::BTN_DOWN)) {
      selection = selection >= bookCount + 1 ? 0 : selection + 1;
      drawHome();
    } else if (input.wasReleased(InputManager::BTN_CONFIRM)) {
      if (selection == bookCount + 1) {
        language = language == strings::Language::English ? strings::Language::Russian
                                                            : strings::Language::English;
        saveLanguage();
        drawHome();
      } else if (selection == bookCount) drawCompanion();
      else openBook(bookNames[selection]);
    } else if (input.wasReleased(InputManager::BTN_BACK) && saved.path[0]) {
      openBook(saved.path[0] == '/' ? saved.path + 1 : saved.path);
    }
  } else if (screen == Screen::Reading) {
    if (input.wasReleased(InputManager::BTN_BACK)) {
      closeBook();
    } else if (input.wasReleased(InputManager::BTN_UP)) {
      if (reader.previous() && !reader.render(display)) showError(uiText(strings::Text::PageError));
      saveState();
    } else if (input.wasReleased(InputManager::BTN_DOWN)) {
      if (reader.next()) {
        if (reader.render(display)) petRenderedPage();
        else showError(uiText(strings::Text::PageError));
        saveState();
      }
    }
  } else if (input.wasReleased(InputManager::BTN_BACK)) {
    reader.close();
    drawHome();
  }
  delay(10);
}
