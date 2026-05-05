# ADR-0005: Hash Chain Instead of Per-Record Digital Signature

Status: accepted

## Context

The Audit & Decision Log must provide tamper-evidence: any alteration of a historical
record must be detectable. Two common approaches exist:

1. **Per-record digital signature** - each record is signed with a private key; a
   verifier with the corresponding public key can authenticate any individual record
   independently.

2. **Hash chain (Merkle-style)** - each node stores the SHA-256 of its own content
   and a chain hash that binds it to all previous nodes. Tampering with any node
   breaks every subsequent chain hash.

## Decision

We use a hash chain with no per-record signature.

Each `AuditChainNode` stores:

- `record_hash` = SHA-256(canonical bytes of the `DecisionRecord`)
- `chain_hash` = SHA-256(`record_hash` ∥ `prev_chain_hash`)
- `prev_chain_hash` - the `chain_hash` of the immediately preceding node (zero for the
  first node)

Verification is a linear scan: recompute both hashes for each node and check that
`prev_chain_hash` equals the previous node's `chain_hash`. Any mutation anywhere in
the chain breaks verification from the tampered node onward.

The canonical record serialization is a newline-delimited concatenation of all record
fields in a fixed order, deterministic across platforms and compiler versions.

SHA-256 is implemented in pure C++23 (FIPS 180-4) with no external dependencies, making
the implementation fully auditable and portable without requiring a cryptography library.

## Consequences

**Good:**
- Zero key-management overhead. There is no private key to generate, rotate, store, or
  protect. Operational simplicity is significant for an embeddable library.
- Verification is self-contained: the verifier needs only the log file and the algorithm.
  No online certificate infrastructure, no key distribution.
- Append-only enforcement is natural: adding a node is cheap (one SHA-256 call); rewriting
  history is computationally infeasible without breaking all subsequent chain hashes.
- The hash chain can be anchored to an external store (S3, blockchain, notary) by publishing
  the current `chain_hash` at checkpoints, enabling third-party verification without
  exposing the raw records.

**Trade-offs:**
- An attacker who controls the entire log file can rebuild the chain after tampering, because
  there is no external root of trust. Per-record signatures would prevent this.
  Mitigation: periodic anchoring of `chain_hash` to an external immutable store, which is
  supported by the `AuditExporterPort` abstraction and deferred to a later step.
- Individual records cannot be authenticated in isolation; the entire chain from the first
  node must be present and unmodified for verification to succeed. This is acceptable because
  audit verification is a batch operation performed by the compliance team, not an online
  check on every request.
- Revoking a specific record's privacy (e.g., a GDPR erasure request) requires rebuilding
  the chain from that node onward. PII redaction is therefore applied before the record is
  hashed, not afterward.

## Alternatives Considered

**Per-record HMAC or digital signature:** Provides individual record authenticity and
supports independent record verification without the full chain. Rejected for v0: introduces
key management complexity (HMAC shared secret or asymmetric key rotation) that significantly
increases operational burden for embedded deployments. Can be layered on top of the hash
chain in a later step if required by compliance.

**No tamper-evidence (plain append-only file):** Simplest to implement. Rejected: an
append-only file without integrity checking provides no evidence of tampering - a privileged
attacker can silently overwrite records. Tamper-evidence is a hard requirement for the
compliance use case that motivates the audit log.
