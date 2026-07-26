# Audit manifest

`AUDIT_SCOPE.md` defines the deterministic inventory and exclusions.  This
manifest is the live, high-level work queue; reports themselves are the
per-file completion evidence.  A source path is **AUDITED** only after its
mirrored report exists and contains evidence rather than a boilerplate verdict.

| Shard | Eligible files | Audited | Status |
|---|---:|---:|---|
| Root build, policy, and planning documents | 8 | 8 | AUDITED |
| `docs/` architecture documents | 3 | 3 | AUDITED |
| CMake and GitHub Actions | 5 | 5 | AUDITED |
| `scripts/` | 9 | 9 | AUDITED |
| Consumer fixtures and validator tests | 16 | 16 | AUDITED |
| Integration tests | 7 | 7 | AUDITED |
| Benchmarks | 1 | 1 | AUDITED |
| 41 runtime modules (source, headers, tests, module docs/CMake) | 1,699 | 1,074 | IN PROGRESS |
| **Total** | **1,748** | **1,123** | **IN PROGRESS** |

The module shard is processed in dependency/risk order rather than directory
order: component boundaries and `Core.Base` first, then collections, IO/text,
threading, diagnostics/process, network, XML, numerics, globalization, and
platform-limited areas.  This ordering aims to expose cross-component defects
while their causal context is still fresh.

## Status vocabulary

- **PENDING** — included, no individual review yet.
- **IN PROGRESS** — evidence collection has started; incomplete reports are
  marked as such and never treated as final evidence.
- **AUDITED** — a complete mirrored report exists.
- **BLOCKED** — a report cannot be completed from local evidence; the blocker
  and a safe next action are recorded in `AUDIT_PROGRESS.md`.

No source changes are made as part of this audit.  Follow-up repairs are
created only after the audit's findings have been reconciled.
