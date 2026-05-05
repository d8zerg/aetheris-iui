/**
 * aetheris - unified CLI for Aetheris-IUI.
 *
 * Subcommands:
 *   version                     print library version and ABI
 *   schema lint [FILE...]        validate Action Schema JSON files
 *   schema generate <openapi>    stub generator (prints guidance)
 *   adapter new <name>           scaffold a new adapter (prints template)
 *   eval run <dataset.jsonl>     run golden-dataset evaluation
 *   audit verify <log.jsonl>     verify audit chain integrity
 *   help                        show this help
 */
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "aetheris/application/runtime_descriptor.hpp"
#include "aetheris/domain/action_schema.hpp"
#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/error.hpp"
#include "aetheris/infrastructure/action_schema_json.hpp"
#include "aetheris/infrastructure/audit_chain.hpp"
#include "aetheris/infrastructure/json_lines_exporter.hpp"
#include "nlohmann/json.hpp"

using namespace aetheris;
namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string read_file(const fs::path& path) {
  std::ifstream f{path};
  if (!f)
    throw std::runtime_error{"cannot open: " + path.string()};
  return {std::istreambuf_iterator<char>{f}, {}};
}

// ── schema lint ───────────────────────────────────────────────────────────────

static int cmd_schema_lint(int argc, char** argv) {
  // argv[0] = "lint", argv[1..] = files (or empty -> stdin)
  bool any_error = false;

  auto lint_text = [&](std::string_view source, const std::string& text) {
    const auto schema = infrastructure::parse_action_schema_json(text);
    if (!schema.has_value()) {
      std::cerr << "error  " << source << ": " << domain::error_code(schema.error()) << " - "
                << domain::error_message(schema.error()) << "\n";
      any_error = true;
      return;
    }
    std::cout << "  ok  " << source << "\n";
  };

  if (argc < 2) {
    std::string text{std::istreambuf_iterator<char>{std::cin}, {}};
    lint_text("<stdin>", text);
  } else {
    for (int i = 1; i < argc; ++i) {
      const fs::path p{argv[i]};
      try {
        lint_text(p.string(), read_file(p));
      } catch (const std::exception& ex) {
        std::cerr << "error  " << p.string() << ": " << ex.what() << "\n";
        any_error = true;
      }
    }
  }
  return any_error ? EXIT_FAILURE : EXIT_SUCCESS;
}

// ── schema generate ───────────────────────────────────────────────────────────

static int cmd_schema_generate(int argc, char** argv) {
  const std::string_view source = argc >= 3 ? argv[2] : "<openapi>";
  std::cout << "# aetheris schema generate: OpenAPI -> Action Schema stub\n"
               "#\n"
               "# Source: "
            << source
            << "\n"
               "#\n"
               "# This feature reads an OpenAPI 3.x spec and emits a skeleton Action Schema\n"
               "# for each operation tagged with x-aetheris-action.\n"
               "#\n"
               "# Full implementation is in @aetheris-iui/adapter-sdk (TypeScript package).\n"
               "# Run:  npx @aetheris-iui/adapter-sdk schema generate "
            << source << "\n";
  return EXIT_SUCCESS;
}

// ── adapter new ───────────────────────────────────────────────────────────────

static int cmd_adapter_new(int argc, char** argv) {
  const std::string name = argc >= 3 ? argv[2] : "my-adapter";
  std::cout << "# Scaffolding adapter: " << name
            << "\n"
               "#\n"
               "# Directory layout:\n"
               "#   "
            << name
            << "/\n"
               "#     schemas/          Action Schema JSON files\n"
               "#     executor.cpp      ActionExecutorPort implementation\n"
               "#     dry_run.cpp       DryRunPort implementation\n"
               "#     CMakeLists.txt    build integration\n"
               "#     test/             adapter contract tests\n"
               "#\n"
               "# Run the schema linter:     aetheris schema lint schemas/*.json\n"
               "# Run the eval harness:      aetheris eval run test/golden.jsonl\n"
               "# Run contract tests:        ctest -L contract\n";
  return EXIT_SUCCESS;
}

// ── eval run ─────────────────────────────────────────────────────────────────

struct EvalCase {
  std::string id;
  std::string description;
  std::string schema_json;
  bool expected_valid{true};
};

static int cmd_eval_run(int argc, char** argv) {
  // argv[0] = "run", argv[1] = dataset file (argc-2, argv+2 dispatch)
  if (argc < 2) {
    std::cerr << "usage: aetheris eval run <dataset.jsonl>\n";
    return EXIT_FAILURE;
  }
  const fs::path dataset_path{argv[1]};
  std::ifstream f{dataset_path};
  if (!f) {
    std::cerr << "error: cannot open dataset: " << dataset_path.string() << "\n";
    return EXIT_FAILURE;
  }

  int passed = 0, failed = 0;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    try {
      const auto doc = nlohmann::json::parse(line);
      EvalCase c;
      c.id = doc.at("id").get<std::string>();
      c.description = doc.value("description", "");
      c.schema_json = doc.at("schema_json").dump();
      c.expected_valid = doc.value("expected_valid", true);

      const auto result = infrastructure::parse_action_schema_json(c.schema_json);
      const bool is_valid = result.has_value();

      if (is_valid == c.expected_valid) {
        std::cout << "  ok  [" << c.id << "] " << c.description << "\n";
        ++passed;
      } else {
        std::cerr << " FAIL [" << c.id << "] " << c.description
                  << " (expected valid=" << (c.expected_valid ? "true" : "false")
                  << ", got valid=" << (is_valid ? "true" : "false") << ")\n";
        if (!is_valid) {
          std::cerr << "       " << domain::error_code(result.error()) << ": "
                    << domain::error_message(result.error()) << "\n";
        }
        ++failed;
      }
    } catch (const std::exception& ex) {
      std::cerr << " FAIL [parse error]: " << ex.what() << "\n";
      ++failed;
    }
  }

  std::cout << "\n" << passed << "/" << (passed + failed) << " eval cases passed\n";
  return failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

// ── audit verify ──────────────────────────────────────────────────────────────

static int cmd_audit_verify(int argc, char** argv) {
  // argv[0] = "verify", argv[1] = log file (argc-2, argv+2 dispatch)
  if (argc < 2) {
    std::cerr << "usage: aetheris audit verify <log.jsonl>\n";
    return EXIT_FAILURE;
  }
  const fs::path log_path{argv[1]};
  std::ifstream f{log_path};
  if (!f) {
    std::cerr << "error: cannot open: " << log_path.string() << "\n";
    return EXIT_FAILURE;
  }
  std::vector<domain::AuditChainNode> nodes;
  std::string line;
  std::uint64_t line_num = 0;
  while (std::getline(f, line)) {
    ++line_num;
    if (line.empty())
      continue;
    const auto node = infrastructure::parse_audit_node_json(line);
    if (!node.has_value()) {
      std::cerr << "error  " << log_path.string() << ":" << line_num << ": "
                << domain::error_code(node.error()) << "\n";
      return EXIT_FAILURE;
    }
    nodes.push_back(std::move(*node));
  }
  const auto result = infrastructure::verify_audit_chain(nodes);
  if (result.has_value()) {
    std::cout << "  ok  audit chain is intact: " << log_path.string() << "\n";
    return EXIT_SUCCESS;
  }
  std::cerr << "error  audit chain verification FAILED: " << log_path.string() << " ("
            << domain::error_code(result.error()) << ")\n";
  return EXIT_FAILURE;
}

// ── help / top-level ──────────────────────────────────────────────────────────

static constexpr std::string_view kHelp = R"(aetheris - Aetheris-IUI command-line tool

Usage:
  aetheris version
  aetheris schema lint [FILE...]
  aetheris schema generate <openapi-file>
  aetheris adapter new <name>
  aetheris eval run <dataset.jsonl>
  aetheris audit verify <log.jsonl>
  aetheris help

Options:
  --version, -v    print version and exit
  --help, -h       show this help

See also:
  aetheris-lint    focused schema linter (subset of 'aetheris schema lint')
  aetheris-audit   audit chain verifier  (subset of 'aetheris audit verify')
)";

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << kHelp;
    return EXIT_SUCCESS;
  }

  std::string_view cmd{argv[1]};

  if (cmd == "--version" || cmd == "-v" || cmd == "version") {
    std::cout << "aetheris-iui " AETHERIS_IUI_VERSION "\n"
              << "abi: " << application::kStableAbiVersion << "\n";
    return EXIT_SUCCESS;
  }

  if (cmd == "--help" || cmd == "-h" || cmd == "help") {
    std::cout << kHelp;
    return EXIT_SUCCESS;
  }

  if (cmd == "schema") {
    if (argc < 3) {
      std::cout << kHelp;
      return EXIT_SUCCESS;
    }
    std::string_view sub{argv[2]};
    if (sub == "lint")
      return cmd_schema_lint(argc - 2, argv + 2);
    if (sub == "generate")
      return cmd_schema_generate(argc - 2, argv + 2);
    std::cerr << "unknown schema subcommand: " << sub << "\n";
    return EXIT_FAILURE;
  }

  if (cmd == "adapter") {
    if (argc < 3) {
      std::cout << kHelp;
      return EXIT_SUCCESS;
    }
    std::string_view sub{argv[2]};
    if (sub == "new")
      return cmd_adapter_new(argc - 2, argv + 2);
    std::cerr << "unknown adapter subcommand: " << sub << "\n";
    return EXIT_FAILURE;
  }

  if (cmd == "eval") {
    if (argc < 3) {
      std::cout << kHelp;
      return EXIT_SUCCESS;
    }
    std::string_view sub{argv[2]};
    if (sub == "run")
      return cmd_eval_run(argc - 2, argv + 2);
    std::cerr << "unknown eval subcommand: " << sub << "\n";
    return EXIT_FAILURE;
  }

  if (cmd == "audit") {
    if (argc < 3) {
      std::cout << kHelp;
      return EXIT_SUCCESS;
    }
    std::string_view sub{argv[2]};
    if (sub == "verify")
      return cmd_audit_verify(argc - 2, argv + 2);
    std::cerr << "unknown audit subcommand: " << sub << "\n";
    return EXIT_FAILURE;
  }

  std::cerr << "unknown command: " << cmd << "\n";
  std::cerr << "run 'aetheris help' for usage\n";
  return EXIT_FAILURE;
}
