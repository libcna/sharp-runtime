# Audit: `modules/core/include/System/IUtf8SpanFormattable.hpp`

## Metadata

- Audit status: AUDITED (70-line public interface, fully read).
- Supporting validation: `IUtf8SpanFormattableTests2.*` passed 2/2 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The non-generic shape correctly follows the .NET interface's lack of a
`TSelf` return type.  The deliberately distinct `TryFormatUtf8` name avoids
C++ ambiguity for a type that also implements the character formatting
interface.  The provider overload is virtual, const, and forwards to the
required three-argument operation without retaining the provider.  The header
does not itself index, convert, or write the destination span.

`intcs` is used for `bytesWritten`, matching the signed local span length
convention.  The declaration's behavior for malformed span metadata remains
an implementation responsibility and is governed by the shared Span boundary
finding `SR-AUD-043` when an implementation converts a signed length to an
unsigned size.

## Positive findings

- The direct fixture writes UTF-8 bytes to a mutable `Span<uint8_t>` and
  separately verifies provider-overload forwarding through a base reference.
- The default provider path reaches the derived three-argument override, so
  the tested virtual dispatch path is intact.

## Other missing assertions and diagnostics

- The tests cover a successful output but not a destination-too-small failure,
  empty destination, zero-byte result, or UTF-8 output longer than one byte.
- As with the character interface, a derived three-argument override hides the
  four-argument base overload for direct calls unless the derived type adds a
  `using` declaration.  The source test correctly calls via the base, but no
  compile regression documents this intentional C++ constraint.
- A provider object is not used, so an implementation that must observe
  culture-specific behavior has no direct regression fixture.

## Final assessment

The declaration is a coherent documented adapter and its provider-forwarding
path is directly exercised.  No evidence-backed defect was found and no source
or test was modified during this audit.
