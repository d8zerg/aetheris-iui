#include <gtest/gtest.h>

#include "aetheris/domain/error.hpp"

namespace {

using aetheris::domain::error_code;
using aetheris::domain::error_details;
using aetheris::domain::error_kind;
using aetheris::domain::error_message;
using aetheris::domain::ErrorDetail;
using aetheris::domain::make_ambiguity_error;
using aetheris::domain::make_domain_error;
using aetheris::domain::make_inference_error;
using aetheris::domain::make_input_error;
using aetheris::domain::make_internal_error;
using aetheris::domain::make_policy_error;

TEST(ErrorTest, ExposesStableKindCodeMessageAndDetails) {
  const auto error =
      make_policy_error("policy.denied", "Missing permission.", {ErrorDetail{"scope", "admin"}});

  EXPECT_EQ(error_kind(error), "policy");
  EXPECT_EQ(error_code(error), "policy.denied");
  EXPECT_EQ(error_message(error), "Missing permission.");
  ASSERT_EQ(error_details(error).size(), 1U);
  EXPECT_EQ(error_details(error).front().key, "scope");
  EXPECT_EQ(error_details(error).front().value, "admin");
}

TEST(ErrorTest, MapsEveryPublicErrorClassToStableKind) {
  EXPECT_EQ(error_kind(make_input_error("input", "input")), "input");
  EXPECT_EQ(error_kind(make_policy_error("policy", "policy")), "policy");
  EXPECT_EQ(error_kind(make_inference_error("inference", "inference")), "inference");
  EXPECT_EQ(error_kind(make_ambiguity_error("ambiguity", "ambiguity")), "ambiguity");
  EXPECT_EQ(error_kind(make_domain_error("domain", "domain")), "domain");
  EXPECT_EQ(error_kind(make_internal_error("internal", "internal")), "internal");
}

} // namespace
