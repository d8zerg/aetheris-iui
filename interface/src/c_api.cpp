#include "aetheris/interface/c_api.h"

#include <cstring>
#include <new>

#include "aetheris/application/runtime_descriptor.hpp"
#include "aetheris/domain/action_schema.hpp"
#include "aetheris/interface/session_snapshot.hpp"
#include "nlohmann/json.hpp"

struct aetheris_context {
  std::uint32_t abi_version = aetheris::application::kStableAbiVersion;
};

extern "C" {

const char* aetheris_version(void) {
  return AETHERIS_IUI_VERSION;
}

uint32_t aetheris_abi_version(void) {
  return aetheris::application::runtime_descriptor().abi_version;
}

aetheris_status aetheris_create_context(aetheris_context** out_context) {
  if (out_context == nullptr) {
    return aetheris_status{AETHERIS_STATUS_INVALID_ARGUMENT, "out_context must not be null"};
  }

  try {
    *out_context = new aetheris_context{};
  } catch (const std::bad_alloc&) {
    return aetheris_status{AETHERIS_STATUS_INTERNAL_ERROR, "failed to allocate context"};
  }

  return aetheris_status{AETHERIS_STATUS_OK, "ok"};
}

void aetheris_destroy_context(aetheris_context* context) {
  delete context;
}

aetheris_status aetheris_session_snapshot_json(const aetheris_context* context,
                                               const char* session_json, char** out_json) {
  if (context == nullptr || session_json == nullptr || out_json == nullptr) {
    return aetheris_status{AETHERIS_STATUS_INVALID_ARGUMENT,
                           "context, session_json, and out_json must not be null"};
  }
  *out_json = nullptr;

  try {
    using namespace aetheris;
    const auto doc = nlohmann::json::parse(session_json);

    // Build a minimal SessionSnapshot directly from the JSON payload.
    // The C API receives already-projected JSON from the server process;
    // this function validates and re-serialises it for host language callers.
    interface::SessionSnapshot snap;
    snap.id = doc.at("id").get<std::string>();
    snap.action_id = doc.at("action_id").get<std::string>();
    snap.operator_id = doc.at("operator_id").get<std::string>();
    snap.tenant_id = doc.at("tenant_id").get<std::string>();
    snap.state = doc.at("state").get<std::string>();
    snap.confirmation_mode = doc.at("confirmation_mode").get<std::string>();
    snap.created_at_us = doc.at("created_at_us").get<std::int64_t>();
    snap.updated_at_us = doc.at("updated_at_us").get<std::int64_t>();

    if (doc.contains("clarification_question") && !doc["clarification_question"].is_null()) {
      snap.clarification_question = doc["clarification_question"].get<std::string>();
    }
    if (doc.contains("preview_result_json") && !doc["preview_result_json"].is_null()) {
      snap.preview_result_json = doc["preview_result_json"].get<std::string>();
    }
    if (doc.contains("archive_reason") && !doc["archive_reason"].is_null()) {
      snap.archive_reason = doc["archive_reason"].get<std::string>();
    }

    for (const auto& sl : doc.at("slots")) {
      domain::Slot slot;
      slot.name = sl.at("name").get<std::string>();
      slot.required = sl.at("required").get<bool>();
      if (!sl["value_json"].is_null()) {
        slot.value_json = sl["value_json"].dump();
      }
      snap.slots.push_back(std::move(slot));
    }

    const std::string result = interface::snapshot_to_json(snap);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    *out_json = static_cast<char*>(std::malloc(result.size() + 1));
    if (*out_json == nullptr) {
      return aetheris_status{AETHERIS_STATUS_INTERNAL_ERROR, "malloc failed"};
    }
    std::memcpy(*out_json, result.c_str(), result.size() + 1);
    return aetheris_status{AETHERIS_STATUS_OK, "ok"};
  } catch (const std::exception& ex) {
    static thread_local std::string msg;
    msg = ex.what();
    return aetheris_status{AETHERIS_STATUS_INTERNAL_ERROR, msg.c_str()};
  } catch (...) {
    return aetheris_status{AETHERIS_STATUS_INTERNAL_ERROR, "unknown error"};
  }
}

void aetheris_free_string(char* str) {
  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
  std::free(str);
}
}
