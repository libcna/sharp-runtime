# Audit: `modules/net/src/System/Net/IPNetwork.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [IPNetwork.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/IPNetwork.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

Prefix masking and IPv4-mapped membership behavior were reviewed against the
reference implementation.  Reconstructing the masked address loses IPv6 scope
metadata even when no address byte changes.

### SR-AUD-303 — medium — `IPNetwork` discards an already-normalized IPv6 base address's scope ID

`IPNetwork(IPAddress::Parse("fe80::%7"), 64)` reports base scope `0` and
formats as `fe80::/64`; .NET's `ClearNonZeroBitsAfterNetworkPrefix` returns the
original `IPAddress` when the bytes are already normalized, preserving scope
identity and formatting.  C++ always rebuilds from bytes, whose constructor
defaults the scope to zero.

Required remediation: return the original address when masking makes no byte
change, and preserve scope when a reconstructed IPv6 address is required.

## Missing assertions and diagnostics

`NetTests.cpp` asserts ordinary IPv4 and unscoped IPv6 networks only.  Add
scoped normalized and masked IPv6 base-address/format/equality coverage.

## Final assessment

Confirmed IPv6 scope-metadata loss.

---

## Correction and remediation record — ticket #2038, 2026-08-04

*Everything above is the original audit text and is preserved verbatim. This section is
appended, not a rewrite.* Evidence: `build-probe/2038_probe1_before_after.log`, one probe source
built twice with `IPNetwork.cpp` and `IPAddress.cpp` compiled **from source**.

### The assessment is correct, and its consequence is sharper than stated

`IPNetwork(fe80::1%7, 64).BaseAddress.ScopeId` was **0** at every prefix length — 0, 1, 63, 64,
127 and 128 all measured — and `ToString()` rendered `fe80::/64`. What the report does not name
is the consequence: two networks on **different links** compared **equal** and **hashed equal**,

```
fe80::1%7/64 == fe80::1%9/64   ->  true   (hash equal: yes)
```

so a map or set keyed on `IPNetwork` silently merged them. No `SR-AUD-*` identifier was issued
for the extension; numbering stays frozen at 364.

### The half that would have broken silently — `Contains`

`IPAddress::operator==` compares the scope id for IPv6. `Contains` compared
`IPAddress(candidateBytes) == baseAddress_`, and the candidate is rebuilt from bytes so it always
carries scope 0. Preserving the base's scope while leaving that line alone would therefore have
made containment scope-**sensitive** and broken three measured rows that are `true` today —
including `fe80::1%7/64 Contains(fe80::1%7)`, the base address failing to be inside its own
network.

`Contains` now compares masked **bytes**. That reproduces **all fifteen** probed answers exactly
(the object comparison already reduced to a byte comparison, because both sides always carried
scope 0), and it is the correct semantics independently: a scope id names a link, not a network
prefix.

### Remediation

- `clearNonPrefixBits` rebuilds an IPv6 result through the 16-byte + scope constructor, keeping
  the scope id it was handed; IPv4 keeps the byte-vector constructor.
- `Contains` compares masked address **bytes**.

**Two observable changes on working calls**, both intended:

| Call | Before | After |
|---|---|---|
| `IPNetwork(fe80::1%7, 64).BaseAddress.ScopeId` | 0 | **7** |
| `IPNetwork(fe80::1%7, 64).ToString()` | `fe80::/64` | **`fe80::%7/64`** |
| `IPNetwork(fe80::1%7,64) == IPNetwork(fe80::1%9,64)` | `true` | **`false`** |
| every IPv4 / scope-0 IPv6 network | — | **identical** |
| all fifteen `Contains` rows | — | **identical** |

No sanitizer applies here — nothing is allocated, indexed, shared or converted out of domain.
Stated rather than silently skipped.

**Tests:** `modules/net/tests/System/Net/IPNetworkScopeTests.cpp` (8), including the fifteen-row
`Contains` table transcribed from the pre-repair probe log, the base-inside-its-own-network
property at six prefix lengths, and the equality/hash consequence.

**Consequences:** no signature, `noexcept`, virtual, vtable, data member or object-layout change.
Relink-only — `IPNetwork.hpp` was not touched.

Status: `confirmed` → **`remediated`**.
