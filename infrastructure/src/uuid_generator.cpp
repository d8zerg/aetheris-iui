#include "aetheris/infrastructure/uuid_generator.hpp"

#include <array>
#include <cstddef>
#include <random>

namespace aetheris::infrastructure {
namespace {

[[nodiscard]] std::uint64_t make_seed() {
  std::random_device device;
  return (static_cast<std::uint64_t>(device()) << 32U) ^ static_cast<std::uint64_t>(device());
}

[[nodiscard]] std::string format_uuid(const std::array<std::uint8_t, 16>& bytes) {
  constexpr std::array<char, 16> hex_digits{'0', '1', '2', '3', '4', '5', '6', '7',
                                            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  std::string value;
  value.reserve(36);

  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (index == 4 || index == 6 || index == 8 || index == 10) {
      value.push_back('-');
    }

    const auto byte = bytes.at(index);
    value.push_back(hex_digits.at((byte >> 4U) & 0x0FU));
    value.push_back(hex_digits.at(byte & 0x0FU));
  }

  return value;
}

template <typename TIdentifier>
[[nodiscard]] domain::Result<TIdentifier> parse_generated(const std::string& value) {
  return TIdentifier::parse(value);
}

} // namespace

UuidGenerator::UuidGenerator() : UuidGenerator(make_seed()) {}

UuidGenerator::UuidGenerator(const std::uint64_t seed) noexcept : generator_{seed} {}

domain::Result<domain::IntentId> UuidGenerator::next_intent_id() {
  return parse_generated<domain::IntentId>(next_uuid_string());
}

domain::Result<domain::SessionId> UuidGenerator::next_session_id() {
  return parse_generated<domain::SessionId>(next_uuid_string());
}

domain::Result<domain::DecisionId> UuidGenerator::next_decision_id() {
  return parse_generated<domain::DecisionId>(next_uuid_string());
}

std::string UuidGenerator::next_uuid_string() {
  std::array<std::uint8_t, 16> bytes{};

  {
    std::scoped_lock lock{mutex_};
    for (std::size_t index = 0; index < bytes.size(); index += sizeof(std::uint64_t)) {
      const auto chunk = generator_();
      for (std::size_t offset = 0; offset < sizeof(std::uint64_t); ++offset) {
        bytes.at(index + offset) = static_cast<std::uint8_t>((chunk >> (offset * 8U)) & 0xFFU);
      }
    }
  }

  bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0FU) | 0x40U);
  bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3FU) | 0x80U);

  return format_uuid(bytes);
}

} // namespace aetheris::infrastructure
