#pragma once

#include <gtest/gtest.h>

#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/ports/knowledge_source_port.hpp"

namespace aetheris::tests::contracts {

using namespace domain;

[[nodiscard]] inline KnowledgeQuery make_knowledge_contract_query(std::size_t max_results = 10) {
  return KnowledgeQuery{.text = "test query", .max_results = max_results};
}

// Contract: name() returns a non-empty string.
inline void expect_name_not_empty(KnowledgeSourcePort& source) {
  EXPECT_FALSE(source.name().empty());
}

// Contract: retrieve() returns a result (not an error) on a generic query.
inline void expect_retrieve_returns_result(KnowledgeSourcePort& source) {
  const auto result = source.retrieve(make_knowledge_contract_query());
  EXPECT_TRUE(result.has_value());
}

// Contract: retrieve() respects max_results.
inline void expect_retrieve_respects_max_results(KnowledgeSourcePort& source) {
  constexpr std::size_t kMax = 2;
  const auto result = source.retrieve(make_knowledge_contract_query(kMax));
  ASSERT_TRUE(result.has_value());
  EXPECT_LE(result->size(), kMax);
}

// Runs all contract checks against the given source.
inline void run_all_knowledge_source_contracts(KnowledgeSourcePort& source) {
  expect_name_not_empty(source);
  expect_retrieve_returns_result(source);
  expect_retrieve_respects_max_results(source);
}

} // namespace aetheris::tests::contracts
