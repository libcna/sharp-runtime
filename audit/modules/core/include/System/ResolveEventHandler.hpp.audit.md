# Audit: `modules/core/include/System/ResolveEventHandler.hpp`

## Metadata

- Audit status: AUDITED (17-line handler alias, fully read).
- Validation: `EventHandlerTypeTests.ResolveEventHandler_Callable` passed in
  the 33-test related event filter on 2026-07-26.
- Reference basis: local .NET `System/ResolveEventHandler.cs:8`.

## SR-AUD-123 — medium — ResolveEventHandler has no representation for .NET's null “not resolved” result

Current .NET declares `ResolveEventHandler` as returning nullable `Assembly?`:
a handler returns `null` when it cannot resolve the request.  The C++ alias is
`std::function<std::string(void*, ResolveEventArgs&)>`
(`ResolveEventHandler.hpp:14-16`), which always returns a string and documents
no optional/sentinel policy.  An empty string is already used by
ResolveEventArgs for absent requesting assembly and cannot be distinguished by
the type from a deliberately returned empty name.  Thus a caller cannot
reliably model the resolution-failure branch even within the string-based
reflection adaptation.

The sole test returns the requested nonempty name; it does not call an empty
handler, model failure, or connect the delegate to the AppDomain resolve APIs,
which are currently stubs under SR-AUD-103.

## Other missing assertions and diagnostics

- Missing `optional<string>`/failure representation, empty string, default
  `std::function`, exception, sender type, and argument mutability vectors.
- No compile-time documentation prevents callers from interpreting an empty
  result as a successful assembly identity.

## Final assessment

The alias is callable but loses its fundamental nullable-result signal.  No
source or test was modified during this audit.
