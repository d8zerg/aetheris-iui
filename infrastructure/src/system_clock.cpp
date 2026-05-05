#include "aetheris/infrastructure/system_clock.hpp"

namespace aetheris::infrastructure {

std::chrono::system_clock::time_point SystemClock::now() const noexcept {
  return std::chrono::system_clock::now();
}

} // namespace aetheris::infrastructure
