#include "aetheris/infrastructure/schema_slot_extractor.hpp"

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::infrastructure {

using nlohmann::json;
using namespace domain;

Result<std::vector<Slot>> SchemaSlotExtractor::extract(const ActionSchema& schema,
                                                       std::string_view slots_json) noexcept {
  try {
    json param_schema;
    try {
      param_schema = json::parse(schema.parameters().json_schema());
    } catch (const json::parse_error& err) {
      return fail(make_inference_error("slot_extractor.schema.parse_error",
                                       "Action parameter schema is not valid JSON.",
                                       {ErrorDetail{"error", err.what()}}));
    }

    json slots_doc;
    try {
      if (slots_json.empty() || slots_json == "null") {
        slots_doc = json::object();
      } else {
        slots_doc = json::parse(slots_json);
      }
    } catch (const json::parse_error&) {
      return fail(make_inference_error(
          "slot_extractor.slots.parse_error", "Classifier slots_json is not valid JSON.",
          {ErrorDetail{"slots_json", std::string{slots_json}.substr(0, 200)}}));
    }

    if (!slots_doc.is_object()) {
      return fail(make_inference_error("slot_extractor.slots.not_object",
                                       "Classifier slots_json must be a JSON object."));
    }

    std::unordered_set<std::string> required_names;
    if (param_schema.contains("required") && param_schema["required"].is_array()) {
      for (const auto& req : param_schema["required"]) {
        if (req.is_string()) {
          required_names.insert(req.get<std::string>());
        }
      }
    }

    std::vector<Slot> result;
    if (!param_schema.contains("properties") || !param_schema["properties"].is_object()) {
      return result; // no declared properties -> empty slot list
    }

    for (const auto& [name, _prop] : param_schema["properties"].items()) {
      Slot slot;
      slot.name = name;
      slot.required = required_names.count(name) > 0;

      if (slots_doc.contains(name) && !slots_doc[name].is_null()) {
        slot.value_json = slots_doc[name].dump();
      }

      result.push_back(std::move(slot));
    }

    return result;

  } catch (const std::exception& ex) {
    return fail(
        make_inference_error("slot_extractor.unexpected_error",
                             std::string{"Unexpected error during slot extraction: "} + ex.what()));
  } catch (...) {
    return fail(make_inference_error("slot_extractor.unexpected_error",
                                     "Unknown error during slot extraction."));
  }
}

} // namespace aetheris::infrastructure
