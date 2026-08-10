# Audit: `modules/net-http/include/System/Net/Http/HttpResponseMessage.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [HttpResponseMessage.cs](/rv/tmp/runtime/src/libraries/System.Net.Http/src/System/Net/Http/HttpResponseMessage.cs).
- Evidence: `/tmp/sharp-runtime-net-http-audit/net_http_probe.cpp`.

## Assessment

The constructor and setter accept arbitrary enum casts, while the reference
rejects status values below zero or above 999.  Reason phrases also accept
embedded newlines.  The direct probe constructs both `-1` and `1000` and
retains each unchanged.

### SR-AUD-316 — medium — response messages accept out-of-range status values and unvalidated reason phrases

Invalid public status codes are preserved and can flow through
`EnsureSuccessStatusCode`; an embedded CR/LF reason is not rejected.  The
managed constructor/setter validate the [0,999] range and prohibit newline or
NUL reason phrases.

Required remediation: validate status-code range at all public writes, reject
invalid reason characters, and add matching argument/format diagnostics.

**SPLIT, reason-phrase half REMEDIATED — ticket #2063, 2026-08-04.**
*(Appended; the original finding text above is preserved verbatim.)*

The two clauses of this finding have different blast radii and are dispositioned
separately, following the SR-AUD-007 -> #1878/#1879 convention
(`docs/SystemNetHttpNamespaceReviewPlan.md` §4.4).

- **Reason phrase — REMEDIATED (#2063).** `setReasonPhraseProperty` rejects CR,
  LF and NUL with `System::FormatException`, naming the field and deliberately
  **not** echoing the rejected text (it is attacker-controlled and this message
  gets logged). `HttpClient::parseStatusLine` rejects a control-bearing status
  line with `HttpRequestException` **before** the setter is reached, so a
  malformed *response* still surfaces as this module's response-error type
  rather than as a caller-argument format error.
- **Status-code range — NOT remediated, blocked #2069.** `-1`, `0`, `1000` and
  `99999` all still construct and answer `false` from
  `getIsSuccessStatusCodeProperty()`. Rejecting them means either making a
  public constructor that today cannot fail **throw** — an exception-
  specification and semantic change — or replacing the parameter type;
  `EnsureSuccessStatusCode`'s message text would change with it. Nothing about
  that is approved. The current behaviour is **pinned** by
  `NetHttpGatedBehaviourPins.Pin2069_ResponseAcceptsAnyStatusNumber`, which was
  mutation-checked: a constructor that rejects a code outside 100–599 fails
  exactly that pin and nothing else.

`setHeader` on this same type also gained the CR/LF/NUL rejection under #2063,
for symmetry with `HttpRequestMessage::setHeader` (SR-AUD-313). The header map's
**case sensitivity** is a different finding, SR-AUD-315, and stays open under
blocked ticket #2068.

SR-AUD-316 therefore remains `confirmed` for its open half.

## Missing assertions and diagnostics

Tests cover only common enum values and reason display.  Add -1/1000,
embedded CR/LF/NUL, default reason behavior, and case-insensitive header lookup
coverage (SR-AUD-315).
