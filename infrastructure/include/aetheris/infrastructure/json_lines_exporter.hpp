#pragma once

#include <iosfwd>

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/ports/audit_exporter_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * AuditExporterPort implementation that writes JSON Lines (ndjson) to a stream.
 *
 * Each AuditChainNode is serialized as a single compact JSON object followed by
 * a newline. Chain hashes are encoded as lower-case hex strings. Compatible with
 * standard JSON Lines readers and log ingestion pipelines.
 */
class JsonLinesExporter final : public domain::AuditExporterPort {
 public:
  explicit JsonLinesExporter(std::ostream& out) noexcept;

  [[nodiscard]] domain::Result<void> export_record(const domain::AuditChainNode& node) override;
  [[nodiscard]] domain::Result<void> flush() override;

 private:
  std::ostream& out_;
};

/**
 * Serializes a single AuditChainNode to a compact JSON string.
 *
 * Used by JsonLinesExporter and the verify CLI.
 */
[[nodiscard]] std::string serialize_audit_node_json(const domain::AuditChainNode& node);

/**
 * Parses a JSON string back into an AuditChainNode.
 *
 * Returns InputError on malformed JSON; does not re-verify the hash chain.
 */
[[nodiscard]] domain::Result<domain::AuditChainNode>
parse_audit_node_json(std::string_view json_text);

} // namespace aetheris::infrastructure
