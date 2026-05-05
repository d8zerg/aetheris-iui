#include "aetheris/infrastructure/session_json.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/session.hpp"

namespace aetheris::infrastructure {

namespace {

using namespace domain;
using json = nlohmann::json;

[[nodiscard]] std::int64_t to_us(std::chrono::system_clock::time_point tp) noexcept {
  return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
}

[[nodiscard]] std::chrono::system_clock::time_point from_us(std::int64_t us) noexcept {
  return std::chrono::system_clock::time_point{std::chrono::microseconds{us}};
}

[[nodiscard]] std::optional<SessionState> state_from_string(std::string_view s) noexcept {
  if (s == "fill")
    return SessionState::fill;
  if (s == "clarification")
    return SessionState::clarification;
  if (s == "preview")
    return SessionState::preview;
  if (s == "commit")
    return SessionState::commit;
  if (s == "archive")
    return SessionState::archive;
  return std::nullopt;
}

[[nodiscard]] std::optional<ArchiveReason> archive_reason_from_string(std::string_view s) noexcept {
  if (s == "completed")
    return ArchiveReason::completed;
  if (s == "cancelled")
    return ArchiveReason::cancelled;
  if (s == "expired")
    return ArchiveReason::expired;
  return std::nullopt;
}

[[nodiscard]] std::string opt_string(const json& obj, std::string_view field) {
  const auto it = obj.find(field);
  if (it != obj.end() && it->is_string())
    return it->get<std::string>();
  return {};
}

template <typename T>
[[nodiscard]] Result<T> require_field(const json& obj, std::string_view field) {
  const auto it = obj.find(field);
  if (it == obj.end()) {
    return fail(make_input_error("session.json.missing_field",
                                 "Required field missing from session JSON.",
                                 {ErrorDetail{"field", std::string{field}}}));
  }
  try {
    return it->template get<T>();
  } catch (const json::exception&) {
    return fail(make_input_error("session.json.field_type",
                                 "Session JSON field has unexpected type.",
                                 {ErrorDetail{"field", std::string{field}}}));
  }
}

} // namespace

std::string serialize_session_json(const IntentSession& session) {
  json obj;
  obj["id"] = session.id().value();
  obj["operator_id"] = session.operator_id().value();
  obj["tenant_id"] = session.tenant_id().value();
  obj["action_id"] = session.action_id().value();
  obj["state"] = std::string{session_state_name(session.state())};
  obj["created_at_us"] = to_us(session.created_at());
  obj["updated_at_us"] = to_us(session.updated_at());
  obj["ttl_seconds"] = session.ttl().count();

  json slots = json::array();
  for (const auto& s : session.slots()) {
    json entry;
    entry["name"] = s.name;
    entry["required"] = s.required;
    if (s.value_json.has_value())
      entry["value_json"] = *s.value_json;
    slots.push_back(std::move(entry));
  }
  obj["slots"] = std::move(slots);

  if (!session.clarification_question().empty())
    obj["clarification_question"] = session.clarification_question();
  if (!session.clarification_answer().empty())
    obj["clarification_answer"] = session.clarification_answer();
  if (!session.preview_result_json().empty())
    obj["preview_result_json"] = session.preview_result_json();
  if (!session.result_json().empty())
    obj["result_json"] = session.result_json();
  if (!session.archive_note().empty())
    obj["archive_note"] = session.archive_note();
  if (const auto r = session.archive_reason(); r.has_value())
    obj["archive_reason"] = std::string{archive_reason_name(*r)};

  return obj.dump();
}

Result<IntentSession> parse_session_json(std::string_view json_text) {
  json obj;
  try {
    obj = json::parse(json_text);
  } catch (const json::exception& e) {
    return fail(make_input_error("session.json.parse_error", e.what()));
  }
  if (!obj.is_object()) {
    return fail(make_input_error("session.json.not_object", "Session JSON must be a JSON object."));
  }

  // Required fields
  auto id_s = require_field<std::string>(obj, "id");
  if (!id_s)
    return fail(id_s.error());
  auto op_s = require_field<std::string>(obj, "operator_id");
  if (!op_s)
    return fail(op_s.error());
  auto ten_s = require_field<std::string>(obj, "tenant_id");
  if (!ten_s)
    return fail(ten_s.error());
  auto act_s = require_field<std::string>(obj, "action_id");
  if (!act_s)
    return fail(act_s.error());
  auto state_s = require_field<std::string>(obj, "state");
  if (!state_s)
    return fail(state_s.error());
  auto cre_us = require_field<std::int64_t>(obj, "created_at_us");
  if (!cre_us)
    return fail(cre_us.error());
  auto upd_us = require_field<std::int64_t>(obj, "updated_at_us");
  if (!upd_us)
    return fail(upd_us.error());
  auto ttl_s = require_field<std::int64_t>(obj, "ttl_seconds");
  if (!ttl_s)
    return fail(ttl_s.error());

  auto session_id = SessionId::parse(*id_s);
  if (!session_id)
    return fail(session_id.error());
  auto operator_id = OperatorId::parse(*op_s);
  if (!operator_id)
    return fail(operator_id.error());
  auto tenant_id = TenantId::parse(*ten_s);
  if (!tenant_id)
    return fail(tenant_id.error());
  auto action_id = ActionId::parse(*act_s);
  if (!action_id)
    return fail(action_id.error());

  const auto state = state_from_string(*state_s);
  if (!state) {
    return fail(make_input_error("session.json.invalid_state", "Unknown session state.",
                                 {ErrorDetail{"value", *state_s}}));
  }

  // Parse slots (pre-filled values included)
  std::vector<Slot> slots;
  if (const auto it = obj.find("slots"); it != obj.end() && it->is_array()) {
    for (const auto& s : *it) {
      if (!s.is_object())
        continue;
      Slot slot;
      const auto nm = s.find("name");
      if (nm == s.end() || !nm->is_string()) {
        return fail(
            make_input_error("session.json.slot.missing_name", "Slot is missing a name field."));
      }
      slot.name = nm->get<std::string>();
      if (const auto rq = s.find("required"); rq != s.end() && rq->is_boolean())
        slot.required = rq->get<bool>();
      if (const auto vl = s.find("value_json"); vl != s.end() && vl->is_string())
        slot.value_json = vl->get<std::string>();
      slots.push_back(std::move(slot));
    }
  }

  // Create session at stored created_at; the slots already carry their values.
  const auto created_at = from_us(*cre_us);
  const auto updated_at = from_us(*upd_us);
  const auto ttl = std::chrono::seconds{*ttl_s};

  auto sr =
      IntentSession::create(std::move(*session_id), std::move(*operator_id), std::move(*tenant_id),
                            std::move(*action_id), std::move(slots), created_at, ttl);
  if (!sr)
    return fail(sr.error());
  IntentSession session = std::move(*sr);

  // Restore the stored state by replaying the minimal transition sequence.
  // create() always starts in `fill`; we drive from there to `*state`.
  const std::string dry_run_result = opt_string(obj, "preview_result_json");
  const std::string result_json = opt_string(obj, "result_json");
  const std::string clar_q = opt_string(obj, "clarification_question");
  const std::string archive_note = opt_string(obj, "archive_note");
  const std::string archive_r_str = opt_string(obj, "archive_reason");

  switch (*state) {
  case SessionState::fill:
    break; // already in fill

  case SessionState::clarification: {
    if (clar_q.empty()) {
      return fail(make_input_error("session.json.clarification.missing_question",
                                   "Clarification session is missing question field."));
    }
    if (auto r = session.request_clarification(clar_q, updated_at); !r)
      return fail(r.error());
    break;
  }

  case SessionState::preview: {
    if (auto r = session.preview(dry_run_result, updated_at); !r)
      return fail(r.error());
    break;
  }

  case SessionState::commit: {
    if (auto r = session.preview(dry_run_result, updated_at); !r)
      return fail(r.error());
    if (auto r = session.confirm(updated_at); !r)
      return fail(r.error());
    break;
  }

  case SessionState::archive: {
    const auto reason = archive_reason_from_string(archive_r_str);
    if (!reason) {
      return fail(make_input_error("session.json.archive.missing_reason",
                                   "Archived session is missing archive_reason field."));
    }
    switch (*reason) {
    case ArchiveReason::completed:
      if (auto r = session.preview(dry_run_result, updated_at); !r)
        return fail(r.error());
      if (auto r = session.confirm(updated_at); !r)
        return fail(r.error());
      if (auto r = session.complete(result_json, updated_at); !r)
        return fail(r.error());
      break;
    case ArchiveReason::cancelled:
      if (auto r = session.cancel(archive_note, updated_at); !r)
        return fail(r.error());
      break;
    case ArchiveReason::expired:
      if (auto r = session.expire(updated_at); !r)
        return fail(r.error());
      break;
    }
    break;
  }
  }

  return session;
}

} // namespace aetheris::infrastructure
