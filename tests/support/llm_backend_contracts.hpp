#pragma once

#include <string>

#include <gtest/gtest.h>

#include "aetheris/domain/intent.hpp"
#include "aetheris/domain/ports/llm_backend_port.hpp"

namespace aetheris::tests {

inline void expect_backend_name_not_empty(domain::LlmBackendPort& backend) {
  EXPECT_FALSE(std::string{backend.backend_name()}.empty())
      << "backend_name() must not return an empty string";
}

inline void expect_is_available_is_callable(domain::LlmBackendPort& backend) {
  [[maybe_unused]] const bool avail = backend.is_available();
}

inline void run_all_llm_backend_contracts(domain::LlmBackendPort& backend) {
  expect_backend_name_not_empty(backend);
  expect_is_available_is_callable(backend);
}

} // namespace aetheris::tests
