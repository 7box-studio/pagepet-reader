#include "ReaderChrome.h"

#include <Arduino.h>
#include <FreeInkUIDisplayTarget.h>
#include <ReaderSmallFont.h>
#include <components/bars/battery-indicator.h>
#include <cstdio>

namespace {
namespace ui = freeink::ui;

void buttonHint(ui::DisplayTarget& target, int16_t x, const char* label) {
  if (!label || !label[0]) return;
  const int16_t y = target.logicalHeight() - 40;
  const ui::Rect box{x, y, 106, 40};
  target.fill(box, ui::Paint::solid(ui::Color::White));
  target.stroke(box, ui::Paint::solid(ui::Color::Black), 1);
  ui::TextStyle text{};
  text.align = ui::TextAlign::Center;
  target.text({static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 9), 98, 22}, label, text);
}

void sideHint(ui::DisplayTarget& target, bool next) {
  // Crosspoint's X3 edge-button position, fitted inside our 26px text margins.
  const int16_t x = next ? target.logicalWidth() - 24 : 4;
  constexpr int16_t y = 155;
  target.fill({x, y, 20, 80}, ui::Paint::solid(ui::Color::White));
  target.stroke({x, y, 20, 80}, ui::Paint::solid(ui::Color::Black), 1);
  const int16_t tip = x + (next ? 14 : 6);
  const int16_t tail = x + (next ? 6 : 14);
  target.line({tail, y + 32}, {tip, y + 40}, 1, ui::Paint::solid(ui::Color::Black));
  target.line({tip, y + 40}, {tail, y + 48}, 1, ui::Paint::solid(ui::Color::Black));
}
}

void ReaderChrome::updateBattery() {
  const uint32_t now = millis();
  if (polled_ && now - lastPollMs_ < POLL_MS) return;
  lastPollMs_ = now;
  polled_ = true;
  static const BatteryMonitor monitor;
  const auto sample = monitor.readStatus();
  // A failed gauge transaction must not masquerade as a depleted battery.
  // Keep the last good percentage, and drop an unconfirmed charging marker.
  if (sample.percentageKnown) {
    battery_.percentage = sample.percentage > 100 ? 100 : sample.percentage;
    battery_.percentageKnown = true;
  }
  battery_.chargingKnown = sample.chargingKnown;
  battery_.charging = sample.chargingKnown && sample.charging;
  battery_.supported = sample.supported;
}

void ReaderChrome::draw(freeink::FreeInkDisplay& display, const ReaderButtonHints& hints) {
  if (!display.getFrameBuffer()) return;
  updateBattery();
  ui::DisplayTarget target(display.getFrameBuffer(), display.getDisplayWidth(), display.getDisplayHeight(),
                           display.getDisplayWidthBytes());
  target.setFont(ui::kReaderSmallFont);
  char percentage[8] = "--%";
  if (battery_.percentageKnown) {
    snprintf(percentage, sizeof(percentage), "%u%%", static_cast<unsigned>(battery_.percentage));
  }
  ui::BatteryIndicatorProps props{};
  props.percent = battery_.percentage;
  props.charging = battery_.charging;
  props.label = percentage;
  props.glyphWidth = 24;
  props.glyphHeight = 13;
  const ui::Rect batteryRect{static_cast<int16_t>(target.logicalWidth() - 114), 2, 90, 24};
  target.fill(batteryRect, ui::Paint::solid(ui::Color::White));
  ui::InteractionBuffer<1> interactions;
  const auto device = target.deviceContext();
  const ui::InputSnapshot input{};
  ui::Frame<1> frame(target, device, input, interactions);
  ui::batteryIndicator(frame, batteryRect, props);

  // Same four front-key columns as Crosspoint on the 528px X3 panel.
  buttonHint(target, 38, hints.back);
  buttonHint(target, 154, hints.confirm);
  buttonHint(target, 268, hints.previous);
  buttonHint(target, 384, hints.next);
  if (hints.sides) { sideHint(target, false); sideHint(target, true); }
}
