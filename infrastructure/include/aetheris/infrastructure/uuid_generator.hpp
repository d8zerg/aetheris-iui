#pragma once

#include <cstdint>
#include <mutex>
#include <random>
#include <string>

#include "aetheris/domain/ports/id_generator_port.hpp"

namespace aetheris::infrastructure {

/**
 * UUID-like IdGeneratorPort implementation with deterministic seeding for tests.
 */
class UuidGenerator final : public domain::IdGeneratorPort {
 public:
  UuidGenerator();
  explicit UuidGenerator(std::uint64_t seed) noexcept;

  [[nodiscard]] domain::Result<domain::IntentId> next_intent_id() override;
  [[nodiscard]] domain::Result<domain::SessionId> next_session_id() override;
  [[nodiscard]] domain::Result<domain::DecisionId> next_decision_id() override;

 private:
  [[nodiscard]] std::string next_uuid_string();

  std::mutex mutex_;
  std::mt19937_64 generator_;
};

} // namespace aetheris::infrastructure
