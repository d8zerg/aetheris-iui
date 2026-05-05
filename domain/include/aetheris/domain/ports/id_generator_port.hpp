#pragma once

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Identifier source injected into use cases for deterministic tests.
 */
class IdGeneratorPort {
 public:
  IdGeneratorPort() = default;
  IdGeneratorPort(const IdGeneratorPort&) = delete;
  IdGeneratorPort& operator=(const IdGeneratorPort&) = delete;
  IdGeneratorPort(IdGeneratorPort&&) = delete;
  IdGeneratorPort& operator=(IdGeneratorPort&&) = delete;
  virtual ~IdGeneratorPort() = default;

  [[nodiscard]] virtual Result<IntentId> next_intent_id() = 0;
  [[nodiscard]] virtual Result<SessionId> next_session_id() = 0;
  [[nodiscard]] virtual Result<DecisionId> next_decision_id() = 0;
};

} // namespace aetheris::domain
