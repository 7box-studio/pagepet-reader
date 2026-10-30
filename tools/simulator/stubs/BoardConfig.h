#pragma once
namespace BoardConfig {
struct DisplayPins { int sclk = 0, mosi = 0, cs = 0, dc = 0, rst = 0, busy = 0; };
struct Board { DisplayPins display; };
inline constexpr Board XTEINK_X3{};
}
