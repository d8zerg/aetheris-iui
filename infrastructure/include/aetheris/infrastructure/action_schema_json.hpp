#pragma once

#include <string>
#include <string_view>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * Parses a JSON string into an ActionSchema.
 *
 * Returns InputError for malformed JSON or schema invariant violations.
 * The domain layer performs all invariant validation - the parser only does
 * structural mapping.
 */
[[nodiscard]] domain::Result<domain::ActionSchema>
parse_action_schema_json(std::string_view json_text);

/**
 * Serializes an ActionSchema to its canonical JSON representation.
 *
 * The output round-trips through parse_action_schema_json without loss.
 */
[[nodiscard]] std::string serialize_action_schema_json(const domain::ActionSchema& schema);

} // namespace aetheris::infrastructure
