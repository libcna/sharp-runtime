# Audit: `scripts/index_dotnet_types.py`

## Metadata

- Audit status: AUDITED (91 lines, full read; not executed because its default
  behavior deletes and recreates a database outside this repository).
- Subsystem: .NET source type indexer.
- Evidence: source, current repository path, `prompt.md`, and read-only check
  that `/rv/data/development/github.com/openeggbert/sharp-runtime/` exists.

## Purpose

Scans local dotnet/runtime C# sources, extracts public type declarations, and
creates a SQLite index.

## Findings

### SR-AUD-005 — medium — default output points at a different checkout and is destructively recreated

`DB_PATH` is hardcoded to
`/rv/data/development/github.com/openeggbert/sharp-runtime/dotnet_types.db`
(line 14), while this repository is `sharp-runtimervc` and the documented
planning database is `plan.sqlite3` in this checkout.  On every invocation the
script unconditionally removes that external path if it exists (lines 53–55)
before recreating it.

The referenced sibling directory exists in the audit environment, so this is
not a harmless impossible path.  The database was absent during the read-only
check, so no external state was changed by the audit.

**Impact:** a maintainer following this script can create or overwrite an
unrelated checkout's `dotnet_types.db`, while the output has no relation to
the current repository's documented `plan.sqlite3` workflow.

**Follow-up evidence needed:** decide whether the index is still maintained.
If it is, derive a repository-local default from `__file__`, add an explicit
output argument, and refuse destructive replacement without a deliberate
flag or a temporary-file/atomic-replace workflow.

## Final assessment

Confirmed stale/destructive developer-tool default, indexed as SR-AUD-005.
