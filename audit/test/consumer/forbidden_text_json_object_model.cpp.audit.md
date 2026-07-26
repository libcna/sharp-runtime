# Audit: `test/consumer/forbidden_text_json_object_model.cpp`

## Metadata

- Audit status: AUDITED (8 lines, full read).
- Role: deliberate Text.Json/ObjectModel include-leakage negative fixture.

## Assessment

The two includes intentionally assert that ObjectModel is absent from the
Text.Json consumer closure.  The CMake harness, not this source's return path,
supplies the expected-failure assertion.

## Final assessment

No fixture-local finding.
