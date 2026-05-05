#pragma once

#include <string>
#include <string_view>

#include "aetheris/domain/result.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::infrastructure {

/**
 * Serializes an IntentSession to a JSON string.
 *
 * The output is a single JSON object.  Timestamps are encoded as Unix
 * microseconds (int64).  Optional fields are omitted when absent.
 * This format is used for persistent storage and restart recovery.
 */
[[nodiscard]] std::string serialize_session_json(const domain::IntentSession& session);

/**
 * Parses an IntentSession from a JSON string produced by serialize_session_json.
 *
 * Returns InputError on malformed JSON or missing required fields.
 */
[[nodiscard]] domain::Result<domain::IntentSession> parse_session_json(std::string_view json_text);

} // namespace aetheris::infrastructure
