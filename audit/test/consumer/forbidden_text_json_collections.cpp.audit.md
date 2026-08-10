# Audit: `test/consumer/forbidden_text_json_collections.cpp`

## Metadata

- Audit status: AUDITED (8 lines, full read).
- Role: deliberate negative include-leakage fixture.

## Assessment

The source combines `Text.Json` with a `Collections.Core` header solely so the
consumer CMake harness can require its compilation to fail when Text.Json is
correctly isolated.  It is not a runtime consumer and must remain intentionally
unbuildable in that negative configuration.

## Final assessment

No fixture-local finding.
