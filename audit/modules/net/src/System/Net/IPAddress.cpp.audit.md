# Audit: `modules/net/src/System/Net/IPAddress.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [IPAddress.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/IPAddress.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

IPv4/IPv6 parsing, formatting, mapping, and hash behavior were compared with
the checked-in .NET source.  The raw IPv6 scope-id entry points remain outside
the validation used by the parser.

### SR-AUD-301 — medium — raw IPv6 scope-id construction and mutation silently narrow invalid values

The array-and-scope constructor and `setScopeIdProperty` cast `longcs` directly
to `uint32_t`.  The local probe observes `-1` become `4294967295` and
`4294967296` become `0`.  Current .NET rejects negative scope IDs and values
greater than `UInt32.MaxValue` before narrowing (`IPAddress.ScopeId`).

Required remediation: share range validation between the constructor and
setter, preserving the existing IPv4 `SocketException` behavior.

## Missing assertions and diagnostics

Existing tests cover parser scope overflow, not constructor/setter negative or
`UInt32.MaxValue + 1` inputs.  Add explicit exception-type and unchanged-state
assertions.

## Final assessment

Confirmed public range-validation gap.

---

## Correction and remediation record — ticket #2036, 2026-08-04

*Everything above is the original audit text and is preserved verbatim. This section is
appended, not a rewrite.* Evidence: `build-probe/2036_probe1_before_after.log`, one probe source
built four ways (`{before, after} × {plain, UBSan}`) with `IPAddress.cpp` compiled **from
source** into every binary.

### The assessment is correct, and the narrowing is worse than "narrow"

The report says `-1` becomes `4294967295` and `4294967296` becomes `0`. Both reproduce. What the
report does not say is that the conversion is a **modulo-2^32 wrap, not a clamp**, so an
out-of-domain value can land on a small, entirely **plausible** scope id that a caller cannot
distinguish from one it asked for:

| `scopeId` in | Stored | `ToString()` |
|---|---|---|
| `LONGCS_MIN` | **0** | `fe80::1` |
| -4294967295 | **1** | `fe80::1%1` |
| -5 | 4294967291 | `fe80::1%4294967291` |
| -1 | 4294967295 | `fe80::1%4294967295` |
| 4294967296 | **0** | `fe80::1` |
| 4294967297 | **1** | `fe80::1%1` |
| `LONGCS_MAX` | 4294967295 | `fe80::1%4294967295` |

Two `IPAddress` objects constructed with `-1` and `4294967295` therefore compared **equal**.

No `SR-AUD-*` identifier was issued for this extension; numbering stays frozen at 364.

### The rest of the scope-id surface, inventoried

Every other door that reaches `addressOrScopeId_` on an IPv6 address is **uint32-sourced** and
cannot be out of range, so the finding is correctly scoped to the two raw doors — verified by
grep across `modules/`, not assumed:

- `tryParseIPv6`'s own `std::from_chars<uint32_t>`, which already failed the parse for
  `fe80::1%4294967296` and `fe80::1%-1`;
- `Dns::fromSockaddrIn6` (`sin6_scope_id`, a `uint32_t`);
- `SocketAddress::GetIPEndPoint` (four bytes reassembled into a `uint32_t`);
- `modules/net-sockets`' `Socket.cpp:169` (`a6->sin6_scope_id`).

### Remediation

One shared file-local helper is adopted by **both** doors, which is this report's own
instruction ("share range validation between the constructor and setter"):

```cpp
uint32_t validatedScopeId(longcs value, const char* paramName);   // [0, 4294967295]
```

- The constructor validates **before any member is assigned**, so a rejected construction leaves
  nothing half-initialised; `paramName` is `"scopeId"`.
- The setter keeps its **IPv4 family guard first** — an IPv4 address has no scope id at all, so
  the wrong-family answer is the more specific one and its `SocketException` is the pre-existing
  tested contract — then validates with `paramName = "value"`. A rejected set leaves the previous
  scope id in place.
- Each door reports its **own** parameter name, matching .NET's `nameof(...)` convention. Plan
  §7.3 wrote `"value"` for a row that combined both doors; that was shorthand, and the more
  accurate spelling is used here and recorded rather than silently adopted.

**UBSan is non-discriminating in both directions** and is recorded as a non-result, not as
evidence: a `longcs` → `uint32_t` narrowing is *implementation-defined*, not undefined, exactly
as the review plan §11 predicted for this conversion.

**Tests:** `modules/net/tests/System/Net/IPAddressScopeIdTests.cpp` (9) — ten out-of-domain
values and ten in-domain values on **both** doors, the exact `paramName` for each, the
no-mutation guarantee, the family guard, `ToString`, the parser's unchanged domain and the
equality consequence.

**Consequences:** no signature, `noexcept`, virtual, vtable, data member or object-layout change;
`sizeof(IPAddress)` is 24 before and after. Relink-only in ABI terms; the header gained
doc-comments stating the new contract.

Status: `confirmed` → **`remediated`**.
