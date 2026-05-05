#pragma once

#include <algorithm>
#include <compare>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {
namespace detail {

[[nodiscard]] constexpr bool is_identifier_char(const char value) noexcept {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') || value == '_' || value == '-' || value == '.' ||
         value == ':';
}

} // namespace detail

/**
 * Strong identifier wrapper with platform-wide syntax invariants.
 */
template <typename TTag> class Identifier final {
 public:
  static constexpr std::size_t max_size = 128;

  Identifier() = delete;

  /**
   * Parses and validates an identifier value.
   */
  [[nodiscard]] static Result<Identifier> parse(std::string_view value) {
    if (value.empty()) {
      return fail(make_input_error("identifier.empty", "Identifier must not be empty."));
    }

    if (value.size() > max_size) {
      return fail(make_input_error("identifier.too_long", "Identifier exceeds the maximum length.",
                                   {ErrorDetail{"max_size", std::to_string(max_size)},
                                    ErrorDetail{"actual_size", std::to_string(value.size())}}));
    }

    const auto invalid = std::ranges::find_if(value, [](const char character) noexcept {
      return !detail::is_identifier_char(character);
    });
    if (invalid != value.end()) {
      return fail(make_input_error("identifier.invalid_character",
                                   "Identifier contains an unsupported character.",
                                   {ErrorDetail{"character", std::string{*invalid}}}));
    }

    return Identifier{std::string{value}};
  }

  /**
   * Returns the stored identifier as an owning string reference.
   */
  [[nodiscard]] const std::string& value() const noexcept {
    return value_;
  }

  /**
   * Returns the stored identifier as a string view.
   */
  [[nodiscard]] std::string_view view() const noexcept {
    return value_;
  }

  [[nodiscard]] friend bool operator==(const Identifier& lhs,
                                       const Identifier& rhs) noexcept = default;
  [[nodiscard]] friend auto operator<=>(const Identifier& lhs,
                                        const Identifier& rhs) noexcept = default;

 private:
  explicit Identifier(std::string value) : value_{std::move(value)} {}

  std::string value_;
};

struct ActionIdTag final {};
struct SchemaVersionTag final {};
struct IntentIdTag final {};
struct SessionIdTag final {};
struct DecisionIdTag final {};
struct OperatorIdTag final {};
struct TenantIdTag final {};

using ActionId = Identifier<ActionIdTag>;
using SchemaVersion = Identifier<SchemaVersionTag>;
using IntentId = Identifier<IntentIdTag>;
using SessionId = Identifier<SessionIdTag>;
using DecisionId = Identifier<DecisionIdTag>;
using OperatorId = Identifier<OperatorIdTag>;
using TenantId = Identifier<TenantIdTag>;

/**
 * Serializes an identifier into its canonical string form.
 */
template <typename TTag> [[nodiscard]] std::string to_string(const Identifier<TTag>& identifier) {
  return identifier.value();
}

} // namespace aetheris::domain
