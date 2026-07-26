# Audit: `modules/net-websockets/src/System/Net/WebSockets/ClientWebSocket.cpp`

## Metadata

- AUDITED: RFC handshake, frame send/receive, state transitions, asynchronous
  task capture, cancellation, and keep-alive use.
- Validation: ASan directly reports `ConnectAsync` stack-use-after-scope;
  current-.NET/C++ probes cover header validation and pre-canceled connect.
  The two loopback fixture cases are environment-blocked by `socket()` policy.

## SR-AUD-247 — high — asynchronous ClientWebSocket tasks dereference destroyed raw owners

Every operation lambda captures raw `this`; the ASan repro destroys a stack
client then waits for its `wss://` connect task.  The worker reads the dead
client in the lambda at line 222.  `Dispose` cannot make raw memory valid or
join the operation, so send, receive, and close share the same ownership
pattern.

## SR-AUD-248 — high — unvalidated request headers permit HTTP upgrade header injection

`performHandshake` interpolates every options map entry into the request
without validation.  The C++ probe accepts a value containing CR/LF where
current .NET's SetRequestHeader throws, so caller text can inject a distinct
HTTP handshake header.  See the options report for exact probe output.

## SR-AUD-251 — medium — every ClientWebSocket asynchronous operation ignores its cancellation token

All five method definitions name the token as an unused parameter and create
a tokenless `Task`.  With an already canceled token and `wss://` URI, C++
faults with PlatformNotSupportedException; current .NET returns
TaskCanceledException.  Existing receive/send/close calls have the same
source-level no-cancellation behavior.

## SR-AUD-252 — medium — configured keep-alive settings never reach any frame or connection logic

Only option getters/setters mention the configured interval/timeout; this
source never reads them.  It can answer peer Ping with Pong, but it never
initiates configured keep-alive Ping/Pong behavior or timeout enforcement.

## Assessment

The code performs a concrete SHA-1 accept check, client masking, bounded frame
length rejection, peer Ping/Pong response, and basic close framing.  It still
lacks tests for handshake `Connection` token/subprotocol selection and RFC
malformed-control/continuation/RSV/opcode/close-reason validation; those paths
could not be exercised without local sockets.

## Other missing assertions and diagnostics

- Add network-permitted malformed handshake/frame tests, concurrent send/receive
  sanitizer coverage, close code/reason UTF-8 limits, and cancellation during
  blocking I/O.
- Log state transitions and handshake headers only after redacting sensitive
  values; do not replace validation with diagnostic logging.

## Final assessment

SR-AUD-247, SR-AUD-248, SR-AUD-251, and SR-AUD-252 are confirmed at this
implementation boundary. No source or test was changed.
