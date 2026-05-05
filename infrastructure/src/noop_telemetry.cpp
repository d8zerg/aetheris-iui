#include "aetheris/infrastructure/noop_telemetry.hpp"

namespace aetheris::infrastructure {

void NoopTelemetry::emit(const domain::TelemetryPoint& point) noexcept {
  static_cast<void>(point);
}

} // namespace aetheris::infrastructure
