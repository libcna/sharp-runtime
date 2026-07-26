# Audit: `modules/net-sockets/tests/System/Net/Sockets/SocketTests.cpp`

## Metadata

- AUDITED: Socket TCP/UDP I/O, flags, bounds, options, poll, and async fixture.
- Validation: compiled; 17 socket-dependent tests failed at construction under
  sandbox `socket()` denial, not as a source regression.

## Assessment

The tests cover several prior offset/flag/poll fixes but omit async destruction
and null receive-buffer safety (SR-AUD-263), invalid Poll/Shutdown state, and
Unix domain maximum/abstract paths.

## Other missing assertions and diagnostics

- Re-run unchanged in a network-permitted gate; add ASan/TSan lifetime tests
  and diagnostic assertions for all invalid public arguments.

## Final assessment

Environment-limited; SR-AUD-263 coverage is missing. No source or test changed.
