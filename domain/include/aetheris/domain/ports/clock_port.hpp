#pragma once

#include <chrono>

namespace aetheris::domain {

/**
 * Deterministic time source used by application use cases.
 */
class ClockPort {
 public:
  ClockPort() = default;
  ClockPort(const ClockPort&) = delete;
  ClockPort& operator=(const ClockPort&) = delete;
  ClockPort(ClockPort&&) = delete;
  ClockPort& operator=(ClockPort&&) = delete;
  virtual ~ClockPort() = default;

  /**
   * Returns current wall-clock time.
   */
  [[nodiscard]] virtual std::chrono::system_clock::time_point now() const noexcept = 0;
};

} // namespace aetheris::domain
