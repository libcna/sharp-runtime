# Audit: `modules/text/src/System/Text/Encoding.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

Each factory returns one mutable shared instance. The base `GetString` adds a
signed index directly to the raw pointer and returns empty for negative count
rather than failing. TSan reports a read/write race between the shared UTF-8
instance's fallback setter and decoding.

## Finding references

- SR-AUD-286 — high — signed raw decode ranges are not validated.
- SR-AUD-288 — high — mutable shared encoding factories are racy.

## Other missing assertions and diagnostics

- Test factory identity/read-only semantics, concurrent configuration and
  conversion, all base raw argument failures, and distinct fallback instances.

## Final assessment

SR-AUD-286 and SR-AUD-288 apply.
