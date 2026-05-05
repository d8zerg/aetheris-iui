#pragma once

#include <cstdint>
#include <filesystem>

#include "aetheris/domain/audit.hpp"
#include "aetheris/domain/ports/audit_sink_port.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::infrastructure {

/**
 * File-backed AuditSinkPort implementation.
 *
 * Writes each AuditChainNode as a single JSON line followed by a newline.
 * Calls fsync after every write to guarantee durability before returning Ok.
 *
 * The file is opened in append mode; existing content is preserved.
 * Multiple processes must not write to the same file concurrently.
 */
class FileAuditSink final : public domain::AuditSinkPort {
 public:
  /**
   * Opens (or creates) the sink file at the given path.
   *
   * Returns an error if the file cannot be opened or created.
   */
  [[nodiscard]] static domain::Result<FileAuditSink> open(std::filesystem::path path);

  FileAuditSink(FileAuditSink&&) noexcept = default;
  FileAuditSink& operator=(FileAuditSink&&) noexcept = default;
  ~FileAuditSink() override;

  [[nodiscard]] domain::Result<void> append(const domain::AuditChainNode& node) override;
  [[nodiscard]] std::uint64_t size() const noexcept override {
    return record_count_;
  }

 private:
  FileAuditSink(std::filesystem::path path, int fd) noexcept;

  std::filesystem::path path_;
  int fd_{-1};
  std::uint64_t record_count_{0};
};

} // namespace aetheris::infrastructure
