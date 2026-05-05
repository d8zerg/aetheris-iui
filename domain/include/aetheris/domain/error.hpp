#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace aetheris::domain {

/**
 * Machine-readable error detail attached to platform errors.
 */
struct ErrorDetail final {
  std::string key;
  std::string value;
};

/**
 * Invalid operator input, malformed schema data, or missing required parameters.
 */
struct InputError final {
  std::string code;
  std::string message;
  std::vector<ErrorDetail> details;
};

/**
 * Rejection caused by permission, blast radius, quota, or another safety policy.
 */
struct PolicyError final {
  std::string code;
  std::string message;
  std::vector<ErrorDetail> details;
};

/**
 * Failure returned by an LLM backend or structured inference adapter.
 */
struct InferenceError final {
  std::string code;
  std::string message;
  std::vector<ErrorDetail> details;
};

/**
 * Failure to select one action candidate with acceptable confidence.
 */
struct AmbiguityError final {
  std::string code;
  std::string message;
  std::vector<ErrorDetail> details;
};

/**
 * Failure returned by the target domain system while executing a confirmed action.
 */
struct DomainError final {
  std::string code;
  std::string message;
  std::vector<ErrorDetail> details;
};

/**
 * Platform invariant violation or unrecoverable inconsistency.
 */
struct InternalError final {
  std::string code;
  std::string message;
  std::vector<ErrorDetail> details;
};

/**
 * Closed sum of all error classes exposed by the domain layer.
 */
using PlatformError = std::variant<InputError, PolicyError, InferenceError, AmbiguityError,
                                   DomainError, InternalError>;

/**
 * Returns the stable machine-readable error kind.
 */
[[nodiscard]] inline std::string_view error_kind(const PlatformError& error) noexcept {
  return std::visit(
      [](const auto& concrete_error) noexcept -> std::string_view {
        using Error = std::remove_cvref_t<decltype(concrete_error)>;
        if constexpr (std::is_same_v<Error, InputError>) {
          return "input";
        } else if constexpr (std::is_same_v<Error, PolicyError>) {
          return "policy";
        } else if constexpr (std::is_same_v<Error, InferenceError>) {
          return "inference";
        } else if constexpr (std::is_same_v<Error, AmbiguityError>) {
          return "ambiguity";
        } else if constexpr (std::is_same_v<Error, DomainError>) {
          return "domain";
        } else {
          return "internal";
        }
      },
      error);
}

/**
 * Returns the stable error code.
 */
[[nodiscard]] inline const std::string& error_code(const PlatformError& error) noexcept {
  return std::visit(
      [](const auto& concrete_error) noexcept -> const std::string& { return concrete_error.code; },
      error);
}

/**
 * Returns the localized or operator-facing error message.
 */
[[nodiscard]] inline const std::string& error_message(const PlatformError& error) noexcept {
  return std::visit(
      [](const auto& concrete_error) noexcept -> const std::string& {
        return concrete_error.message;
      },
      error);
}

/**
 * Returns machine-readable details carried by the error.
 */
[[nodiscard]] inline const std::vector<ErrorDetail>&
error_details(const PlatformError& error) noexcept {
  return std::visit(
      [](const auto& concrete_error) noexcept -> const std::vector<ErrorDetail>& {
        return concrete_error.details;
      },
      error);
}

[[nodiscard]] inline PlatformError make_input_error(std::string code, std::string message,
                                                    std::vector<ErrorDetail> details = {}) {
  return InputError{std::move(code), std::move(message), std::move(details)};
}

[[nodiscard]] inline PlatformError make_policy_error(std::string code, std::string message,
                                                     std::vector<ErrorDetail> details = {}) {
  return PolicyError{std::move(code), std::move(message), std::move(details)};
}

[[nodiscard]] inline PlatformError make_inference_error(std::string code, std::string message,
                                                        std::vector<ErrorDetail> details = {}) {
  return InferenceError{std::move(code), std::move(message), std::move(details)};
}

[[nodiscard]] inline PlatformError make_ambiguity_error(std::string code, std::string message,
                                                        std::vector<ErrorDetail> details = {}) {
  return AmbiguityError{std::move(code), std::move(message), std::move(details)};
}

[[nodiscard]] inline PlatformError make_domain_error(std::string code, std::string message,
                                                     std::vector<ErrorDetail> details = {}) {
  return DomainError{std::move(code), std::move(message), std::move(details)};
}

[[nodiscard]] inline PlatformError make_internal_error(std::string code, std::string message,
                                                       std::vector<ErrorDetail> details = {}) {
  return InternalError{std::move(code), std::move(message), std::move(details)};
}

} // namespace aetheris::domain
