#pragma once

#include "aetheris/domain/ports/slot_extractor_port.hpp"

namespace aetheris::infrastructure {

/**
 * SlotExtractorPort implementation that derives typed slots from the action's
 * ParameterSignature (JSON Schema) and the classifier's slots_json output.
 *
 * For each property declared in the schema's "properties" object:
 *   - required = property name appears in the "required" array
 *   - value_json = the compact-JSON-encoded value from slots_json, or nullopt
 *
 * Returns InferenceError when slots_json is malformed or when the parameter
 * schema itself cannot be parsed as a JSON object.
 */
class SchemaSlotExtractor final : public domain::SlotExtractorPort {
 public:
  SchemaSlotExtractor() noexcept = default;

  [[nodiscard]] domain::Result<std::vector<domain::Slot>>
  extract(const domain::ActionSchema& schema, std::string_view slots_json) noexcept override;
};

} // namespace aetheris::infrastructure
