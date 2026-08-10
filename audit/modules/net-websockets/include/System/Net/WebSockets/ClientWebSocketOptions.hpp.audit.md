# Audit: `modules/net-websockets/include/System/Net/WebSockets/ClientWebSocketOptions.hpp`

## Metadata

- AUDITED: request-header/subprotocol validation, immutable-after-connect
  transition, buffers, and keep-alive configuration.
- Validation: direct C++/current-.NET probes covered CR/LF header input and
  `chat,evil` subprotocol input.

## SR-AUD-248 — high — SetRequestHeader accepts CR/LF and serializes caller text directly into the HTTP upgrade request

The C++ probe accepts value `ok\r\nInjected: yes`; current .NET throws
`ArgumentException`.  `performHandshake` subsequently writes map key/value
text as `name + ": " + value + "\r\n"`, permitting the unvalidated newline
to create a second handshake header on the wire.  This is a request-header
injection boundary, not merely a cosmetic validation difference.

## SR-AUD-249 — medium — AddSubProtocol accepts HTTP-token separators that .NET rejects

The C++ probe accepts `chat,evil`, while current .NET rejects it with
`ArgumentException` through its token validation.  The implementation checks
only control/space bytes and DEL, then joins requests with commas, so a single
invalid caller string can alter the advertised protocol list.

## SR-AUD-252 — medium — configured keep-alive interval and timeout are stored but never affect a ClientWebSocket connection

Repository-wide `keepAlive` use ends in these getters/setters; no connection
or frame path reads either stored option.  Current .NET uses its values to
choose ping/pong keep-alive behavior and response timeout.  The public
configuration therefore reports successful setup but has no transport effect.

## Assessment

Read-only transition, positive buffer validation, and ASCII duplicate checks
are present.  Header token validation, header-value sanitization, and actual
keep-alive consumption are absent.

## Other missing assertions and diagnostics

- Add CR/LF/NUL/name-token rejection, all RFC token separators, Unicode/ASCII
  duplicate pairs, and immutability-after-connect cases.
- In a network-permitted fixture, assert outbound headers contain no injected
  line and configured keep-alives produce the documented ping/pong behavior.

## Final assessment

SR-AUD-248, SR-AUD-249, and SR-AUD-252 are confirmed. No source or test was
changed.

---

## Post-audit remediation record — ticket #2089 (2026-08-04)

Appended, not rewritten: the original assessment above is retained verbatim.

**SR-AUD-248 — remediated.** Reproduced exactly as filed
(`build-probe/2089_probe1_before.log`): a six-field handshake request became an **eight-field**
one carrying a smuggled `GET /admin HTTP/1.1`, because `performHandshake` concatenates
`name + ": " + value + "\r\n"` into the upgrade request verbatim. Both `headerName` and
`headerValue` now reject CR, LF and NUL at the door, before any socket exists.

**Scope correction the finding does not contain.** The finding names `SetRequestHeader`; measured,
the **request URI is a third door**. The request line is built from
`uri.getPathAndQueryProperty()` and `Host:` from `uri.getHostProperty()`, and `System::Uri`
preserves CR, LF and NUL in both components (`build-probe/2089_probe2_uri_door.log`), so
`ws://127.0.0.1:P/a\r\nX-Injected:+yes` put `GET /a` on the request line and
`X-Injected: yes HTTP/1.1` into a header field — request smuggling, not one extra field. Closing
only the two option doors would have marked this finding remediated with the smuggling vector
still open. This is the **same** door #2063 found missing from SR-AUD-313's paraphrase, now in a
second namespace. `System::Uri` itself is deliberately **not** modified — the Uri-side defect is
the separate, still-blocked ticket #2003.

**SR-AUD-249 — remediated, with its premise corrected in the port's favour and against it at
once.** A subprotocol validator **did** already exist and already rejected `c <= 0x20` and `0x7F`,
so the finding is correct in its example and wrong in implying no validation existed: the repair
**widens** an existing check rather than creating one. Measured before, **not one** of the
seventeen RFC 7230 separators `( ) < > @ , ; : \ " / [ ] ? = { }` was rejected. All seventeen now
are, while every RFC 7230 `tchar` stays accepted and the pre-existing empty/space/tab/DEL
rejections and the case-insensitive duplicate check are unchanged. A subprotocol never was a
CR/LF/NUL door — all three are `<= 0x20` and were already caught.

**One predicate body, not a second policy.** The rule moved down to
`System::Net::detail::ContainsProtocolFieldTerminator` in the `Net` component both protocol
modules already depend on; `System::Net::Http::detail::ContainsProtocolControlCharacter` is kept
as a forwarder, so no `System::Net::Http` call site, exception type or message changed (181/181
green). The module graph is unchanged at 41 modules and 91 edges.

**Exception identity is this port's choice**, not a match to .NET (`/rv/tmp/runtime/` re-verified
absent 2026-08-04): `System::ArgumentException`, because the sibling `AddSubProtocol` check in
this same class already reported invalid characters as an argument error. The rejected text is
never echoed into the message.

**Evidence.** +16 tests, add-only (`SharpRuntimeTests_Net_WebSockets` 39 → 55), wire-level cases
over a real loopback socket with no sleeps. Three mutations discriminating exactly 5 / 2 / 3
tests, each reverted from an exact backup. Descriptor accounting — not LSan, which tracks memory
rather than descriptors — shows a `/proc/self/fd` delta of 0 over 20 rejected connections with
all 20 throwing. ASan/UBSan/LSan clean over 52 rejections and 19 acceptances with
`ClientWebSocket.cpp` compiled from source, plus a control heap-buffer-overflow proving the
instrumentation was live.

**SR-AUD-252 remains confirmed** and blocked as #2094. No source change touched it.
