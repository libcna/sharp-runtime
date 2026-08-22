<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Audit findings reconciliation against `next`

*2026-08-22.* The audit index is a current disposition ledger; the per-file
`*.audit.md` reports preserve evidence observed at audit time and are not a
claim that their pre-repair source still exists. This pass compared every one
of the 82 entries that had remained `confirmed` or `confirmed (design-complete)`
with the current `next` sources and their completed tickets.

## Result

| Disposition after reconciliation | Count |
| --- | ---: |
| `remediated` | 293 |
| `confirmed` | 43 |
| `confirmed (design-complete)` | 28 |
| **total** | **364** |

Eleven entries had stale statuses and are now `remediated` in
`audit/AUDIT_FINDINGS_INDEX.md`:

| Finding | Closure |
| --- | --- |
| SR-AUD-012 | Defined full-domain `RandomNumberGenerator::GetInt32` arithmetic, this pass |
| SR-AUD-146, 150, 151 | `UriParser::Register`, `Uri::GetLeftPart`, `Uri::CheckHostName` — #1997 |
| SR-AUD-204, 210 | Writer preference and Barrier phase access — #1957 |
| SR-AUD-237 | `ReadOnlyObservableCollection` forwarding-callback lifetime, this pass |
| SR-AUD-240 | Defined ALPN hashing — #1838 |
| SR-AUD-245 | `Regex`/`Match::NextMatch` lifetime, this pass |
| SR-AUD-280 | Per-thread `CultureInfo` state — #2409 |
| SR-AUD-326 | Functional JsonDocument parsing options — #2115 |

SR-AUD-203 deliberately remains `confirmed`: #1955 repaired its concurrent
`disposed_` access, but disposal while the caller owns a lock is a distinct
remaining contract. SR-AUD-153 likewise remains confirmed only for the
documented absence of `FrameworkDescription`; `RuntimeIdentifier` already
landed under #1980. The remaining 69 records were checked against their current
source/ticket disposition and have no stale closure claim.

## Documentation gate

The Doxygen script's checked baseline is **2,675 warnings** for Doxygen 1.9.8,
not the obsolete 1,942 figure from an earlier record. This pass makes
`scripts/local_ci_check.sh` run `scripts/check_doxygen_warnings.sh`, so the
same bounded warning gate now runs locally and in CI. Reducing the existing
documentation backlog remains welcome maintenance work, but is no longer a
silently failing or unrun gate.
