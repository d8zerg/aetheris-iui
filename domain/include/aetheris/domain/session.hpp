#pragma once

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

// ---- Slot ----

/**
 * One named parameter slot for an in-flight intent session.
 *
 * `required` slots must be filled before the session can transition to preview.
 */
struct Slot final {
  std::string name;
  bool required{true};
  std::optional<std::string> value_json;
};

[[nodiscard]] inline bool is_filled(const Slot& slot) noexcept {
  return slot.value_json.has_value();
}

// ---- SessionState ----

/**
 * States of the intent session state machine.
 *
 * Valid transitions:
 *   fill          -> {clarification, preview, archive}
 *   clarification -> {fill, archive}
 *   preview       -> {fill, commit, archive}
 *   commit        -> {archive}
 *   archive       - terminal, no further transitions
 */
enum class SessionState {
  fill,
  clarification,
  preview,
  commit,
  archive,
};

[[nodiscard]] inline std::string_view session_state_name(SessionState s) noexcept {
  switch (s) {
  case SessionState::fill:
    return "fill";
  case SessionState::clarification:
    return "clarification";
  case SessionState::preview:
    return "preview";
  case SessionState::commit:
    return "commit";
  case SessionState::archive:
    return "archive";
  }
  return "unknown";
}

[[nodiscard]] inline bool is_terminal(SessionState s) noexcept {
  return s == SessionState::archive;
}

// ---- ArchiveReason ----

enum class ArchiveReason {
  completed,
  cancelled,
  expired,
};

[[nodiscard]] inline std::string_view archive_reason_name(ArchiveReason r) noexcept {
  switch (r) {
  case ArchiveReason::completed:
    return "completed";
  case ArchiveReason::cancelled:
    return "cancelled";
  case ArchiveReason::expired:
    return "expired";
  }
  return "unknown";
}

// ---- IntentSession ----

/**
 * Aggregate root for a single multi-turn intent resolution session.
 *
 * Encapsulates the state machine, slot-filling state, TTL, and isolation
 * context (operator + tenant).  All mutating methods accept an explicit `now`
 * parameter for deterministic testing without ClockPort injection.
 *
 * Sessions always start in the `fill` state.  If all required slots are
 * already pre-filled the caller may immediately call preview() without
 * calling fill_slot().
 */
class IntentSession final {
 public:
  IntentSession() = delete;

  /**
   * Creates a new session in the `fill` state.
   */
  [[nodiscard]] static Result<IntentSession>
  create(SessionId id, OperatorId operator_id, TenantId tenant_id, ActionId action_id,
         std::vector<Slot> slots, std::chrono::system_clock::time_point now,
         std::chrono::seconds ttl = std::chrono::seconds{3600}) {
    if (std::any_of(slots.begin(), slots.end(),
                    [](const auto& slot) { return slot.name.empty(); })) {
      return fail(make_input_error("session.slot.empty_name", "Slot name must not be empty."));
    }
    return IntentSession{std::move(id),
                         std::move(operator_id),
                         std::move(tenant_id),
                         std::move(action_id),
                         std::move(slots),
                         now,
                         ttl};
  }

  // ---- Transitions ----

  /** fill -> clarification */
  [[nodiscard]] Result<void> request_clarification(std::string question,
                                                   std::chrono::system_clock::time_point now) {
    if (auto e = require_state(SessionState::fill, "request_clarification"); !e.has_value()) {
      return e;
    }
    if (question.empty()) {
      return fail(make_input_error("session.clarification.empty_question",
                                   "Clarification question must not be empty."));
    }
    clarification_question_ = std::move(question);
    transition_to(SessionState::clarification, now);
    return {};
  }

  /** clarification -> fill */
  [[nodiscard]] Result<void> accept_clarification(std::string answer,
                                                  std::chrono::system_clock::time_point now) {
    if (auto e = require_state(SessionState::clarification, "accept_clarification");
        !e.has_value()) {
      return e;
    }
    clarification_answer_ = std::move(answer);
    clarification_question_.clear();
    transition_to(SessionState::fill, now);
    return {};
  }

  /** fill -> fill (intra-state slot update) */
  [[nodiscard]] Result<void> fill_slot(std::string_view name, std::string value_json,
                                       std::chrono::system_clock::time_point now) {
    if (auto e = require_state(SessionState::fill, "fill_slot"); !e.has_value()) {
      return e;
    }
    if (value_json.empty()) {
      return fail(make_input_error("session.slot.empty_value", "Slot value must not be empty."));
    }

    const auto it = std::find_if(slots_.begin(), slots_.end(),
                                 [name](const Slot& s) { return s.name == name; });
    if (it == slots_.end()) {
      return fail(make_input_error("session.slot.unknown",
                                   "No slot with that name exists in this session.",
                                   {ErrorDetail{"slot_name", std::string{name}}}));
    }

    it->value_json = std::move(value_json);
    updated_at_ = now;
    return {};
  }

  /** fill -> preview (requires all required slots filled) */
  [[nodiscard]] Result<void> preview(std::string dry_run_result_json,
                                     std::chrono::system_clock::time_point now) {
    if (auto e = require_state(SessionState::fill, "preview"); !e.has_value()) {
      return e;
    }
    if (!all_required_slots_filled()) {
      return fail(make_input_error("session.preview.slots_incomplete",
                                   "All required slots must be filled before previewing."));
    }
    preview_result_json_ = std::move(dry_run_result_json);
    transition_to(SessionState::preview, now);
    return {};
  }

  /** preview -> fill (operator rejected preview, wants to change params) */
  [[nodiscard]] Result<void> reject_preview(std::chrono::system_clock::time_point now) {
    if (auto e = require_state(SessionState::preview, "reject_preview"); !e.has_value()) {
      return e;
    }
    preview_result_json_.clear();
    transition_to(SessionState::fill, now);
    return {};
  }

  /** preview -> commit */
  [[nodiscard]] Result<void> confirm(std::chrono::system_clock::time_point now) {
    if (auto e = require_state(SessionState::preview, "confirm"); !e.has_value()) {
      return e;
    }
    transition_to(SessionState::commit, now);
    return {};
  }

  /** commit -> archive (completed) */
  [[nodiscard]] Result<void> complete(std::string result_json,
                                      std::chrono::system_clock::time_point now) {
    if (auto e = require_state(SessionState::commit, "complete"); !e.has_value()) {
      return e;
    }
    result_json_ = std::move(result_json);
    archive_reason_ = ArchiveReason::completed;
    transition_to(SessionState::archive, now);
    return {};
  }

  /** any non-terminal -> archive (cancelled) */
  [[nodiscard]] Result<void> cancel(std::string note, std::chrono::system_clock::time_point now) {
    if (is_terminal(state_)) {
      return fail(make_input_error("session.transition.already_archived",
                                   "Cannot cancel a session that is already archived.",
                                   {ErrorDetail{"session_id", id_.value()}}));
    }
    archive_note_ = std::move(note);
    archive_reason_ = ArchiveReason::cancelled;
    transition_to(SessionState::archive, now);
    return {};
  }

  /** any non-terminal -> archive (expired) */
  [[nodiscard]] Result<void> expire(std::chrono::system_clock::time_point now) {
    if (is_terminal(state_)) {
      return fail(make_input_error("session.transition.already_archived",
                                   "Cannot expire a session that is already archived.",
                                   {ErrorDetail{"session_id", id_.value()}}));
    }
    archive_reason_ = ArchiveReason::expired;
    transition_to(SessionState::archive, now);
    return {};
  }

  // ---- Queries ----

  [[nodiscard]] bool all_required_slots_filled() const noexcept {
    return std::all_of(slots_.begin(), slots_.end(),
                       [](const Slot& s) { return !s.required || is_filled(s); });
  }

  [[nodiscard]] bool is_expired(std::chrono::system_clock::time_point now) const noexcept {
    return now >= created_at_ + ttl_;
  }

  // ---- Accessors ----

  [[nodiscard]] const SessionId& id() const noexcept {
    return id_;
  }
  [[nodiscard]] const OperatorId& operator_id() const noexcept {
    return operator_id_;
  }
  [[nodiscard]] const TenantId& tenant_id() const noexcept {
    return tenant_id_;
  }
  [[nodiscard]] const ActionId& action_id() const noexcept {
    return action_id_;
  }
  [[nodiscard]] SessionState state() const noexcept {
    return state_;
  }
  [[nodiscard]] const std::vector<Slot>& slots() const noexcept {
    return slots_;
  }
  [[nodiscard]] std::chrono::system_clock::time_point created_at() const noexcept {
    return created_at_;
  }
  [[nodiscard]] std::chrono::system_clock::time_point updated_at() const noexcept {
    return updated_at_;
  }
  [[nodiscard]] std::chrono::seconds ttl() const noexcept {
    return ttl_;
  }
  [[nodiscard]] const std::string& clarification_question() const noexcept {
    return clarification_question_;
  }
  [[nodiscard]] const std::string& clarification_answer() const noexcept {
    return clarification_answer_;
  }
  [[nodiscard]] const std::string& preview_result_json() const noexcept {
    return preview_result_json_;
  }
  [[nodiscard]] const std::string& result_json() const noexcept {
    return result_json_;
  }
  [[nodiscard]] const std::string& archive_note() const noexcept {
    return archive_note_;
  }
  [[nodiscard]] std::optional<ArchiveReason> archive_reason() const noexcept {
    return archive_reason_;
  }

 private:
  explicit IntentSession(SessionId id, OperatorId operator_id, TenantId tenant_id,
                         ActionId action_id, std::vector<Slot> slots,
                         std::chrono::system_clock::time_point now, std::chrono::seconds ttl)
      : id_{std::move(id)}, operator_id_{std::move(operator_id)}, tenant_id_{std::move(tenant_id)},
        action_id_{std::move(action_id)}, state_{SessionState::fill}, slots_{std::move(slots)},
        created_at_{now}, updated_at_{now}, ttl_{ttl} {}

  [[nodiscard]] Result<void> require_state(SessionState expected,
                                           std::string_view operation) const noexcept {
    if (state_ == SessionState::archive) {
      return fail(make_input_error("session.transition.already_archived",
                                   "Cannot perform operation on an archived session.",
                                   {ErrorDetail{"operation", std::string{operation}},
                                    ErrorDetail{"session_id", id_.value()}}));
    }
    if (state_ != expected) {
      return fail(make_input_error(
          "session.transition.invalid", "Operation is not valid in the current session state.",
          {ErrorDetail{"operation", std::string{operation}},
           ErrorDetail{"current_state", std::string{session_state_name(state_)}},
           ErrorDetail{"required_state", std::string{session_state_name(expected)}}}));
    }
    return {};
  }

  void transition_to(SessionState next, std::chrono::system_clock::time_point now) noexcept {
    state_ = next;
    updated_at_ = now;
  }

  SessionId id_;
  OperatorId operator_id_;
  TenantId tenant_id_;
  ActionId action_id_;
  SessionState state_{SessionState::fill};
  std::vector<Slot> slots_;
  std::chrono::system_clock::time_point created_at_{};
  std::chrono::system_clock::time_point updated_at_{};
  std::chrono::seconds ttl_{3600};
  std::string clarification_question_;
  std::string clarification_answer_;
  std::string preview_result_json_;
  std::string result_json_;
  std::string archive_note_;
  std::optional<ArchiveReason> archive_reason_;
};

} // namespace aetheris::domain
