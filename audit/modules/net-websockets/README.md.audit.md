# Audit: `modules/net-websockets/README.md`

## Metadata

- AUDITED: compiled-component scope and declared public dependencies.
- Evidence: CMake registration, public headers, implementation, tests, and
  current local .NET sources were inspected.

## Assessment

The README accurately describes a compiled WebSocket component and lists its
dependency surface.  It should make the implemented `ws://`-only boundary,
network-gated test condition, and the asynchronous ownership restrictions more
visible to consumers.

## Other missing assertions and diagnostics

- Document lack of TLS/compression and the current lack of effective
  cancellation/keep-alive behavior until SR-AUD-251/252 are remediated.
- Cross-link a network-permitted test command and the raw-object lifetime risk
  tracked by SR-AUD-247.

## Final assessment

The basic metadata is accurate but incomplete for operational constraints. No
source or test was changed.
