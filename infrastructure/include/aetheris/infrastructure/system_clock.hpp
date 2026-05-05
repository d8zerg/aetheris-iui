#pragma once

#include <chrono>

#include "aetheris/domain/ports/clock_port.hpp"

namespace aetheris::infrastructure {

/**
 * ClockPort implementation backed by std::chrono::system_clock.
 */
class SystemClock final : public domain::ClockPort {
 public:
  [[nodiscard]] std::chrono::system_clock::time_point now() const noexcept override;
};

} // namespace aetheris::infrastructure
