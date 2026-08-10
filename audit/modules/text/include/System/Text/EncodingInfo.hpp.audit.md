# Audit: `modules/text/include/System/Text/EncodingInfo.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

`GetEncoding` unconditionally returns UTF-8 even when the object advertises a
different public code page and name. The reduction is documented but creates a
successful wrong conversion rather than an unsupported-code-page diagnostic.

### SR-AUD-299 — medium — EncodingInfo ignores its declared code page

An `EncodingInfo(20127, "us-ascii", ...)` is indistinguishable at conversion
time from UTF-8. Callers receive a valid object with different byte behavior
than the metadata they selected.

## Finding references

- SR-AUD-299 — medium — code-page metadata and returned encoding diverge.

## Other missing assertions and diagnostics

- Test known implemented code pages, unknown code pages, provider-backed
  entries, name/code-page consistency, and diagnostic type.

## Final assessment

SR-AUD-299 applies.
