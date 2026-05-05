#include <string>

#include <gtest/gtest.h>

#include "aetheris/domain/identifier.hpp"

namespace {

using aetheris::domain::ActionId;
using aetheris::domain::error_code;
using aetheris::domain::error_kind;
using aetheris::domain::to_string;

TEST(IdentifierTest, AcceptsStableActionIdSyntax) {
  const auto action_id = ActionId::parse("camera.archive.export:v1");

  ASSERT_TRUE(action_id.has_value());
  EXPECT_EQ(action_id->value(), "camera.archive.export:v1");
}

TEST(IdentifierTest, SerializesToCanonicalString) {
  const auto action_id = ActionId::parse("camera.archive.export:v1");

  ASSERT_TRUE(action_id.has_value());
  EXPECT_EQ(to_string(*action_id), "camera.archive.export:v1");
}

TEST(IdentifierTest, RejectsEmptyIdentifier) {
  const auto action_id = ActionId::parse("");

  ASSERT_FALSE(action_id.has_value());
  EXPECT_EQ(error_kind(action_id.error()), "input");
  EXPECT_EQ(error_code(action_id.error()), "identifier.empty");
}

TEST(IdentifierTest, RejectsUnsupportedCharacters) {
  const auto action_id = ActionId::parse("camera/archive/export");

  ASSERT_FALSE(action_id.has_value());
  EXPECT_EQ(error_kind(action_id.error()), "input");
  EXPECT_EQ(error_code(action_id.error()), "identifier.invalid_character");
}

TEST(IdentifierTest, RejectsTooLongIdentifier) {
  const auto action_id = ActionId::parse(std::string(ActionId::max_size + 1, 'a'));

  ASSERT_FALSE(action_id.has_value());
  EXPECT_EQ(error_kind(action_id.error()), "input");
  EXPECT_EQ(error_code(action_id.error()), "identifier.too_long");
}

} // namespace
