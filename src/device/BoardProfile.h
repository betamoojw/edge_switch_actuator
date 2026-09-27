#pragma once
#include <array>
#include <stdint.h>

namespace actuator::board
{
inline constexpr std::array<uint8_t, 6> RelayPins = {1, 2, 41, 42, 45, 46};
inline constexpr uint8_t BootPin = 0;
inline constexpr uint8_t BuzzerPin = 21;
inline constexpr uint8_t RgbPin = 38;
inline constexpr uint8_t Rs485RxPin = 18;
inline constexpr uint8_t Rs485TxPin = 17;
} // namespace actuator::board
