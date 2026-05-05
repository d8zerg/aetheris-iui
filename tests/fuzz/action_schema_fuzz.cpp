#include <cstddef>
#include <cstdint>
#include <string_view>

#include "aetheris/infrastructure/action_schema_json.hpp"

// When compiled with -fsanitize=fuzzer, LLVMFuzzerTestOneInput is the entry point.
// When compiled normally (AETHERIS_FUZZ_STANDALONE), main() feeds stdin.
//
// The invariant under test: parse_action_schema_json never crashes, hangs, or
// invokes undefined behaviour on any byte sequence.

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  const std::string_view text{reinterpret_cast<const char*>(data), size};
  (void)aetheris::infrastructure::parse_action_schema_json(text);
  return 0;
}

#ifdef AETHERIS_FUZZ_STANDALONE
#include <iostream>
#include <iterator>
#include <string>

int main() {
  const std::string input{std::istreambuf_iterator<char>{std::cin},
                          std::istreambuf_iterator<char>{}};
  const auto result = aetheris::infrastructure::parse_action_schema_json(input);
  if (result.has_value()) {
    std::cout << "parsed ok: " << result->action_id().value() << "\n";
  } else {
    std::cout << "rejected: " << aetheris::domain::error_code(result.error()) << "\n";
  }
  return 0;
}
#endif
