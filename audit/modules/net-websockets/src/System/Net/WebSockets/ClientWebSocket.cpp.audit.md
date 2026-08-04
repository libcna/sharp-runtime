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

---

## Post-audit note — ticket #2091 (2026-08-04), no `SR-AUD-*` identifier

Appended, not rewritten. **This file's four payload-validation defects carry NO audit
identifier**: SR-AUD-251 is this report's only finding and concerns cancellation, and corrected
premise 6.5 of `docs/SystemNetWebSocketsNamespaceReviewPlan.md` records why — **the frame parser
was entirely unaudited**. Audit numbering stays frozen at 364.

Repaired by #2091, all four reproduced first
(`build-probe/2091_probe1_before.log`) and closed after (`..._after.log`):

1. a **1-byte close payload was silently ignored** (the guard was `payload.size() >= 2`) and
   reported as a **clean close** with no status;
2. **any** 16-bit value was cast straight to `WebSocketCloseStatus`, so 0, 999, 1004, 1005, 1006,
   1015 and 5000 all reached the public `getCloseStatusProperty()`;
3. Text payloads and close reasons were **never UTF-8 validated**;
4. the response's `Sec-WebSocket-Protocol` was stored unchecked, so the server chose the client's
   `SubProtocol` freely — even when the client had requested none.

**Two close parsers existed, not one.** `ReceiveAsync`'s `case 0x8` and `CloseAsync`'s
close-handshake loop each had their own copy of the same unvalidated block; repairing only the
receive path would have left the close handshake accepting malformed frames. Both now call one
`parseClosePayload`, and mutation N1b fails a test at **each** site.

**Premise corrected:** plan §7.7 says the getter "can return an enumerator that does not exist".
Half right — codes 3000–4999 are legal on the wire and have no enumerator in this port *or* in
.NET, whose enum names the same subset. That is inherent to the enum's design. What #2091 closes
is codes that can *never* be valid reaching the getter.

**Stated limit, pinned by a test:** a **fragmented** Text message is deliberately not UTF-8
validated, because a scalar may straddle a fragment boundary and validating properly needs
incremental decoder state on the object — an object-layout change this compatible ticket does not
make.

**Recorded, not changed:** `sendCloseFrame` does not validate a *caller*-supplied close code or
reason, so an application can still send what this client would reject on receipt. A caller-side
door, outside this ticket's remote-input subject.

**SR-AUD-251 remains confirmed** and blocked as #2093. No change here touched cancellation.
