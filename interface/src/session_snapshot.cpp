#include "aetheris/interface/session_snapshot.hpp"

#include <sstream>

namespace aetheris::interface {

namespace {

std::string escape_json(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (const char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

std::string json_string(const std::string& s) {
  return '"' + escape_json(s) + '"';
}

std::string json_optional(const std::optional<std::string>& opt) {
  return opt.has_value() ? json_string(*opt) : "null";
}

// Serialises a value_json field: already a JSON value or null.
std::string json_value_json(const std::optional<std::string>& vj) {
  return vj.has_value() ? *vj : "null";
}

} // namespace

std::string confirmation_mode_name(domain::ConfirmationMode m) noexcept {
  switch (m) {
  case domain::ConfirmationMode::automatic:
    return "automatic";
  case domain::ConfirmationMode::single:
    return "single";
  case domain::ConfirmationMode::typed:
    return "typed";
  case domain::ConfirmationMode::multi_party:
    return "multi_party";
  case domain::ConfirmationMode::cooling_off:
    return "cooling_off";
  }
  return "single";
}

SessionSnapshot make_snapshot(const domain::IntentSession& session,
                              domain::ConfirmationMode confirmation_mode) {
  auto to_us = [](std::chrono::system_clock::time_point tp) -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
  };

  std::optional<std::string> clarification_q;
  if (!session.clarification_question().empty()) {
    clarification_q = session.clarification_question();
  }

  std::optional<std::string> preview;
  if (!session.preview_result_json().empty()) {
    preview = session.preview_result_json();
  }

  std::optional<std::string> archive_reason;
  if (const auto r = session.archive_reason(); r.has_value()) {
    archive_reason = std::string{domain::archive_reason_name(*r)};
  }

  return SessionSnapshot{
      .id = session.id().value(),
      .action_id = session.action_id().value(),
      .operator_id = session.operator_id().value(),
      .tenant_id = session.tenant_id().value(),
      .state = std::string{domain::session_state_name(session.state())},
      .confirmation_mode = confirmation_mode_name(confirmation_mode),
      .slots = session.slots(),
      .clarification_question = std::move(clarification_q),
      .preview_result_json = std::move(preview),
      .archive_reason = std::move(archive_reason),
      .created_at_us = to_us(session.created_at()),
      .updated_at_us = to_us(session.updated_at()),
  };
}

std::string snapshot_to_json(const SessionSnapshot& snap) {
  std::ostringstream os;
  os << '{';
  os << "\"id\":" << json_string(snap.id) << ',';
  os << "\"action_id\":" << json_string(snap.action_id) << ',';
  os << "\"operator_id\":" << json_string(snap.operator_id) << ',';
  os << "\"tenant_id\":" << json_string(snap.tenant_id) << ',';
  os << "\"state\":" << json_string(snap.state) << ',';
  os << "\"confirmation_mode\":" << json_string(snap.confirmation_mode) << ',';

  os << "\"slots\":[";
  for (std::size_t i = 0; i < snap.slots.size(); ++i) {
    if (i > 0)
      os << ',';
    const auto& sl = snap.slots[i];
    os << '{';
    os << "\"name\":" << json_string(sl.name) << ',';
    os << "\"required\":" << (sl.required ? "true" : "false") << ',';
    os << "\"value_json\":" << json_value_json(sl.value_json);
    os << '}';
  }
  os << "],";

  os << "\"clarification_question\":" << json_optional(snap.clarification_question) << ',';
  os << "\"preview_result_json\":" << json_optional(snap.preview_result_json) << ',';
  os << "\"archive_reason\":" << json_optional(snap.archive_reason) << ',';
  os << "\"created_at_us\":" << snap.created_at_us << ',';
  os << "\"updated_at_us\":" << snap.updated_at_us;
  os << '}';
  return os.str();
}

} // namespace aetheris::interface
