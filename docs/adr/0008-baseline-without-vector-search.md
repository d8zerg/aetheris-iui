# ADR-0008: Knowledge Layer - Baseline Works Without Vector Search

Status: accepted

## Context

The aetheris-iui implementation plan introduces a Knowledge Layer for contextual enrichment of operator intents before LLM inference. The requirements include semantic similarity search via vector embeddings, but the platform must also work in constrained environments where:

- No GPU or ML runtime is available.
- An embedding model has not been configured or loaded.
- A vector database (Qdrant, Milvus, pgvector) is not provisioned.

We need to decide how to structure the knowledge sources so that the system degrades gracefully when vector search is unavailable, rather than failing hard.

## Decision

The Knowledge Layer is structured around three independent `KnowledgeSourcePort` implementations with explicit priority ordering:

| Priority | Source                 | Dependency                  | Baseline? |
|----------|------------------------|-----------------------------|-----------|
| 0        | `SchemaKnowledgeSource`| `ActionSchemaRegistry`      | Yes       |
| 1        | `GlossaryKnowledgeSource` | in-process glossary map  | Yes       |
| 2        | `VectorKnowledgeSource`| `EmbeddingsPort` + `VectorIndexPort` | Optional |

`KnowledgeOrchestrator` applies **graceful degradation**: it iterates sources in priority order and continues past any source that returns an error. If `VectorKnowledgeSource` fails (embedding unavailable, index dimension mismatch, backend timeout), the orchestrator silently skips it and returns results from the available sources. The caller receives an enriched result (potentially smaller), not an error.

`NullEmbeddings` always returns a zero vector. This lets `VectorKnowledgeSource` run without a real ML backend - queries produce equal-scoring results (random ordering), which is equivalent to no semantic ranking. The baseline is tested and produces useful results via schema and glossary sources.

The `InMemoryVectorIndex` implements exact cosine similarity search. It is used in tests and can serve small in-process corpora. Production deployments replace it with Qdrant/Milvus/pgvector adapters.

## Consequences

**Positive:**
- The platform ships a working knowledge enrichment pipeline from day one without any ML infrastructure.
- Schema and glossary sources provide deterministic, structured context that is often sufficient for well-defined action spaces.
- Tests do not require any external services or ML runtimes.
- Adding a vector backend is additive - no existing code changes.

**Negative:**
- Semantic similarity (fuzzy intent-to-schema matching) requires the optional vector stack to be fully effective.
- `NullEmbeddings` returns zero vectors; all indexed documents score equally and order is arbitrary. Callers must not rely on `VectorKnowledgeSource` ranking without a real embeddings backend.

## Alternatives Considered

**Fail fast when vector search is unavailable** - rejected. Enrichment is best-effort; the orchestration pipeline must not abort a session because a non-critical intelligence component is offline.

**Merge EmbeddingsPort and VectorIndexPort into a single port** - rejected. The two concerns (text-to-vector transformation and ANN storage) are independently swappable. Keeping them separate allows replacing the embedding model without changing the index, and vice versa.
