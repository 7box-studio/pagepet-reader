// Compile the actual application in this translation unit. Hardware adapters
// live only on the native include path; the ESP32 build never sees them.
#include "../../src/main.cpp"

#include <iostream>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace {
bool sleeping = false;
bool busy = false;
uint32_t frameNumber = 0;

std::string jsonString(const char* value) {
  std::string out = "\"";
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
    if (*p == '\\' || *p == '"') { out += '\\'; out += *p; }
    else if (*p < 0x20) {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
      out += escaped;
    } else out += *p;
  }
  return out + '"';
}

const char* screenName() {
  switch (screen) {
    case Screen::Home: return "home";
    case Screen::Library: return "library";
    case Screen::Reading: return "reading";
    case Screen::ReadingSettings: return "settings";
    case Screen::Companion: return "companion";
    case Screen::Error: return "error";
  }
  return "unknown";
}

void emitFrame() {
  // Undo the same Portrait rotation that PageRenderer and DisplayTarget use.
  // PBM encodes black as 1; FreeInkDisplay encodes white as 1.
  constexpr int width = 528, height = 792, stride = width / 8;
  uint8_t portrait[stride * height]{};
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int panelX = y, panelY = width - 1 - x;
      if (!(display.presented[panelY * 99 + panelX / 8] & (0x80u >> (panelX & 7)))) {
        portrait[y * stride + x / 8] |= 0x80u >> (x & 7);
      }
    }
  }
  const std::string header = "P4\n528 792\n";
  std::cout << "{\"bytes\":" << header.size() + sizeof(portrait)
            << ",\"frame\":" << ++frameNumber
            << ",\"screen\":\"" << screenName() << '"'
            << ",\"sleeping\":" << (sleeping ? "true" : "false")
            << ",\"busy\":" << (busy ? "true" : "false")
            << ",\"card\":" << (hostsd::online ? "true" : "false")
            << ",\"selection\":" << selection << ",\"bookCount\":" << library.count()
            << ",\"libraryPath\":" << jsonString(library.path())
            << ",\"librarySelection\":" << library.selection()
            << ",\"libraryFirst\":" << library.first()
            << ",\"libraryCount\":" << library.count()
            << ",\"libraryRows\":[";
  for (size_t i = 0; i < library.rows(); ++i) {
    const auto* entry = library.row(i);
    if (i) std::cout << ',';
    std::cout << "{\"name\":" << jsonString(entry->name)
              << ",\"folder\":" << (entry->directory ? "true" : "false") << '}';
  }
  std::cout << ']'
            << ",\"book\":" << jsonString(reader.path())
            << ",\"chapter\":" << reader.spine() << ",\"chapters\":" << reader.spineCount()
            << ",\"page\":" << reader.page() << ",\"pages\":" << reader.pageCount()
            << ",\"character\":" << reader.characterOffset()
            << ",\"language\":" << static_cast<unsigned>(language)
            << ",\"textSize\":" << static_cast<unsigned>(textSize)
            << ",\"progressPage\":" << reader.progressPage()
            << ",\"petPages\":" << pet.state().valid_pages
            << ",\"refreshes\":" << display.refreshes
            << ",\"builds\":" << display.loans
            << ",\"sourceReads\":" << hostsd::sourceReads
            << ",\"battery\":" << chrome.battery().percentage
            << ",\"batteryKnown\":" << (chrome.battery().percentageKnown ? "true" : "false")
            << ",\"charging\":" << (chrome.battery().charging ? "true" : "false")
            << ",\"batteryReads\":" << hostbattery::reads
            << ",\"error\":" << jsonString(screen == Screen::Error ? errorText : "") << "}\n";
  std::cout.write(header.data(), header.size());
  std::cout.write(reinterpret_cast<const char*>(portrait), sizeof(portrait));
  std::cout.flush();
}

void shutdownApplication() {
  if (!sleeping && reader.path()[0]) saveState(true);
  savePet();
  reader.close();
}
}

int main(int argc, char** argv) {
  if (argc != 2 || !std::filesystem::is_directory(argv[1])) {
    std::cerr << "Usage: pagepet-simulator <existing-sd-folder>\n";
    return 2;
  }
  hostsd::root = std::filesystem::canonical(argv[1]);
  if (const char* battery = std::getenv("PAGEPET_SIM_BATTERY")) {
    if (std::string(battery) == "unknown") {
      hostbattery::sample.percentageKnown = false;
      hostbattery::sample.chargingKnown = false;
    } else {
      unsigned percentage = 82, charging = 0;
      if (sscanf(battery, "%u %u", &percentage, &charging) == 2) {
        hostbattery::sample.percentage = static_cast<uint16_t>(percentage);
        hostbattery::sample.charging = charging != 0;
      }
    }
  }
  display.onPresent = emitFrame;
  busy = true;
  setup();
  loop(); // release the boot-time power-button guard
  busy = false;
  emitFrame();
  std::string command;
  while (std::getline(std::cin, command)) {
    if (command == "quit") break;
    if (command == "restart" || (command == "power" && sleeping)) {
      shutdownApplication();
      execv(argv[0], argv); // wake resets globals exactly as a real MCU reset does
      perror("restart simulator");
      return 1;
    }
    busy = true;
    if (command == "card-off") hostsd::online = false;
    else if (command == "card-on") hostsd::online = true;
    else if (command == "advance") hostMillis += 30000;
    else if (command.rfind("battery ", 0) == 0 || command == "battery-fail") {
      unsigned percentage = 0, charging = 0;
      if (command == "battery-fail") {
        hostbattery::sample.percentageKnown = false;
        hostbattery::sample.chargingKnown = false;
        setenv("PAGEPET_SIM_BATTERY", "unknown", 1);
      } else if (sscanf(command.c_str(), "battery %u %u", &percentage, &charging) == 2) {
        hostbattery::sample.percentage = static_cast<uint16_t>(percentage);
        hostbattery::sample.percentageKnown = true;
        hostbattery::sample.chargingKnown = true;
        hostbattery::sample.charging = charging != 0;
        char value[24];
        snprintf(value, sizeof(value), "%u %u", percentage, charging != 0);
        setenv("PAGEPET_SIM_BATTERY", value, 1);
      }
      if (!sleeping) {
        chrome.invalidateBattery();
        drawChrome(screen);
        display.displayBuffer(freeink::FreeInkDisplay::FAST_REFRESH);
      }
    }
    else if (!sleeping) {
      if (command == "up") input.release(InputManager::BTN_UP);
      else if (command == "down") input.release(InputManager::BTN_DOWN);
      else if (command == "left") input.release(InputManager::BTN_LEFT);
      else if (command == "right") input.release(InputManager::BTN_RIGHT);
      else if (command == "ok") input.release(InputManager::BTN_CONFIRM);
      else if (command == "back") input.release(InputManager::BTN_BACK);
      else if (command == "power") input.release(InputManager::BTN_POWER);
    }
    if (!sleeping) {
      try { loop(); }
      catch (const freeink::SimulatedSleep&) { sleeping = true; }
    }
    busy = false;
    emitFrame();
  }
  shutdownApplication();
  return 0;
}
