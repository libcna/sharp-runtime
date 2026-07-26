# Audit: `test/consumer/text_json.cpp`

## Metadata

- Audit status: AUDITED (7 lines, full read).
- Role: direct Text.Json public-header smoke consumer.

## Assessment

The fixture gives the selective matrix a representative `JsonDocument` public
include.  Its complementary forbidden fixtures assert that collections and
object-model headers do not leak into this closure.

## Final assessment

No fixture-local finding.
