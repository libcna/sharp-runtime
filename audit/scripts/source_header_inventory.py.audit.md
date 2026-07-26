# Audit: `scripts/source_header_inventory.py`

## Metadata

- Audit status: AUDITED (121 lines, full read).
- Subsystem: source/header inventory and planning diagnostics.
- Validation: run with `--csv /tmp/sharp-runtime-audit-source-header-inventory.csv`.

## Purpose

Inventories module public headers and implementation files, checks SPDX marker
presence, extracts best-effort namespaces/types, writes CSV output, and is
documented as cross-referencing the local planning database.

## Assessment

The audit run scanned 1,229 files (1,015 headers and 214 implementations),
found zero missing SPDX markers, and wrote its CSV safely outside the
repository.  Its filesystem traversal is limited to module `include` and
`src` trees, appropriate to the tool's stated source/header focus.

## Findings

### SR-AUD-004 — low — claimed planning cross-reference is not implemented

The module docstring promises to cross-reference headers with `plan.sqlite3`
so maintainers can find headers lacking task rows or `ported` rows lacking a
header.  The implementation only loads the set of `ported` names (lines
67–77) and prints its cardinality (line 114); it never compares that set with
the extracted file namespace/type data, emits unmatched entries, or changes
its exit status for a mismatch.

**Impact:** the generated CSV and “Tasks marked ported” count can suggest that
planning-to-source consistency was checked when no such assertion occurred.

**Follow-up evidence needed:** define a conservative C++-to-task mapping (with
documented exemptions for multi-type headers/templates), then report both
unmatched headers and `ported` task rows with no owned header.  The script
should make any intended consistency assertion explicit in its exit status.

## Final assessment

SPDX inventory behavior is useful and passed; the advertised database
cross-reference is incomplete (SR-AUD-004).
