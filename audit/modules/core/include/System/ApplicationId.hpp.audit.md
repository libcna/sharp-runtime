# Audit: `modules/core/include/System/ApplicationId.hpp`

## Metadata

- Audit status: AUDITED (108-line inline value implementation, fully read).
- Validation: `ApplicationIdTests.*` passed 16/16 in the 22-test identity
  filter on 2026-07-26.
- Reference basis: local .NET `System/ApplicationId.cs:9-83`.

## SR-AUD-124 — medium — ApplicationId replaces binary/null-aware identity fields with undocumented strings and skips required name validation

Current .NET takes and clones a `byte[]` public-key token, rejects null/empty
Name, and permits nullable Culture/ProcessorArchitecture.  This port instead
takes all three as mandatory `std::string` values (`ApplicationId.hpp:35-42`),
accepts an empty name, and documents neither byte encoding nor a null/empty
sentinel.  Arbitrary binary key material and the distinction between a null and
an explicitly empty optional component are therefore unrepresentable or
ambiguous.

The green tests use only printable `"token123"`, nonempty name, and
`"neutral"`/`"amd64"`; they never challenge constructor validation, binary
bytes, empty/null adaptation, or copy isolation of token storage.

## SR-AUD-125 — medium — ApplicationId.ToString omits the public-key token and uses a different identity grammar

Current .NET writes lowercase quoted `culture`, `version`, `publicKeyToken`
(uppercase hex bytes), and `processorArchitecture`, omitting nullable fields.
The port instead emits unquoted capitalized `Version`/`Culture`/
`ProcessorArchitecture`, never includes the token, and always includes the
last two fields (`ApplicationId.hpp:101-105`).  This makes its string unable to
identify unequal ApplicationIds that differ only by token and incompatible with
the current manifest identity representation.

Tests merely search for fragments, so all 16 pass without checking exact text,
token distinction, optional omission, escaping, or byte-to-hex conversion.

### Status: BOTH STILL CONFIRMED — approval boundary recorded (#2290 review, #2291 `needs_user`, 2026-08-11)

**A genuine pair, and the dependency runs one way.** SR-AUD-125's faithful repair
*requires* SR-AUD-124's decision: ".NET writes `publicKeyToken` as uppercase hex
bytes" only has meaning if the token is a `byte[]` — while it is an
encoding-less `std::string`, the raw text, hex of the storage bytes and hex of a
decoded value are three different strings and nothing says which the caller
supplied — and ".NET omits nullable fields" only has meaning if Culture and
ProcessorArchitecture can *be* null, which this port cannot express. So
SR-AUD-125 is **not independently compatible** and was deliberately not split
out. The dependency is one-way: SR-AUD-124 could be decided without touching
`ToString`, which is why #2291 separates the options.

**Both findings reproduce exactly as filed; no premise correction was needed** —
worth recording because the two slices before this one in the same batch each
corrected something. Verified against the current header: five mandatory
by-const-reference `std::string`/`Version` parameters with no `optional`; no
validation of any kind in the constructor body; `publicKeyToken_` absent from the
`ToString` expression; unquoted capitalized keys with both optional components
always emitted.

**Consumer inventory measured, not inherited:** **zero** production consumers
(`ApplicationIdentity` is a different type that neither includes nor mentions
this one); two test files, 18 cases, in **two different executables**
(`ApplicationIdTests.cpp` 16 in Core.Base, `Task42Tests.cpp` 2 in the integration
binary) — so the shared suite name is not the one-binary duplication seen on
`Void` and `UnitySerializationHolder`. Zero consumers does not license a public
source break: downstream consumers exist and this batch may not inspect them.

**Routes priced in `docs/CoreApplicationIdIdentityModelDesign.md` §4**, none
selected — the selection *is* the decision. The token: a `byte[]`-shaped
parameter and getter (source break), an uppercase-hex contract on the existing
`std::string` (no signature change, but the two tokens these tests use are not
hex, so it is a behaviour break dressed as documentation), or opaque text
(no break, closes nothing). The optional components: `std::optional` (source
break on two getters and the constructor), added predicates (additive but the
constructor still cannot receive the distinction), or the documented single
representation (no break, closes nothing). The text: one decision, because a
partial move changes it without reaching parity.

**The separable one, recommended and deliberately not taken:** rejecting an empty
name needs no representation decision, the port already has
`ArgumentException::ThrowIfNullOrEmpty` so no message would be invented, and no
in-repo call site passes an empty name. It was still left to #2291 because it
turns an input that currently succeeds into a runtime throw for every consumer
while closing nothing — SR-AUD-124 is a conjunction whose representation halves
would keep it open.

**What was done anyway, and what it does not close:** the header now states, on
the class, on `ToString()` and on `GetHashCode()`, what is true today and under
every option — the token is text stored verbatim with **no encoding applied or
assumed** so binary key material cannot be round-tripped; there is **no
null/empty distinction**; the name is **not validated**; `ToString()` is a
counterpart **in role only**, with both grammars spelled out and the consequence
stated plainly (*two ApplicationIds differing only by token produce identical
text, so this string does not identify an ApplicationId and must not be used as a
manifest identity or an equality proxy*, while `Equals` does distinguish them);
and `GetHashCode` derives from name and version only, matching .NET.
**No signature, stored type, emitted text or validation changed, so neither
finding is closed.**

**No test was added**, for the reason #2281 records on `UnitySerializationHolder`:
every assertion the notes below ask for — exact text, token distinction, optional
omission, escaping, byte-to-hex, copy isolation of token storage — is an
assertion about the disputed model, and each would have to be retired by
whichever option is approved. They are listed in #2291 instead.

**A separate latent defect found while reviewing is ticket #2292 and carries no
`SR-AUD-*` identifier:** `GetHashCode()` is declared `noexcept` and calls
`Version::ToString()`, which builds a `std::string`, so an allocation failure
calls `std::terminate` instead of propagating. Verified by inspection; no crash
was staged. It is independent of the identity model, survives every option, and
would still be there if both findings were closed. Three routes recorded (drop
`noexcept`; hash `Version::GetHashCode()` instead of its text, which changes
emitted hash values; document only), none taken.

**Not a family with SR-AUD-127/136/137**, reviewed earlier in the same batch: the
"public shape drift with no production consumer" characteristic is a
characteristic, not a cause. These two are joined by something narrower and more
specific — one finding's repair needs the other's representation — which is a
dependency inside one type, not a recurring mechanism. **No CCF minted.**
Neither finding is marked design-complete: #2291 prices the routes but does not
select one.

## Other missing assertions and diagnostics

- Equality includes every stored field as .NET does, but hash tests do not
  verify equality/hash coupling across differing token/culture/architecture.
- `Copy` is a normal value copy; no test shows token representation/lifetime.

## Final assessment

Core scalar storage works, but validation, binary/nullable modeling, and text
identity compatibility are materially incomplete.  No source or test was
modified during this audit.
