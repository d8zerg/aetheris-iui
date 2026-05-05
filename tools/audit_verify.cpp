#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/infrastructure/audit_chain.hpp"
#include "aetheris/infrastructure/json_lines_exporter.hpp"

namespace {

using namespace aetheris::domain;
using namespace aetheris::infrastructure;

constexpr std::string_view kUsage = R"(usage: aetheris-audit verify [FILE...]

Verifies the cryptographic integrity of an Aetheris-IUI audit chain stored
as a JSON Lines file (one AuditChainNode per line).

If no FILE is given, reads from stdin.

The verifier checks:
  - sequence numbers are consecutive starting from 0
  - each record_hash matches SHA-256 of the canonical record bytes
  - each chain_hash matches SHA-256(record_hash || prev_chain_hash)
  - each node's prev_chain_hash equals the previous node's chain_hash
  - the first node's prev_chain_hash is the zero sentinel (genesis)

Exit codes:
  0  chain is intact
  1  chain is tampered, corrupted, or unreadable
)";

[[nodiscard]] bool verify_stream(std::istream& in, std::string_view source) {
  std::vector<AuditChainNode> nodes;
  std::string line;
  std::uint64_t line_num = 0;

  while (std::getline(in, line)) {
    ++line_num;
    if (line.empty())
      continue;

    const auto node = parse_audit_node_json(line);
    if (!node.has_value()) {
      std::cerr << "error  " << source << ":" << line_num << ": " << error_code(node.error())
                << " - " << error_message(node.error()) << "\n";
      return false;
    }
    nodes.push_back(std::move(*node));
  }

  if (nodes.empty()) {
    std::cout << "  ok   " << source << ": empty chain (0 records)\n";
    return true;
  }

  const auto result = verify_audit_chain(nodes);
  if (!result.has_value()) {
    std::cerr << "error  " << source << ": " << error_code(result.error()) << " - "
              << error_message(result.error()) << "\n";
    return false;
  }

  std::cout << "  ok   " << source << ": " << nodes.size() << " record(s), chain intact\n";
  return true;
}

} // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && (std::string_view{argv[1]} == "--help" || std::string_view{argv[1]} == "-h")) {
    std::cout << kUsage;
    return EXIT_SUCCESS;
  }

  // Accept both: `aetheris-audit verify [FILE...]` and `aetheris-audit [FILE...]`
  int first_file_arg = 1;
  if (argc >= 2 && std::string_view{argv[1]} == "verify") {
    first_file_arg = 2;
  }

  bool all_ok = true;

  if (argc <= first_file_arg) {
    if (!verify_stream(std::cin, "<stdin>")) {
      all_ok = false;
    }
  } else {
    for (int i = first_file_arg; i < argc; ++i) {
      const std::filesystem::path path{argv[i]};
      std::ifstream file{path};
      if (!file.is_open()) {
        std::cerr << "error  " << path.string() << ": cannot open file\n";
        all_ok = false;
        continue;
      }
      if (!verify_stream(file, path.string())) {
        all_ok = false;
      }
    }
  }

  return all_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
