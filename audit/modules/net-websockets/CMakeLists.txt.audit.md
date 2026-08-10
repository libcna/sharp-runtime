# Audit: `modules/net-websockets/CMakeLists.txt`

## Metadata

- AUDITED: static-library registration and public/private component boundary.
- Validation: `SharpRuntimeTests_Net_WebSockets` built successfully with four
  jobs; 22/24 tests passed and the two loopback transport tests were blocked at
  sandbox `socket()` creation.

## Assessment

The static target declares the socket, task, threading, URI, and component
dependencies required by the implementation while keeping `Net` private.  The
generated catalogue matches this boundary.

## Other missing assertions and diagnostics

- Add a header-consumer build plus a network-permitted loopback gate for the
  two currently environment-blocked fixture cases.
- Record the TLS-free `ws://` scope in component-level diagnostics rather than
  allowing a static WebSocket target name to imply `wss://` support.

## Final assessment

The module dependency boundary is coherent. No source or test was changed.
