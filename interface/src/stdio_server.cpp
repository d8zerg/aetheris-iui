/**
 * Minimal stdio JSON-lines server for IPC-based integration testing.
 *
 * Protocol (newline-delimited JSON):
 *   Client -> Server: {"type":"ping"}
 *   Server -> Client: {"type":"pong","version":"<AETHERIS_IUI_VERSION>"}
 *
 *   Client -> Server: {"type":"version"}
 *   Server -> Client: {"type":"version","version":"<AETHERIS_IUI_VERSION>","abi":<uint>}
 *
 *   Client -> Server: {"type":"snapshot","session":{...}}
 *   Server -> Client: {"type":"snapshot","snapshot":{...}}
 *                 or {"type":"error","message":"..."}
 *
 *   Client -> Server: {"type":"quit"}
 *   (server exits with code 0)
 *
 * All other message types produce {"type":"error","message":"unknown type"}.
 * The server reads from stdin, writes to stdout, logs to stderr.
 */
#include <cstdlib>
#include <iostream>
#include <string>

#include "aetheris/application/runtime_descriptor.hpp"
#include "aetheris/interface/session_snapshot.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

static json handle(const json& req) {
  const auto type = req.value("type", std::string{});

  if (type == "ping") {
    return {{"type", "pong"}, {"version", AETHERIS_IUI_VERSION}};
  }

  if (type == "version") {
    return {
        {"type", "version"},
        {"version", AETHERIS_IUI_VERSION},
        {"abi", aetheris::application::kStableAbiVersion},
    };
  }

  if (type == "snapshot") {
    try {
      const auto& session_doc = req.at("session");
      aetheris::interface::SessionSnapshot snap;
      snap.id = session_doc.at("id").get<std::string>();
      snap.action_id = session_doc.at("action_id").get<std::string>();
      snap.operator_id = session_doc.at("operator_id").get<std::string>();
      snap.tenant_id = session_doc.at("tenant_id").get<std::string>();
      snap.state = session_doc.at("state").get<std::string>();
      snap.confirmation_mode = session_doc.at("confirmation_mode").get<std::string>();
      snap.created_at_us = session_doc.at("created_at_us").get<std::int64_t>();
      snap.updated_at_us = session_doc.at("updated_at_us").get<std::int64_t>();

      if (session_doc.contains("clarification_question") &&
          !session_doc["clarification_question"].is_null()) {
        snap.clarification_question = session_doc["clarification_question"].get<std::string>();
      }
      if (session_doc.contains("preview_result_json") &&
          !session_doc["preview_result_json"].is_null()) {
        snap.preview_result_json = session_doc["preview_result_json"].get<std::string>();
      }
      if (session_doc.contains("archive_reason") && !session_doc["archive_reason"].is_null()) {
        snap.archive_reason = session_doc["archive_reason"].get<std::string>();
      }
      for (const auto& sl : session_doc.at("slots")) {
        aetheris::domain::Slot slot;
        slot.name = sl.at("name").get<std::string>();
        slot.required = sl.at("required").get<bool>();
        if (!sl.at("value_json").is_null()) {
          slot.value_json = sl["value_json"].dump();
        }
        snap.slots.push_back(std::move(slot));
      }

      const std::string result_str = aetheris::interface::snapshot_to_json(snap);
      const auto result_doc = json::parse(result_str);
      return {{"type", "snapshot"}, {"snapshot", result_doc}};
    } catch (const std::exception& ex) {
      return {{"type", "error"}, {"message", ex.what()}};
    }
  }

  if (type == "quit") {
    std::cerr << "[stdio_server] quit requested\n";
    std::cout << json{{"type", "bye"}}.dump() << '\n';
    std::cout.flush();
    std::exit(0);
  }

  return {{"type", "error"}, {"message", "unknown type: " + type}};
}

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  std::cerr << "[stdio_server] Aetheris-IUI " AETHERIS_IUI_VERSION " stdio server ready\n";

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty())
      continue;
    try {
      const json req = json::parse(line);
      const json resp = handle(req);
      std::cout << resp.dump() << '\n';
      std::cout.flush();
    } catch (const std::exception& ex) {
      const json err = {{"type", "error"}, {"message", ex.what()}};
      std::cout << err.dump() << '\n';
      std::cout.flush();
    }
  }
  return 0;
}
