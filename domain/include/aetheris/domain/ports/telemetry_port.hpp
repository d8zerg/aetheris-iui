#pragma once

#include <string>
#include <vector>

namespace aetheris::domain {

/**
 * Unit attached to emitted telemetry measurements.
 */
enum class TelemetryUnit {
  count,
  milliseconds,
  bytes,
};

/**
 * Key-value attribute for metrics and traces.
 */
struct TelemetryAttribute final {
  std::string key;
  std::string value;
};

/**
 * Minimal telemetry point emitted by core use cases.
 */
struct TelemetryPoint final {
  std::string name;
  double value = 0.0;
  TelemetryUnit unit = TelemetryUnit::count;
  std::vector<TelemetryAttribute> attributes;
};

/**
 * Metrics and tracing boundary. Implementations may export to OpenTelemetry or stay no-op.
 */
class TelemetryPort {
 public:
  TelemetryPort() = default;
  TelemetryPort(const TelemetryPort&) = delete;
  TelemetryPort& operator=(const TelemetryPort&) = delete;
  TelemetryPort(TelemetryPort&&) = delete;
  TelemetryPort& operator=(TelemetryPort&&) = delete;
  virtual ~TelemetryPort() = default;

  /**
   * Emits one telemetry point. The port is noexcept so telemetry cannot break use cases.
   */
  virtual void emit(const TelemetryPoint& point) noexcept = 0;
};

} // namespace aetheris::domain
