#include "aetheris/infrastructure/file_audit_sink.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <format>
#include <sstream>
#include <string>

#include "aetheris/domain/error.hpp"
#include "aetheris/infrastructure/json_lines_exporter.hpp"

namespace aetheris::infrastructure {

FileAuditSink::FileAuditSink(std::filesystem::path path, const int fd) noexcept
    : path_{std::move(path)}, fd_{fd} {}

FileAuditSink::~FileAuditSink() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

domain::Result<FileAuditSink> FileAuditSink::open(std::filesystem::path path) {
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
  if (fd < 0) {
    return domain::fail(domain::make_internal_error(
        "audit.sink.open_failed",
        std::format("Cannot open audit sink '{}': {}.", path.string(), std::strerror(errno))));
  }
  return FileAuditSink{std::move(path), fd};
}

domain::Result<void> FileAuditSink::append(const domain::AuditChainNode& node) {
  const std::string line = serialize_audit_node_json(node) + '\n';

  const auto* ptr = line.data();
  std::size_t remaining = line.size();
  while (remaining > 0) {
    const auto written = ::write(fd_, ptr, remaining);
    if (written < 0) {
      return domain::fail(domain::make_internal_error(
          "audit.sink.write_failed", std::format("Write to audit sink '{}' failed: {}.",
                                                 path_.string(), std::strerror(errno))));
    }
    ptr += static_cast<std::size_t>(written);
    remaining -= static_cast<std::size_t>(written);
  }

  if (::fsync(fd_) != 0) {
    return domain::fail(domain::make_internal_error(
        "audit.sink.fsync_failed",
        std::format("fsync on audit sink '{}' failed: {}.", path_.string(), std::strerror(errno))));
  }

  ++record_count_;
  return {};
}

} // namespace aetheris::infrastructure
