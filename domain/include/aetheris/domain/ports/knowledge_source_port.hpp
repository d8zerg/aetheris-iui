#pragma once

#include <string>
#include <vector>

#include "aetheris/domain/knowledge.hpp"
#include "aetheris/domain/result.hpp"

namespace aetheris::domain {

/**
 * Abstract source of contextual knowledge entries.
 *
 * Each implementation covers one retrieval strategy:
 *   - Action Schema lookup (structured, deterministic)
 *   - Glossary (multilingual term definitions)
 *   - Vector similarity search (semantic, approximate)
 *
 * The KnowledgeOrchestrator queries all registered sources in priority order,
 * merges results, and continues past failures (degradation).
 */
class KnowledgeSourcePort {
 public:
  KnowledgeSourcePort() = default;
  KnowledgeSourcePort(const KnowledgeSourcePort&) = delete;
  KnowledgeSourcePort& operator=(const KnowledgeSourcePort&) = delete;
  KnowledgeSourcePort(KnowledgeSourcePort&&) = delete;
  KnowledgeSourcePort& operator=(KnowledgeSourcePort&&) = delete;
  virtual ~KnowledgeSourcePort() = default;

  /**
   * Stable identifier for this source; used for deduplication and telemetry.
   */
  [[nodiscard]] virtual std::string name() const noexcept = 0;

  /**
   * Ordering priority relative to other sources.
   * Lower value = higher priority; results from lower-priority sources are
   * placed after higher-priority ones in the merged output.
   */
  [[nodiscard]] virtual int priority() const noexcept = 0;

  /**
   * Retrieves contextual entries relevant to the query.
   *
   * Must not throw.  Returns DomainError on transient failures so the
   * orchestrator can apply degradation without aborting the whole enrichment.
   */
  [[nodiscard]] virtual Result<std::vector<KnowledgeEntry>>
  retrieve(const KnowledgeQuery& query) noexcept = 0;
};

} // namespace aetheris::domain
