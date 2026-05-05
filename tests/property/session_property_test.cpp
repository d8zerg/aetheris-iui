#include <chrono>
#include <string>
#include <vector>

#include <rapidcheck.h>

#include "aetheris/domain/identifier.hpp"
#include "aetheris/domain/session.hpp"

namespace {

using namespace aetheris::domain;

const auto kT0 = std::chrono::system_clock::time_point{std::chrono::seconds{1000}};
const auto kT1 = std::chrono::system_clock::time_point{std::chrono::seconds{1001}};

[[nodiscard]] IntentSession make_prop_session(int idx = 0) {
  return *IntentSession::create(*SessionId::parse("prop-sess-" + std::to_string(idx)),
                                *OperatorId::parse("op-prop"), *TenantId::parse("tenant-prop"),
                                *ActionId::parse("cam.disable"), {}, kT0);
}

// ---- Properties ----

/**
 * Property: archive is always reachable from any non-terminal state via cancel().
 * A session can always be aborted regardless of which state it is in.
 */
[[nodiscard]] bool archive_always_reachable_via_cancel() {
  return rc::check("archive is reachable from any non-terminal state via cancel", [] {
    // Generate a random sequence of valid forward transitions, then cancel.
    const auto steps = *rc::gen::inRange(0, 5);

    auto session = make_prop_session();
    // Drive session forward through a subset of the happy path
    if (steps >= 1) {
      // fill -> preview (no slots required)
      const auto r = session.preview("", kT1);
      RC_ASSERT(r.has_value());
    }
    if (steps >= 2) {
      // preview -> commit
      const auto r = session.confirm(kT1);
      RC_ASSERT(r.has_value());
    }
    // Any further step would be complete() -> archive, which leaves no room for cancel.
    // So we stop at commit and cancel.

    if (is_terminal(session.state())) {
      // Already archived from complete() - cannot cancel.
      return;
    }

    const auto r = session.cancel("property test cancellation", kT1);
    RC_ASSERT(r.has_value());
    RC_ASSERT(session.state() == SessionState::archive);
    RC_ASSERT(session.archive_reason().has_value());
    RC_ASSERT(*session.archive_reason() == ArchiveReason::cancelled);
  });
}

/**
 * Property: once archived, no transition method succeeds.
 */
[[nodiscard]] bool archived_session_rejects_all_transitions() {
  return rc::check("no transition succeeds on an archived session", [] {
    auto session = make_prop_session();
    RC_ASSERT(session.cancel("prop-archive", kT1).has_value());
    RC_ASSERT(session.state() == SessionState::archive);

    RC_ASSERT(!session.request_clarification("Q?", kT1).has_value());
    RC_ASSERT(!session.accept_clarification("A", kT1).has_value());
    RC_ASSERT(!session.fill_slot("x", "v", kT1).has_value());
    RC_ASSERT(!session.preview("", kT1).has_value());
    RC_ASSERT(!session.reject_preview(kT1).has_value());
    RC_ASSERT(!session.confirm(kT1).has_value());
    RC_ASSERT(!session.complete("r", kT1).has_value());
    RC_ASSERT(!session.cancel("again", kT1).has_value());
    RC_ASSERT(!session.expire(kT1).has_value());
  });
}

/**
 * Property: is_expired is monotone - once expired, it stays expired.
 */
[[nodiscard]] bool is_expired_is_monotone() {
  return rc::check("once is_expired returns true, it stays true for later times", [] {
    const auto ttl_s = static_cast<int64_t>(*rc::gen::inRange(1, 3600));
    const auto delta_s = static_cast<int64_t>(*rc::gen::inRange(1, 1000));

    const auto session = make_prop_session();
    const auto expiry = kT0 + std::chrono::seconds{ttl_s};
    const auto later = expiry + std::chrono::seconds{delta_s};

    // Before TTL: not expired
    // At or after TTL: expired
    if (!session.is_expired(expiry))
      return; // session has default TTL=3600

    RC_ASSERT(session.is_expired(later));
  });
}

/**
 * Property: fill_slot on an unknown slot always returns an error.
 */
[[nodiscard]] bool fill_slot_unknown_name_always_fails() {
  return rc::check("fill_slot with unknown name always fails", [] {
    auto session = make_prop_session();
    // Session has no slots, so any name is unknown
    const auto name =
        *rc::gen::map(rc::gen::inRange(1, 10), [](int i) { return "slot-" + std::to_string(i); });
    const auto value = R"("v")";
    const auto r = session.fill_slot(name, value, kT1);
    RC_ASSERT(!r.has_value());
    RC_ASSERT(error_code(r.error()) == "session.slot.unknown");
  });
}

/**
 * Property: a session with no required slots is always ready to preview.
 */
[[nodiscard]] bool no_required_slots_means_ready_to_preview() {
  return rc::check("session with no required slots is always preview-ready", [] {
    const auto n = *rc::gen::inRange(0, 5);
    std::vector<Slot> slots;
    for (int i = 0; i < n; ++i) {
      slots.push_back(
          Slot{.name = "opt-" + std::to_string(i), .required = false, .value_json = std::nullopt});
    }

    auto session = *IntentSession::create(*SessionId::parse("prop-noreq"),
                                          *OperatorId::parse("op-p"), *TenantId::parse("t-p"),
                                          *ActionId::parse("a.x"), std::move(slots), kT0);

    RC_ASSERT(session.all_required_slots_filled());
    const auto r = session.preview("", kT1);
    RC_ASSERT(r.has_value());
  });
}

} // namespace

int main() {
  const bool p1 = archive_always_reachable_via_cancel();
  const bool p2 = archived_session_rejects_all_transitions();
  const bool p3 = is_expired_is_monotone();
  const bool p4 = fill_slot_unknown_name_always_fails();
  const bool p5 = no_required_slots_means_ready_to_preview();
  return (p1 && p2 && p3 && p4 && p5) ? 0 : 1;
}
