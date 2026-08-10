# Audit: `modules/net/src/System/Net/IPEndPoint.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [IPEndPoint.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/IPEndPoint.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

Port range checks and normal IPv4/IPv6 parsing agree with the reference.  The
bracketed parser does not consume the grammar exactly.

### SR-AUD-302 — medium — bracketed endpoint parser accepts and discards text between `]` and the port delimiter

For `[::1]ignored:80`, `TryParse` locates the closing bracket and then searches
for any later colon, silently discarding `ignored`; the probe returns true and
produces `[::1]:80`.  The reference parser passes the complete address slice to
`IPAddressParser` and returns false for the same non-endpoint text.

Required remediation: require either end-of-input immediately after `]` or a
colon immediately after it, then consume the complete decimal port.

## Missing assertions and diagnostics

The endpoint tests cover valid bracketed input and invalid ports, but not
trailing/interstitial text after a closing IPv6 bracket.

## Final assessment

Confirmed permissive parser mismatch.

---

## Correction and remediation record — ticket #2037, 2026-08-04

*Everything above is the original audit text and is preserved verbatim. This section is
appended, not a rewrite.* Evidence: `build-probe/2037_probe1_before_after.log`, one probe source
built twice (before/after) with `IPEndPoint.cpp`, `IPAddress.cpp` and `SocketAddress.cpp`
compiled **from source**, exercising `Parse` and `TryParse` over the same 29 inputs.

### Correction — the finding names one shape; there are four

`[::1]ignored:80` → `[::1]:80` reproduces exactly. Three more shapes were measured that the
report does not name, all the same defect:

| Input | Before | Note |
|---|---|---|
| `"[::1]ignored:80"` | `[::1]:80` | the report's own case |
| `"[::1]ignored"` | `[::1]:0` | **no colon anywhere** — the port was simply cleared and success reported |
| `"[::1]x"` | `[::1]:0` | same |
| `"[::1] :80"` | `[::1]:80` | a literal **space** dropped |

The decisive comparison is inside the same function: `"1.2.3.4 :80"` was **rejected** by the
unbracketed branch. One `TryParse`, two answers for the same input shape — which is exactly why
this repair needed no external reference, and that matters because
`/rv/tmp/runtime/src/libraries/` is absent from this container.

No `SR-AUD-*` identifier was issued for the extension; numbering stays frozen at 364.

### Remediation

```cpp
const size_t afterBracket = closeBracket + 1;
if (afterBracket < s.size()) {
    if (s[afterBracket] != ':') return false;   // trailing text is no longer discarded
    portPart = s.substr(afterBracket + 1);
}
```

`Parse` and `TryParse` agree on **all 29** probed inputs before and after. Every previously
valid form parses **identically**, including `[::1]` → `[::1]:0`, `[fe80::1%7]:80`, and every
unbracketed form.

### One adjacent defect deliberately left open — ticket #2045

A trailing `':'` with no digits after it is accepted as port 0, in **both** branches:
`"[::1]:"` → `[::1]:0` and `"1.2.3.4:"` → `1.2.3.4:0`. That is cause N-C's *shape* — a parser
accepting text it does not understand — but not this finding's site, and rejecting it would
violate this ticket's own acceptance criterion that every currently-valid form still parse
identically. It is carried by ordinary inactive ticket **#2045**, blocked on evidence rather
than on effort, and both current results are **pinned** by
`IPEndPointParseTests.BracketedTrailingColonWithNoPort_StillAccepted`.

**Tests:** `modules/net/tests/System/Net/IPEndPointParseTests.cpp` (9).

**Consequences:** no signature, `noexcept`, virtual, vtable, data member or layout change;
`sizeof(IPEndPoint)` is 40 before and after. Relink-only — `IPEndPoint.hpp` was not touched.

Status: `confirmed` → **`remediated`**.
