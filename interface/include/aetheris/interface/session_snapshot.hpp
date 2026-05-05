#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::interface {

/**
 * UI-friendly, serialisation-ready projection of an IntentSession.
 *
 * All enums are stringified, timestamps are microseconds since epoch (matching
 * the TypeScript Session interface in @aetheris-iui/core).
 */
struct SessionSnapshot {
  std::string id;
  std::string action_id;
  std::string operator_id;
  std::string tenant_id;
  std::string state;
  std::string confirmation_mode;
  std::vector<domain::Slot> slots;
  std::optional<std::string> clarification_question;
  std::optional<std::string> preview_result_json;
  std::optional<std::string> archive_reason;
  std::int64_t created_at_us{};
  std::int64_t updated_at_us{};
};

[[nodiscard]] std::string confirmation_mode_name(domain::ConfirmationMode m) noexcept;

/**
 * Projects an IntentSession onto a SessionSnapshot.
 * Requires the session's confirmation_mode (from its ActionSchema).
 */
[[nodiscard]] SessionSnapshot make_snapshot(const domain::IntentSession& session,
                                            domain::ConfirmationMode confirmation_mode);

/** Serialises a SessionSnapshot to a JSON string. */
[[nodiscard]] std::string snapshot_to_json(const SessionSnapshot& snap);

} // namespace aetheris::interface
