#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * In-memory Action Schema registry used by tests, bootstrap flows, and future adapters.
 */
class ActionSchemaRegistry final {
 public:
  [[nodiscard]] Result<void> register_schema(ActionSchema schema) {
    if (find(schema.action_id(), schema.version()) != nullptr) {
      return fail(make_input_error("schema_registry.duplicate_version",
                                   "Action schema version is already registered."));
    }

    schemas_.push_back(std::move(schema));
    return {};
  }

  [[nodiscard]] const ActionSchema* find(const ActionId& action_id,
                                         const SchemaVersion& version) const noexcept {
    const auto it = std::find_if(schemas_.begin(), schemas_.end(), [&](const auto& schema) {
      return schema.action_id() == action_id && schema.version() == version;
    });
    return it != schemas_.end() ? &*it : nullptr;
  }

  [[nodiscard]] const ActionSchema* latest_for(const ActionId& action_id) const noexcept {
    const ActionSchema* latest = nullptr;
    for (const auto& schema : schemas_) {
      if (schema.action_id() == action_id) {
        latest = &schema;
      }
    }

    return latest;
  }

  [[nodiscard]] std::vector<SchemaVersion> versions_for(const ActionId& action_id) const {
    std::vector<SchemaVersion> versions;
    for (const auto& schema : schemas_) {
      if (schema.action_id() == action_id) {
        versions.push_back(schema.version());
      }
    }

    return versions;
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return schemas_.size();
  }

  [[nodiscard]] const std::vector<ActionSchema>& all() const noexcept {
    return schemas_;
  }

 private:
  std::vector<ActionSchema> schemas_;
};

} // namespace aetheris::domain
