# Audit: `modules/core/include/System/MulticastNotSupportedException.hpp`

## Metadata

- Audit status: AUDITED (43-line inline implementation, fully read).
- Validation: the focused five-type exception filter passed 29/29 on 2026-07-26.
- Reference basis: local .NET `MulticastNotSupportedException.cs` and `COR_E_MULTICASTNOTSUPPORTED` (`0x80131514`).

## Assessment

All three constructors assign the derived HResult after `SystemException`
construction. Normal default/message/inner and inheritance tests pass. No
standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Tests omit all HResult assertions, exact default text, stored-inner pointer identity/rethrow, and empty/UTF-8 messages.
- The audit found no native delegate-combine path that rejects a non-multicast delegate with this type, so normal constructor coverage does not establish operational integration.

## Final assessment

The inline constructor code is consistent with the documented diagnostic contract. No source or test was modified.
