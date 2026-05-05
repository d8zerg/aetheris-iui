#pragma once

#include <string_view>
#include <vector>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/result.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::domain {

/**
 * Converts the raw slots_json produced by the intent classifier into a
 * vector of typed Slot values aligned with the action's parameter schema.
 *
 * The SchemaSlotExtractor infrastructure implementation parses both the
 * JSON Schema (from ActionSchema::parameters()) and the classifier output
 * to determine which slots are required, which are optional, and which
 * already have values from the classification pass.
 *
 * Returns InferenceError if the slots_json is malformed.
 */
class SlotExtractorPort {
 public:
  SlotExtractorPort() = default;
  SlotExtractorPort(const SlotExtractorPort&) = delete;
  SlotExtractorPort& operator=(const SlotExtractorPort&) = delete;
  SlotExtractorPort(SlotExtractorPort&&) = delete;
  SlotExtractorPort& operator=(SlotExtractorPort&&) = delete;
  virtual ~SlotExtractorPort() = default;

  /**
   * Extracts a typed slot list from the classifier's slots_json output.
   * slots_json is a JSON object whose keys are parameter names.
   * Must not throw.
   */
  [[nodiscard]] virtual Result<std::vector<Slot>> extract(const ActionSchema& schema,
                                                          std::string_view slots_json) noexcept = 0;
};

} // namespace aetheris::domain
