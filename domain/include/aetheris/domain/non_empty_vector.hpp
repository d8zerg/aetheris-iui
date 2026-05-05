#pragma once

#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

#include "aetheris/domain/error.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Vector wrapper that cannot represent an empty collection.
 */
template <typename TValue> class NonEmptyVector final {
 public:
  NonEmptyVector() = delete;

  /**
   * Creates a NonEmptyVector from an existing vector.
   */
  [[nodiscard]] static Result<NonEmptyVector> create(std::vector<TValue> values) {
    if (values.empty()) {
      return fail(make_input_error("non_empty_vector.empty", "Collection must not be empty."));
    }

    return NonEmptyVector{std::move(values)};
  }

  /**
   * Creates a NonEmptyVector from an initializer list.
   */
  [[nodiscard]] static Result<NonEmptyVector> create(std::initializer_list<TValue> values) {
    return create(std::vector<TValue>{values});
  }

  [[nodiscard]] const std::vector<TValue>& values() const noexcept {
    return values_;
  }

  [[nodiscard]] const TValue& front() const noexcept {
    return values_.front();
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return values_.size();
  }

 private:
  explicit NonEmptyVector(std::vector<TValue> values) : values_{std::move(values)} {}

  std::vector<TValue> values_;
};

} // namespace aetheris::domain
