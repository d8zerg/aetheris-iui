#include <cstddef>
#include <string>
#include <string_view>

#include <rapidcheck.h>

#include "aetheris/domain/identifier.hpp"

namespace {

using aetheris::domain::ActionId;
using aetheris::domain::SessionId;
using aetheris::domain::TenantId;

constexpr std::string_view kAllowedIdentifierCharacters =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.:";

[[nodiscard]] char generated_identifier_character() {
  const auto index =
      *rc::gen::inRange<int>(0, static_cast<int>(kAllowedIdentifierCharacters.size()));
  return kAllowedIdentifierCharacters[static_cast<std::size_t>(index)];
}

[[nodiscard]] std::string generated_valid_identifier() {
  const auto size = *rc::gen::inRange<int>(1, 96);
  std::string raw;
  raw.reserve(static_cast<std::size_t>(size));

  for (int index = 0; index < size; ++index) {
    raw.push_back(generated_identifier_character());
  }

  return raw;
}

template <typename TIdentifier> void assert_identifier_round_trip(std::string_view raw) {
  const auto parsed = TIdentifier::parse(raw);

  RC_ASSERT(parsed.has_value());
  RC_ASSERT(aetheris::domain::to_string(*parsed) == raw);
  RC_ASSERT(TIdentifier::parse(aetheris::domain::to_string(*parsed)).has_value());
}

[[nodiscard]] bool valid_identifier_round_trips() {
  return rc::check("generated identifier values round-trip through canonical serialization", [] {
    const auto raw = generated_valid_identifier();

    assert_identifier_round_trip<ActionId>(raw);
    assert_identifier_round_trip<SessionId>(raw);
    assert_identifier_round_trip<TenantId>(raw);
  });
}

[[nodiscard]] bool invalid_identifier_is_rejected() {
  return rc::check("generated ActionId values with slash are rejected", [] {
    const auto suffix = *rc::gen::inRange<int>(0, 1'000'000);
    const auto raw = std::string{"action/schema/"} + std::to_string(suffix);

    const auto parsed = ActionId::parse(raw);

    RC_ASSERT(!parsed.has_value());
  });
}

} // namespace

int main() {
  const bool valid_case = valid_identifier_round_trips();
  const bool invalid_case = invalid_identifier_is_rejected();
  return valid_case && invalid_case ? 0 : 1;
}
