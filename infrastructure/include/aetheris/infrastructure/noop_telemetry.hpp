#pragma once

#include "aetheris/domain/ports/telemetry_port.hpp"

namespace aetheris::infrastructure {

/**
 * TelemetryPort implementation that intentionally drops all events.
 */
class NoopTelemetry final : public domain::TelemetryPort {
 public:
  void emit(const domain::TelemetryPoint& point) noexcept override;
};

} // namespace aetheris::infrastructure
