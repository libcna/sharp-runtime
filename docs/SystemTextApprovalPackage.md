<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Text` approval package — the nine blocked decisions, verified

Ticket **#2022**, written 2026-08-03 on branch
`feature/remediation-batch-text-approvals-next-review`.

This document is the **single place a decision is requested** for the nine
approval-sensitive `System::Text` tickets **#2013 – #2021**. It supersedes
`docs/SystemTextNamespaceReviewPlan.md` §14 as the *request*; §14 stays as the
historical design record and is not rewritten. Where this document and §14
disagree, **this document is the measured one** and says so explicitly in
§10, "Corrections to §14".

**Nothing gated is implemented by writing this.** The one thing that changed in
the repository during the verification is **#2022**, a test-only ticket
(§9) that pins behaviour nothing was guarding.

**Evidence base.** Every "now" row below was re-measured on 2026-08-03 against
the shipped library, not copied from the review that designed it:
`build-probe/2022_probe1_approval_verify.cpp` → `build-probe/2022_probe1_verify.log`.
`/rv/tmp/runtime/src/libraries/` was re-verified **absent**, and no .NET runtime
is installed in this container, so each entry states separately whether its
target behaviour is **measured here**, **transcribed from a committed contract
document**, or **only inferred from knowledge of .NET** — and the last of those
is never sufficient on its own (§11).

---

## 1. How to read this, and what a "yes" costs

Nine tickets, but they are **not nine independent decisions**. They form six
families, and inside a family the parts interact:

| Family | Tickets | One-line question |
|---|---|---|
| **A — object identity and ownership** | #2013, #2021 | What object may a factory or a metadata record hand out, and may a caller mutate it? |
| **B — the unit of an index** | #2015 | Is a public index a UTF-8 byte, a UTF-16 code unit, or a scalar? |
| **C — conversion semantics** | #2014, #2016, #2017 | What bytes may an encoding produce, and what happens to input it cannot convert? |
| **D — the composite-format grammar** | #2020 | May `CompositeFormat::Parse` change which format strings it accepts? (**This is CCF-012's last member.**) |
| **E — the Web encoders' policy** | #2019 | What must the default HTML/JavaScript encoders escape? |
| **F — Unicode classification** | #2018 | Does `Rune` get real Unicode category and case tables? |

Approve **families**, not lines. A partial approval inside family C is
specifically dangerous and §5.4 says why.

Every approval in this document is a **behaviour change to a
currently-succeeding call**. None is a bug fix in the sense the compatible batch
(#2007 – #2012) used the word: those changed only inputs that were undefined,
faulting, or defined to an unusable value.

---

## 2. Family A — object identity and ownership (#2013, #2021)

### 2.1 #2013 — the seven factory encodings are one process-wide mutable object

**Finding:** SR-AUD-288 (high, `confirmed (design-complete)`), cause **T-G**,
structurally **CCF-009**'s shape (`Random::Shared`, `Guid::NewGuid`).

**Current behaviour, measured.** `Encoding::UTF8()` and its six siblings are
function-local `static std::shared_ptr` singletons
(`modules/text/src/System/Text/Encoding.cpp:15-55`). Every call returns the same
object; the object has two publicly settable members. Measured:

```
Encoding::UTF8().get() == Encoding::UTF8().get()                     // true
Encoding::UTF8()->setDecoderFallbackProperty(DecoderReplacementFallback("<X>"))
Encoding::UTF8()->GetString({0xFF})                                  // "<X>"  — for EVERY caller
```

and `audit/modules/text/src/System/Text/Encoding.cpp.audit.md` records a **TSan
read/write race** between the setter and a concurrent decode. (That TSan run is
the audit's; it was **not** re-run here — see §11, row 1.)

**Proposed behaviour (recommended option A′, see below).** The seven factory
instances become **read-only**: both fallback setters raise
`System::InvalidOperationException` on them, a new `getIsReadOnlyProperty()`
reports it, and a new `Clone()` returns a mutable copy. Directly constructed
instances (`UTF8Encoding u;`) are unaffected and stay mutable.

**Observable before / after:**

| Call | Before | After |
|---|---|---|
| `Encoding::UTF8()->setDecoderFallbackProperty(f)` | succeeds, changes every caller's decoding | **`InvalidOperationException`** |
| `UTF8Encoding u; u.setDecoderFallbackProperty(f);` | succeeds | succeeds — unchanged |
| `Encoding::UTF8() == Encoding::UTF8()` | same pointer | same pointer — unchanged |
| `Encoding::UTF8()->GetString(...)` concurrently with a setter | TSan race | no race: nothing can mutate it |
| `Encoding::UTF8()->getIsReadOnlyProperty()` | — | `true` (new member) |

**Exact public declarations affected** (`modules/text/include/System/Text/Encoding.hpp`):

```cpp
void setDecoderFallbackProperty(std::shared_ptr<DecoderFallback> value);   // gains a throw path
void setEncoderFallbackProperty(std::shared_ptr<EncoderFallback> value);   // gains a throw path
[[nodiscard]] bool getIsReadOnlyProperty() const;                          // NEW
[[nodiscard]] std::shared_ptr<Encoding> Clone() const;                     // NEW
```

**Source / ABI / layout / vtable / `noexcept` consequences — this is where §14.1
is incomplete.** §14.1 recommends "option A … adding an `IsReadOnly` property
and a `Clone()`" without stating what those cost. Measured
(`2022_probe1_verify.log` §G):

```
sizeof(Encoding) = 40   alignof = 8      (vptr 8 + two shared_ptr 16 each)
sizeof(UTF8Encoding) = 40   sizeof(UnicodeEncoding) = 48   sizeof(UTF32Encoding) = 48
```

There is **no free padding byte**. A `bool isReadOnly_` data member therefore
takes `sizeof(Encoding)` **40 → 48**, and every derived encoding with it — an
**object-layout change** requiring a full rebuild of every consumer, in the
class of #1788/#1789/#1889. A **virtual** `Clone()` additionally appends a
**vtable slot**.

**Option A′ — the same contract with neither cost, and the recommendation.**
Read-only-ness is a property of *identity*, not of state: exactly seven objects
are read-only, and they are the seven the library itself created. So

```cpp
bool Encoding::getIsReadOnlyProperty() const;   // defined in Encoding.cpp: compares `this`
                                                // against the seven factory instances
std::shared_ptr<Encoding> Encoding::Clone() const;  // non-virtual; dispatches on code page
```

needs **no data member, no virtual, no vtable slot and no layout change**. The
two setters move from inline to `Encoding.cpp` (so a consumer **relinks** as
well as recompiles, the ordinary consequence of de-inlining), and `Clone()`
reuses the same code-page dispatch #2021 needs anyway (§2.2), which is the
second reason to decide A and F together. `noexcept`: none present anywhere in
the type, none added; the two setters were already potentially-throwing after
#2008.

**Migration.** In-repository: **one** call site,
`modules/text/tests/System/Text/TextNamespaceTests.cpp`, which mutates
`Encoding::UTF8()`'s fallback and restores it; it migrates to `Clone()`. Plus
`TextUnitContractTests.TheFactoryEncodingsAreStillOneSharedMutableObject`, whose
whole purpose is to fail here. **Downstream (CNA, mobile-eggbert) was not
investigated — #1773 stays blocked**, and any external caller that configures a
factory encoding gets a **runtime** exception, not a compile error. That is the
sharpest edge in this family and the reason a `Clone()` must ship with it.

**Alternatives considered.** (A) .NET's read-only contract with a new data
member — correct but costs layout. (**A′**) the same contract by identity —
recommended. (B) a fresh instance per factory call — breaks
`UTF8() == UTF8()` identity, which existing code and tests rely on, and silently
doubles allocation in hot paths. (C) a mutex inside `Encoding` — fixes the race
but not the aliasing, and costs a data member anyway. (D) document and leave —
what #2012 already did; leaves a `high` finding open and a real race.

**Test plan.** Both setters throw on all seven factories with exact type and
message; every directly constructed encoding still accepts both setters;
`Clone()` returns a distinct, mutable object with equal code page;
`getIsReadOnlyProperty()` true for the seven and false for a clone and for a
stack instance; the migrated `TextNamespaceTests` case; a `sizeof`/`alignof`
pin proving A′ changed no layout.

**Sanitizer plan.** **TSan is the gate**, and it is the only ticket in the
namespace for which TSan is applicable: a two-thread probe (setter vs decode)
must report the race **before** and be silent **after**. ASan/UBSan/LSan over
the same probe.

**Rollback.** Delete the two new members and restore the two setter bodies; no
persisted state, no file format, no serialized data is involved. Under A′ the
rollback is a pure revert with no layout consequence in either direction.

> **Approval A1.** Approve making `System::Text::Encoding`'s seven factory
> instances read-only — so `setDecoderFallbackProperty` and
> `setEncoderFallbackProperty` raise `System::InvalidOperationException` when
> called on an object returned by `UTF8()`, `ASCII()`, `Unicode()`,
> `BigEndianUnicode()`, `UTF32()`, `UTF7()`, `Latin1()` or `Default()` — and
> adding a non-virtual `getIsReadOnlyProperty()` and a non-virtual `Clone()`
> implemented by identity comparison and code-page dispatch, so that **no data
> member, vtable slot or object layout changes**; accepting that this
> repository's own `TextNamespaceTests` migrates to `Clone()` and that any
> downstream caller which configures a factory encoding begins to receive a
> runtime exception. Ticket **#2013**.

### 2.2 #2021 — `EncodingInfo::GetEncoding` ignores its own code page

**Finding:** SR-AUD-299 (medium, `confirmed (design-complete)`), cause **T-F**.

**Current behaviour, measured.** `GetEncoding()` is a two-line inline that
returns `Encoding::UTF8()` whatever the code page says
(`EncodingInfo.hpp:41-43`):

```
EncodingInfo(20127,"us-ascii","US-ASCII").GetEncoding()->getCodePageProperty()  // 65001
                                          .GetEncoding()->GetBytes(u8"é")       // c3 a9  (UTF-8)
identity is Encoding::UTF8():                                                   // yes
```

**The measured fact §14.9 does not state:** the object handed out **is the
shared mutable singleton family A owns**. So a caller that receives an
`EncodingInfo`'s encoding and configures it silently reconfigures
`Encoding::UTF8()` for the whole process. #2021 and #2013 are therefore one
decision about one object, which is why they are grouped.

**Proposed behaviour.** Resolve the code page to the matching implemented
encoding — 65001 → UTF-8, 20127 → ASCII, 1200/1201 → UTF-16 LE/BE, 12000/12001
→ UTF-32 LE/BE, 28591 → Latin-1, 65000 → UTF-7 — and raise
`System::ArgumentException` (or `NotSupportedException`; see §11, row 5) for a
code page this port does not implement.

**Observable before / after:**

| Call | Before | After |
|---|---|---|
| `EncodingInfo(20127,…).GetEncoding()->getCodePageProperty()` | `65001` | `20127` |
| `EncodingInfo(20127,…).GetEncoding()->GetBytes(u8"é")` | `c3 a9` | `3f` (`'?'`) |
| `EncodingInfo(28591,…).GetEncoding()` | UTF-8 | Latin-1 (whose own semantics are #2014) |
| `EncodingInfo(437,…).GetEncoding()` | UTF-8 | **throws** |
| identity with `Encoding::UTF8()` | always equal | equal only for 65001 |

**Consequences.** Signature unchanged; `GetEncoding` stays non-virtual and
inline-or-`.cpp`. **No layout, vtable or `noexcept` change.** One include-graph
consequence: resolving code pages inside the header would make
`EncodingInfo.hpp` pull in all seven encoding headers, so the body should move
to a new `EncodingInfo.cpp` in the same component — **no new module edge**
(`Text` already owns every encoding), the graph stays **41 modules / 91 edges**.

**Migration.** **No first-party caller exists** — re-verified 2026-08-03: no
code in `modules/` or `tests/` constructs an `EncodingInfo`. The risk is
external only, and it is a behaviour change plus a new throw.

**Alternatives.** (A) resolve and throw for the unknown — recommended.
(B) resolve and fall back to UTF-8 for the unknown — keeps every current call
succeeding, but preserves exactly the "successful wrong conversion" the finding
is about. (C) delete `GetEncoding` — a public source break, larger than the
defect.

**Test plan.** One case per implemented code page asserting the resolved
`getCodePageProperty()` and one round-trip byte comparison; the unknown-code-page
throw with exact type and message; the `#2022` pin
(`EncodingInfoStillIgnoresItsOwnCodePage`) inverted. **Sanitizers:** ASan/UBSan
only; nothing shared, no arithmetic — unless A1 is declined, in which case the
returned object stays mutable and shared and TSan applies here too.

**Rollback.** Restore the two-line body.

> **Approval A2.** Approve making `System::Text::EncodingInfo::GetEncoding`
> return the encoding matching its own code page — 65001, 20127, 1200, 1201,
> 12000, 12001, 28591 and 65000 — and raise a `System` exception for any other
> code page, accepting that it no longer returns UTF-8 unconditionally and that
> an unrecognised code page begins to throw; and noting that until Approval A1
> also lands, the object it returns remains the shared, publicly mutable factory
> instance. Ticket **#2021**.

---

## 3. Family B — the unit of a public index (#2015)

**Finding:** SR-AUD-290 + SR-AUD-296 (both medium, both
`confirmed (design-complete)`), cause **T-I**. The disclosure half is
**remediated** by #2012; this is the semantic half.

**Current behaviour, measured.** Every public index, length and count in the
namespace is a **UTF-8 storage byte**:

```
Encoding::UTF8()->GetCharCount(U+1F600 as 4 bytes)   // 4   (.NET: 2 UTF-16 code units)
StringBuilder(u8"éA").getLengthProperty()            // 3   (.NET: 2)
StringBuilder(u8"éA").Remove(1,1).ToString()         // c3 41 — ill-formed UTF-8
```

**Proposed behaviour.** Option (A) UTF-16 code units, matching .NET; option (B)
scalar counts; option (C) keep bytes and declare them — which #2012 has already
done for the doc-comments.

**The measured fact that decides this, and that §14.3 only gestures at.**
§14.3 says the decision "should not be taken without deciding it for
`System::String` too". Measured: **`System::String` is not a type with a
`Length` property at all.** It is a static helper class over `std::string`
(`modules/core/include/System/String.hpp`), and *every* index it takes or
returns — `IndexOf`, `LastIndexOf`, `Substring`, `IndexOfAny` — is a
`std::string` byte offset. The runtime's string *is* `std::string`; there is no
adapter layer in which a unit could be reinterpreted.

The consequence is decisive rather than merely large. Adopting (A) or (B) inside
`System::Text` alone would make `Encoding::GetCharCount` and
`StringBuilder::Length` **disagree with `std::string::size()`, with every
`String::` static, and with `StringBuilder::ToString().size()`** — i.e. it would
replace one honest divergence from .NET with an internal contradiction inside
one program. Adopting them *everywhere* is not a remediation ticket: it is a
change of the runtime's string representation, and it would touch every module.

**Consequences of the recommended option (C).** None: zero code change. The
work is already done — the contracts are true (#2012) and the current units are
pinned (`TextUnitContractTests`, and #2022 for the rest).

**Alternatives.** (A) UTF-16 units — the .NET answer; needs a UTF-16 string
representation first; **not recommended as a `System::Text` ticket**. (B) scalar
counts — internally consistent within `System::Text` and consistent with
neither .NET nor `std::string`; the worst of the three. (C) declare and keep —
recommended, and **should be recorded as a permanent deviation in `CLAUDE.md`'s
"Known permanent deviations" list**, alongside reflection and GC, rather than
left as an open finding forever.

**Test / sanitizer plan.** Option C requires no new test beyond the existing
pins. If (A) is ever chosen it needs its own namespace review, not this ticket.

**Rollback.** Not applicable to (C).

> **Approval B.** Approve **closing #2015 as a permanent, documented deviation**
> rather than implementing it: every public index, length and count in
> `System::Text` (and in `System::String`, `System::Text::StringBuilder` and the
> rune enumerators) remains a **UTF-8 storage-byte** position, because this
> runtime represents a string as a UTF-8 `std::string` and changing the unit in
> one namespace would make it disagree with `std::string::size()` and with every
> `System::String` static — accepting that `Encoding::GetCharCount` and
> `StringBuilder::Length` permanently report different numbers than .NET, as
> their doc-comments now state, and that the deviation is recorded in
> `CLAUDE.md`. Ticket **#2015**.
>
> *(If instead you want .NET's unit, say so and #2015 stays blocked pending a
> whole-runtime UTF-16 decision — do not approve a `System::Text`-only change.)*

---

## 4. Family C — conversion semantics (#2014, #2016, #2017)

These three all change **bytes a currently-succeeding call produces**. They also
interact: #2017 decides what happens to input #2014 cannot encode, and #2016
changes the byte count #2017's tests measure. §5.4 states why a partial approval
here is worse than none.

### 4.1 #2014 — `Latin1Encoding` converts storage bytes, not code points

**Finding:** SR-AUD-289 (medium), cause **T-H**.

**Now, measured.** `Latin1.GetBytes(u8"é")` → `c3 a9` (ISO-8859-1 is the single
byte `e9`); `Latin1.GetString({0xE9})` → the single byte `e9`, which is not
well-formed UTF-8 (correct answer: `c3 a9`). It is the **only** encoding in the
component that does not decode the UTF-8 storage into scalars first — ASCII,
UTF-16 and UTF-32 all do.

**Proposed.** Decode the UTF-8 storage into scalars; emit one byte per scalar
U+0000–U+00FF and the configured **encoder fallback** otherwise; decode each
byte to the scalar of the same value and re-encode as UTF-8. This is exactly
`ASCIIEncoding`'s existing algorithm one range wider.

| Call | Before | After |
|---|---|---|
| `GetBytes(u8"é")` | `c3 a9` | `e9` |
| `GetBytes(u8"€")` (U+20AC, not in Latin-1) | `e2 82 ac` | `3f` — or the fallback, **which is #2017** |
| `GetString({0xE9})` | `e9` (ill-formed) | `c3 a9` |
| `GetBytes("Hi")` / ASCII generally | `48 69` | **identical** |

**Consequences.** Signature unchanged; the class is fully header-inline, so a
consumer **recompiles**. No layout, vtable or `noexcept` change. `Latin1Encoding`
has no data members. **Accepted input is not narrowed** — every input is still
accepted; the *output* changes.

**Migration.** No first-party caller constructs a `Latin1Encoding` or calls
`Encoding::Latin1()`. External callers that persisted Latin-1 bytes produced by
this port hold **UTF-8 bytes mislabelled as ISO-8859-1**; after the change the
same source text produces genuine ISO-8859-1. Round-tripping old data through
the new code is *not* value-preserving, and that is the migration hazard —
it deserves a `docs/Migration-Latin1Scalars.md` note, on the
`docs/Migration-DecimalCommaGroupSeparator.md` precedent.

**Alternatives.** (A) convert scalars — recommended, and the only reading under
which the name, the IANA name `iso-8859-1` and the code page 28591 are true.
(B) rename/redocument as a byte pass-through — contradicts all three, and
`getIsSingleByteProperty()` already returns `true`.

**Test / sanitizer plan.** A generated round-trip over U+0000–U+00FF; the
unencodable-scalar path once per fallback kind; ASCII byte-identity before and
after; the `TextUnitContractTests.Latin1MapsStorageBytesNotCodePoints` pin
inverted. ASan/UBSan over a generated corpus; TSan not applicable.

**Rollback.** Restore the two loops; the class is self-contained.

> **Approval C1.** Approve making `System::Text::Latin1Encoding` convert Unicode
> scalar values rather than UTF-8 storage bytes — `GetBytes` emitting one
> ISO-8859-1 byte per scalar U+0000–U+00FF and the configured encoder fallback
> for every other scalar, `GetString` decoding each byte to the scalar of the
> same value — accepting that every non-ASCII conversion produces different
> bytes than today and that data previously produced by this class cannot be
> round-tripped through the new code. Ticket **#2014**.

### 4.2 #2016 — the byte-order mark is serialized as payload

**Finding:** SR-AUD-291 (medium), cause **T-J**.

**Now, measured — and §14.4 names only half of it.** §14.4 says
"`Encoding::UTF32()` produces". Measured across **all seven** factories
(`2022_probe1_verify.log` §B):

```
UTF8()             GetBytes("A") = 41
ASCII()            GetBytes("A") = 41
Unicode()          GetBytes("A") = 4100
BigEndianUnicode() GetBytes("A") = feff0041          <-- BOM AS PAYLOAD
UTF32()            GetBytes("A") = fffe000041000000  <-- BOM AS PAYLOAD
UTF7()             GetBytes("A") = 41
Latin1()           GetBytes("A") = 41
```

**Two** default factories emit it, not one: `Encoding::BigEndianUnicode()` is
constructed `UnicodeEncoding(true, true)` (`Encoding.cpp:35`) and
`Encoding::UTF32()` is `UTF32Encoding()`, whose default constructor sets
`byteOrderMark_(true)` (`UTF32Encoding.hpp:29`).

**And a second half neither the finding nor §14.4 mentions:** the **decode**
direction *consumes* a leading U+FEFF (`UnicodeEncoding.hpp:115-118`,
`UTF32Encoding.hpp:99-104`). Measured:

```
UTF16LE.GetString(FF FE 41 00)             = "A"   — the U+FEFF is DROPPED
UTF32LE.GetString(FF FE 00 00 41 00 00 00) = "A"   — DROPPED
UTF32().GetString(UTF32().GetBytes("A"))   = "A"   — a round trip cancels both halves
```

.NET's `GetString` does not strip a BOM: U+FEFF decodes to ZERO WIDTH NO-BREAK
SPACE. The round-trip cancellation is why no existing test ever saw either half.
Both are now pinned (#2022) and folded into #2016 — **no `SR-AUD-*` identifier
issued; numbering stays frozen at 364.**

**Proposed.** `GetBytes` never prepends; a new `GetPreamble()` exposes the bytes
for callers who want them; `GetString` stops consuming a leading U+FEFF.

**Consequences — two gates, exactly as §14.4 says, plus a third half.**
(a) emitted bytes change for `Encoding::BigEndianUnicode()` **and**
`Encoding::UTF32()`; (b) decoded text changes for any input that starts with
U+FEFF; (c) a **virtual** `GetPreamble()` on `Encoding` **appends a vtable
slot** — a full recompilation for every consumer, and an ABI break for anything
that was linked against the old vtable. A non-virtual per-class `GetPreamble()`
avoids (c) but cannot be reached through an `Encoding&`, which is how every
consumer holds one; the same identity-dispatch trick that makes A′ cheap does
**not** work here, because the preamble depends on constructor arguments, not on
identity.

**Migration.** In-repository callers of `Encoding::UTF32()`/`BigEndianUnicode()`:
none outside tests. Externally, any consumer writing a UTF-32 or UTF-16BE file
and relying on the port to emit the BOM must call `GetPreamble()` explicitly —
a **source** change, the only one in this whole package.

**Alternatives.** (A) full .NET behaviour with a virtual `GetPreamble` —
recommended if the vtable cost is acceptable. (B) stop prepending, expose the
preamble non-virtually per class — no vtable change, but unreachable
polymorphically. (C) keep prepending and rename the property to say so —
leaves two default factories producing text that no conforming reader treats as
the caller intended.

**Test / sanitizer plan.** `GetBytes` byte-identity for all seven factories
before/after; `GetPreamble` per encoding and endianness; a decode case with a
real leading U+FEFF asserting it survives; the two #2022 pins inverted; a
`sizeof`/vtable-slot-count pin. ASan/UBSan; TSan not applicable.

**Rollback.** Revert three inline bodies and remove the new member; if the
virtual shipped, removing it is a second vtable change — so **prefer option B
if there is any chance of reversal.**

> **Approval C2.** Approve (a) removing the byte-order mark from
> `UnicodeEncoding::GetBytes` and `UTF32Encoding::GetBytes`, changing the bytes
> **both** `Encoding::BigEndianUnicode()` and `Encoding::UTF32()` produce for
> every input; (b) making `GetString` stop consuming a leading U+FEFF, so that a
> real ZERO WIDTH NO-BREAK SPACE is decoded instead of discarded; and (c) adding
> a **virtual** `GetPreamble()` to `System::Text::Encoding`, accepting the
> resulting vtable layout change and the full recompilation it forces on every
> consumer. Ticket **#2016**. *(Answer (c) separately if you prefer the
> non-virtual per-class spelling, which avoids the vtable change but cannot be
> reached through an `Encoding&`.)*

### 4.3 #2017 — the configured fallbacks are inert outside `UTF8Encoding`

**Finding:** SR-AUD-292 + SR-AUD-293 (both medium), cause **T-K**.

**Now, measured.** Only `UTF8Encoding` consults the configured fallback objects
(`UTF8Encoding.cpp:56-71, 89-104`). Measured:

```
UTF-16 + DecoderFallback::ExceptionFallback(), lone surrogate  -> does NOT throw
ASCII  + EncoderReplacementFallback("!"),      GetBytes(u8"é") -> 3f    ('?' hard-coded)
UTF16LE.GetString(3 bytes)                                     -> "A"   (trailing byte dropped)
UTF32LE.GetString(6 bytes)                                     -> "A"   (two bytes dropped)
UTF8 + EncoderReplacementFallback("!"), GetBytes("\xFF")       -> 21    ('!' — the one that works)
```

**Proposed.** Every encoding routes each unencodable scalar and each ill-formed
byte sequence — including an incomplete trailing UTF-16 or UTF-32 unit —
through its configured `EncoderFallback`/`DecoderFallback`, and each concrete
encoding's constructor installs the replacement its loop hard-codes today
(`U+FFFD` for UTF-16/UTF-32, `'?'` for ASCII and Latin-1), so **default** output
does not change.

**The consequence §14.5 does not state, and it is a second gate.** The fallback
surface is **byte-shaped**, not scalar-shaped:

```cpp
virtual bool Fallback(char charUnknown, SharpRuntime::intcs index) = 0;   // EncoderFallbackBuffer
virtual std::vector<bytecs> GetFallbackBytes(char unknownChar) const = 0; // EncoderFallback
char EncoderFallbackException::getCharUnknownProperty() const;
```

A `char` **cannot carry U+00E9, let alone U+20AC**. `EncoderFallback.hpp`'s own
doc-comment records this as the deliberate reduction of .NET's
`CharUnknownHigh`/`CharUnknownLow`. So "route the unencodable **scalar** through
the fallback" is not implementable against the current surface: it requires
either widening these to a scalar type — a **public virtual signature change**,
i.e. a vtable change *and* a source break for any external subclass — or
reporting only the first byte, which makes the reported character wrong for
every non-ASCII input. **This is an approval question in its own right and
§14.5 does not ask it.**

**Consequences.** Emitted bytes unchanged *only if* every constructor installs
its current hard-coded replacement (a requirement, not a nicety); a configured
**exception** fallback starts throwing from calls that never threw; if the
signatures widen, vtable + source break; no data-member or layout change
otherwise.

**Migration.** In-repository: `TextUnitContractTests` and the new #2022 pin both
fail deliberately. Externally, any caller that installed an exception fallback
"for safety" on a non-UTF-8 encoding and never saw it fire will start seeing
exceptions.

**Alternatives.** (A) route everything, keep the `char` surface, report the
first byte — cheapest, and wrong in the reported character.
(B) route everything and widen the fallback surface to a scalar type —
correct, costs a public virtual signature change. (C) route ill-formed
*sequences* only (the decoder direction), leaving the encoder direction as is —
closes SR-AUD-293 and half of SR-AUD-292, needs no signature change.
(D) document the reduction and leave. **Recommended: B if a source break is
acceptable, otherwise C as a bounded first step**, with the encoder half staying
blocked.

**Test / sanitizer plan.** For each of the seven encodings × {replacement,
exception, custom} fallback × {ill-formed input, unencodable scalar, truncated
trailing unit}: exact output or exact exception; a default-output byte-identity
block proving the constructors installed the right replacements; the two pins
inverted. LSan matters here — a fallback that throws mid-conversion must not
leak the partial buffer.

**Rollback.** Per-encoding; each loop is independent.

> **Approval C3.** Approve making every `System::Text` encoding route
> unencodable characters and ill-formed byte sequences — including an incomplete
> trailing UTF-16 or UTF-32 unit — through its configured
> `EncoderFallback`/`DecoderFallback`, so a configured exception fallback
> throws, and requiring each concrete encoding's constructor to install the
> replacement text its loop hard-codes today so that default output is
> unchanged. **Answer separately:** may the fallback surface
> (`EncoderFallbackBuffer::Fallback`, `EncoderFallback::GetFallbackBytes`,
> `EncoderFallbackException::getCharUnknownProperty`) be widened from `char` to
> a Unicode scalar type — a public virtual signature change, hence a vtable
> change and a source break for any external subclass — or must the reported
> character stay a single byte, which is wrong for every non-ASCII input?
> Ticket **#2017**.

### 4.4 Why family C should be approved together or not at all

`Latin1Encoding` after C1 needs an encoder fallback for U+0100 and above — which
is inert until C3. If C1 lands alone, unencodable scalars fall back to the
hard-coded `'?'` that C3 is meant to remove, and the C1 tests then encode that
`'?'` as the expected answer; C3 later has to change them. If C2 lands alone,
every byte-count expectation written for C1 and C3 shifts by the preamble
length. The three are one coherent change to "what bytes come out".

---

## 5. Family D — one composite-format grammar (#2020), and CCF-012's closure

**Finding:** SR-AUD-298 grammar half (medium), cause **T-N**, **the last open
member of CCF-012**.

### 5.1 What the population actually is

Measured by exhaustive search of `modules/` (2026-08-03), **there are exactly
two composite-format grammar implementations in the repository**:

| Implementation | Reached by | State |
|---|---|---|
| `System::detail::runCompositeFormat` (`modules/core/include/System/detail/CompositeFormat.hpp`) | `String::Format` ×22, `FormattableString::ToString`, `StringBuilder::AppendFormat` ×11, `Console::Write`/`WriteLine` ×11 — all of which delegate | **fixed** by #1882/#1883/#1884 |
| `System::Text::CompositeFormat::countPlaceholders` (`modules/text/include/System/Text/CompositeFormat.hpp:48`) | `CompositeFormat::Parse` | **#2020, blocked** |

Everything else that scans a brace is a different grammar and **not** a CCF-012
member: `Regex`'s `${name}` substitution (`Match.hpp:156`), `XName`'s
`{ns}local` (`XName.hpp:87`), `Guid`'s `{…}` format specifiers
(`Guid.cpp:280`), and JSON/XML writers.

**A numbering correction.** The plan calls `CompositeFormat::Parse` "a fourth
hand-written composite-format grammar" in §5 and "a third" in §4.6. Counting
implementations that exist **today**, it is the **second**, and the only
divergent one; historically it was the third of three hand-written engines,
two of which #1882/#1883/#1884 merged into one. Both plan statements are left in
place; this is the appended correction.

### 5.2 What the two grammars actually do — measured, and §14.8 is wrong about it

`2022_probe1_verify.log` §A, port `Parse` versus the shared #1884 grammar over
27 inputs. The rows that matter:

| Format | `CompositeFormat::Parse` today | Shared #1884 grammar | Direction |
|---|---|---|---|
| `{0,not-a-width}` | ok, minArgCount 1 | **FormatException** | narrows |
| `{0,-}` | ok, minArgCount 1 | **FormatException** | narrows |
| `{0 }` | **FormatException** | **ok**, index 0 | **WIDENS** |
| `{0  ,5}` | **FormatException** | **ok**, index 0 | **WIDENS** |
| `{999999}` | ok, 1000000 | ok, index 999999 | same |
| `{1000000}` | ok, 1000001 | **ok, index 1000000** | **same — §14.8 says this throws** |
| `{9999999}` | ok, 10000000 | **ok, index 9999999** | **same — §14.8 says this throws** |
| `{10000000}` | ok, 10000001 | **FormatException** | narrows |
| `{2147483646}` | ok, 2147483647 | **FormatException** | narrows |
| `{2147483647}` | FormatException (#2010) | FormatException | same |
| `{000000000001}` | ok, 2 | ok, index 1 | same |
| `{0,1000000}` | ok, 1 | ok — **and it allocates 1,000,000 spaces** | see §5.3 |
| `Hello {`, `a } b`, `{ 0 }`, `{-1}`, `{0:{1}}` | FormatException | FormatException | same |

**Three corrections to §14.8, all measured:**

1. **"any index at or above 1,000,000 begin to throw" is false.**
   `kCompositeIndexLimit` is not a rejection bound: `runCompositeFormat`
   *stops consuming digits* once the accumulated index reaches 1,000,000
   (`detail/CompositeFormat.hpp:136`) and only fails if a digit follows. So
   `{1000000}` … `{9999999}` are **accepted** there, and the first rejected
   values are eight-digit ones such as `{10000000}` — and `{2147483646}`, which
   this port accepts today with a defined positive answer.
2. **Adoption is not purely a narrowing.** `{0 }` and `{0  ,5}` are
   `FormatException` here today and **accepted** by the shared grammar, which
   skips spaces after the index (`detail/CompositeFormat.hpp:140`). §14.8's
   sentence describes narrowing only.
3. **Leading zeros are unbounded in both**, so `{000000000001}` parses as index
   1 either way — worth stating because it means the "index limit" bounds the
   *value*, not the *length*.

### 5.3 The structural obstacle §14.8 does not mention

`runCompositeFormat` is a **formatting** engine, not a validator. Its signature
is `runCompositeFormat(format, argCount, render)`; it raises
`Format_IndexOutOfRange` when `index >= argCount` (line 174), and it **executes
alignment padding** while it runs. `CompositeFormat::Parse` has **no argument
list** — its whole job is to report `minimumArgumentCount` *before* any
arguments exist. Consequences:

- a literal reuse must pass an artificial `argCount` and a `render` that records
  the maximum index. Measured: with an unbounded count the engine's own
  `out.reserve(format.size() + 16*argCount)` raises `std::bad_alloc` before
  parsing anything (this is **not** reachable from any public API, where
  `argCount` is the real argument count ≤ 22 — recorded so it is not mistaken
  for a live defect);
- with a workable count it still pads: `Parse("{0,1000000}")` would allocate a
  megabyte of spaces **to validate a string**, measured at `pad=1000000`. A
  `Parse` that allocates proportionally to a user-supplied alignment is a
  denial-of-service shape in a function whose entire purpose is to be cheap.

**So #2020 cannot be "call `runCompositeFormat`".** It must extract the scanner
from the engine — a `scanCompositeFormat(format, onItem)` in
`System::detail` that both the formatter and `Parse` use — so that the grammar
is stated once and neither renders nor pads while validating. That is a
**refactor of a `modules/core` header that `String::Format` depends on**, which
raises the blast radius of #2020 well above "a `modules/text` ticket" and is the
single most important thing this verification found about it.

### 5.4 Consequences, migration, alternatives

**Consequences.** `CompositeFormat::Parse`'s signature, layout and `noexcept`
are unchanged; `CompositeFormat` keeps `format_` + `minArgCount_` in order. The
extracted scanner changes **no** observable behaviour of `String::Format` if the
extraction is behaviour-preserving — which must be proved by the 3,675-case
`String::Format` corpus #1884 left behind, re-run unchanged.

**Migration.** No first-party caller of `CompositeFormat::Parse` exists outside
its own tests. Externally, a caller that parses `{0,not-a-width}` or
`{2147483646}` starts receiving `FormatException`, and one that parses `{0 }`
stops receiving it.

**Alternatives.** (A) extract the scanner and share it — recommended; the only
option that actually closes CCF-012. (B) hand-align `countPlaceholders` with the
shared grammar without sharing code — leaves two engines that agree today and
may drift, i.e. exactly what CCF-012 warns against. (C) leave `Parse` as the
minimal index scanner and **declare** the grammar difference — cheap, honest,
and leaves CCF-012 open forever.

**Test plan.** The 27-row table above as a permanent parametrised regression,
each row asserting the *new* expected side; the #2022 widening pin inverted; the
`String::Format` corpus unchanged, proving the extraction was behaviour
preserving; an alignment-heavy `Parse` case asserting **no** allocation
proportional to the width.

**Sanitizer plan.** UBSan on the extracted scanner's arithmetic (the index and
width accumulators); ASan/LSan over a generated malformed-format corpus; TSan
not applicable (no shared state).

**Rollback.** Two commits: the extraction (pure refactor, safe to keep) and the
adoption (revertible on its own).

### 5.5 Does closing #2020 close CCF-012?

**Yes — and only if it is done as option A.** Verified:

- CCF-012's own member finding **SR-AUD-015 is already `remediated`** (#1882,
  #1883, #1884 all `done`);
- CCF-012's exclusion list still reads *"and `System.Text.CompositeFormat`,
  which is not ported"*, which is false; the correction is already recorded in
  `audit/AUDIT_CROSS_CUTTING_FINDINGS.md`'s System::Text appendix and is **not**
  re-litigated here;
- the population is exactly the two implementations of §5.1, and no third exists;
- CCF-012's own closing condition is *"a repair needs a shared parsed-token model
  or a deliberately narrow documented formatter; altering just one API preserves
  divergent brace rules"*. Option B **preserves divergent rules** in code and so
  does not satisfy it; option C satisfies its second clause only if the
  divergence is documented on both sides.

**CCF-012 is therefore closable by #2020 under option A, and by option C only as
a documented deviation. It is not closable by planning**, and this document does
not mark it closed.

> **Approval D.** Approve closing CCF-012 by extracting the composite-format
> **scanner** from `System::detail::runCompositeFormat` into a shared,
> non-rendering `System::detail::scanCompositeFormat` used by both the formatter
> and `System::Text::CompositeFormat::Parse`, so that one grammar is stated
> once — accepting that `Parse` begins to **reject** `{0,not-a-width}`, `{0,-}`,
> `{10000000}` and every index above 9,999,999 (including `{2147483646}`, which
> succeeds today), and begins to **accept** `{0 }` and `{0  ,5}`, which it
> rejects today; and accepting a behaviour-preserving refactor of a
> `modules/core` header on which all 45 `String::Format`/`AppendFormat`/
> `Console::Write` entries depend. Ticket **#2020**.

---

## 6. Family E — the default Web encoders' policy (#2019)

**Finding:** SR-AUD-297 policy half (medium), cause **T-M**. The diagnostics
half is **remediated** by #2011.

**Now, measured.** `HtmlEncoder::Encode` escapes five ASCII characters
(`& < > " '`) and passes every other byte through, including all non-ASCII;
`JavaScriptEncoder::Encode` escapes `\ " \n \r \t` and C0 controls and passes
everything else, including all non-ASCII. Both `Encode(u8"é")` return the input
unchanged.

**Proposed.** .NET's default encoders allow **Basic Latin** only and escape
everything else, plus a `Create(UnicodeRange…)` opt-in for the current
pass-through.

**Why this one is different from the other eight: the target is not verifiable
here.** §7 of the plan already marks T-M "no evidence available", and that is
still true. Beyond the range policy, .NET's default `JavaScriptEncoder` also
escapes several **ASCII** characters this port passes through (`<`, `>`, `&`,
`'`, `+`) because the default is HTML-safe, and `HtmlEncoder` escapes `+`. I
know that from prior knowledge of .NET, **not** from anything in this
repository, and this package does not treat prior knowledge as evidence. So the
exact allow-list, the exact escape spelling (`&#xNNNN;` vs `&#NNNN;`), and the
supplementary-scalar spelling (one `&#x1F600;` or two surrogate escapes) are all
**unverifiable here**.

**Consequences if approved.** Emitted text changes for all non-ASCII input and
possibly for five ASCII characters; `UnicodeRanges` already exists
(`Unicode/UnicodeRanges.hpp`, 348 lines) so the opt-in has its vocabulary; new
static `Create` factories are additive; the classes have no data members, so no
layout change; the singletons `Default()` are immutable, so no thread-safety
change.

**Migration.** In-repository: `EncodingWebTests` and the #2011 pin
(`TheGatedDefaultPolicyIsDeliberatelyUnchanged`). Externally, every rendered
page or script produced through these helpers changes bytes — visibly, in
output that is often persisted.

**Alternatives.** (A) full .NET default policy — needs evidence this container
does not have. (B) escape non-ASCII only, leaving the ASCII set alone — a
defensible, verifiable-here subset (RFC-level HTML escaping), smaller and
honest. (C) document the reduced policy and leave — what #2011 pinned.

**Recommendation: do not approve (A) in this container.** Either take (B) as a
bounded, self-evidenced step, or leave #2019 blocked until the reference tree is
available. This is the one ticket in the package where "approve" and
"implementable" are not the same thing.

**Test / sanitizer plan.** A generated corpus over U+0000–U+FFFF plus
supplementary samples, asserting the chosen policy exactly; the #2011 pin
inverted; round-trip through a decoder where one exists. ASan/UBSan/LSan;
TSan not applicable.

> **Approval E.** *(Recommended answer: **defer**.)* Approve giving
> `System::Text::Encodings::Web`'s default HTML and JavaScript encoders a
> Basic-Latin allow-list that escapes every non-Basic-Latin scalar, and adding a
> `Create(UnicodeRange…)` opt-in for the current pass-through — **and state
> which allow-list**, because .NET's exact default set (including the ASCII
> characters its JavaScript encoder escapes for HTML safety and the exact escape
> spelling) **cannot be verified in this container** and must not be implemented
> from memory. Ticket **#2019**.

---

## 7. Family F — Unicode classification and casing (#2018)

**Finding:** SR-AUD-294 (medium), cause **T-L**.

**Now, measured** (`2022_probe1_verify.log` §D):

| Scalar | IsLetter | IsDigit | IsUpper | IsLower | IsWhiteSpace | ToUpper | ToLower |
|---|---|---|---|---|---|---|---|
| U+0041 `A` | yes | no | yes | no | no | U+0041 | U+0061 |
| U+00E9 `é` | **no** | no | no | **no** | no | **U+00E9** | U+00E9 |
| U+00C9 `É` | **no** | no | **no** | no | no | U+00C9 | **U+00C9** |
| U+0660 `٠` | no | **no** | no | no | no | — | — |
| U+03B1 `α` | **no** | no | no | no | no | **U+03B1** | U+03B1 |
| U+00A0 nbsp | no | no | no | no | **yes** | — | — |
| U+FEFF | no | no | no | no | **yes** | — | — |

**A refinement §14.6 does not have.** §14.6 frames this as "six ASCII-only
members versus one Unicode-aware `IsWhiteSpace`". Measured, the Unicode-aware
one is **itself divergent**: this port's white-space table
(`Rune.hpp:121-123`) contains **U+FEFF**, which .NET removed from its
white-space set — `Char.IsWhiteSpace('﻿')` is false there. So the repair is
not "make six members match the seventh"; it is "give all seven real tables",
and the seventh changes too. Recorded here and pinned by #2022; **no `SR-AUD-*`
identifier issued**.

**Proposed.** Real Unicode category and simple-case-mapping tables, with a
stated Unicode version.

**Consequences.** All members are `static` free-standing functions on a
one-`uint32_t` value type: **no signature, layout, vtable or `noexcept` change**
(`sizeof(Rune) == 4` and stays so). The cost is a **generated data table** —
new source, a generator, a licence/attribution question for the UCD, and a
data-versioning question (which Unicode version, and what happens when it moves).
Results change for every non-ASCII scalar.

**Migration.** In-repository: the ASCII-only Rune tests are unaffected (that is
the problem #2022 fixed); the new #2022 pins fail deliberately. Externally, any
caller that used `Rune::IsLetter` as a cheap ASCII test gets a different answer.

**Alternatives.** (A) full tables — the .NET answer; large and needs a data
source this container does not have. (B) extend to Latin-1/BMP only — a
half-measure that is wrong in a different place. (C) **declare the ASCII scope in
the class doc-comment** (seven of eight member comments already say it) and fix
only the U+FEFF divergence — smallest, verifiable here, and leaves the finding
open as a documented deviation.

**Recommendation: (C) now, (A) only if a Unicode data source and a version
policy are agreed.** The tables cannot be authored from memory, and a
hand-written approximation is worse than a declared reduction.

**Test / sanitizer plan.** Whichever option: a per-category sample set with the
Unicode version named in the test file; the #2022 pins inverted; a table-size /
binary-size note. ASan/UBSan over a full U+0000–U+10FFFF sweep; TSan not
applicable (the tables are `constexpr`).

> **Approval F.** Approve implementing real Unicode category and simple case
> mapping in `System::Text::Rune` — including the generated table, its data
> source, its attribution, and a **stated Unicode version with a policy for
> updating it** — accepting that every non-ASCII classification and casing
> result changes, and that `Rune::IsWhiteSpace` also changes, because this
> port's white-space table wrongly contains U+FEFF. Ticket **#2018**.
> *(Recommended alternative if no Unicode data source is available: approve only
> the U+FEFF correction plus a class-level statement of the ASCII scope, and
> keep #2018 blocked.)*

---

## 8. Summary table

| # | Ticket | Finding | Cause | Family | Gate in one line | Layout | Vtable | Signature | Recommendation |
|---|---|---|---|---|---|---|---|---|---|
| A1 | **#2013** | SR-AUD-288 | T-G | A | a currently-succeeding setter throws | **none** under A′ | **none** under A′ | 2 new members | **approve (A′)** |
| A2 | **#2021** | SR-AUD-299 | T-F | A | `GetEncoding` returns a different object and can throw | none | none | none | **approve** |
| B | **#2015** | SR-AUD-290, 296 | T-I | B | the meaning of every public index | none | none | none | **approve as a permanent deviation** |
| C1 | **#2014** | SR-AUD-289 | T-H | C | produced bytes change for all non-ASCII | none | none | none | approve **with C2, C3** |
| C2 | **#2016** | SR-AUD-291 | T-J | C | two factories' bytes change; decode changes; **vtable** | none | **yes** (a) | 1 new virtual | approve **with C1, C3** |
| C3 | **#2017** | SR-AUD-292, 293 | T-K | C | a configured exception fallback starts throwing | none | **if widened** | **if widened** | approve **with C1, C2**; answer the `char` question |
| D | **#2020** | SR-AUD-298 | T-N | D | accepted grammar changes **both ways**; **closes CCF-012** | none | none | none | **approve (option A)** |
| E | **#2019** | SR-AUD-297 | T-M | E | emitted text changes; **target unverifiable here** | none | none | 2 new statics | **defer** |
| F | **#2018** | SR-AUD-294 | T-L | F | every non-ASCII classification changes; needs a data table | none | none | none | **defer, or take the U+FEFF half** |

### The compact checklist

```
Approval A1 — #2013  Read-only factory encodings, by identity (no layout/vtable change),
                     plus IsReadOnly and Clone.                       RECOMMENDED
Approval A2 — #2021  EncodingInfo::GetEncoding resolves its own code page and
                     throws for an unimplemented one.                 RECOMMENDED
Approval B  — #2015  Public indices stay UTF-8 bytes, permanently and by decision,
                     recorded in CLAUDE.md as a known deviation.      RECOMMENDED
Approval C1 — #2014  Latin-1 converts scalars, not storage bytes.     with C2+C3
Approval C2 — #2016  No BOM in GetBytes (BigEndianUnicode AND UTF32), no BOM
                     stripping in GetString, virtual GetPreamble.     with C1+C3
Approval C3 — #2017  Every encoding routes through its configured fallback.
                     SEPARATE QUESTION: may the fallback surface widen from
                     char to a scalar type?                           with C1+C2
Approval D  — #2020  One shared, non-rendering composite-format scanner;
                     Parse both narrows and widens.  CLOSES CCF-012.  RECOMMENDED
Approval E  — #2019  Default Web encoders' allow-list.                DEFER — no evidence
Approval F  — #2018  Real Unicode tables in Rune.                     DEFER — no data source
```

---

## 9. What this verification changed in the repository

**One ticket, test-only: #2022.** The #2006 batch stated that every gated
behaviour was pinned "so none can land silently". Re-measured, that was true for
#2013, #2014, #2015, #2016's UTF-32 half and #2017's decoder half, and **false**
for five things: SR-AUD-294 (#2018) and SR-AUD-299 (#2021) had **no pin at
all** — every Rune test in the repository uses ASCII, so all of them pass
identically before and after a Unicode repair, and `EncodingInfo` had no test
anywhere — while SR-AUD-291, SR-AUD-292 and SR-AUD-298 were pinned only in part.

`modules/text/tests/System/Text/TextGatedBehaviourPinTests.cpp`, +8 tests,
add-only; `SharpRuntimeTests_Text` **288 → 296**. No production file was
touched. **Mutation-checked three ways in one pass** — a Latin-1-widened
`Rune::IsLetter`, U+FEFF removed from `Rune::IsWhiteSpace`, and
`EncodingInfo::GetEncoding` resolving 20127 to ASCII — each rebuilt and
re-executed: all three **fail** the new pins and all three **pass** the
pre-existing `TextUnitContractTests`, which is precisely the gap. Mutations
reverted; suite green at 296.

No other blocked ticket contained a separable compatible portion. Each was
tested against the rule: #2014, #2016, #2017, #2018, #2019, #2020 and #2021 all
change a **defined, currently-observable** result on valid input, and #2013
changes a thread-safety guarantee; none can be split without an approval.

---

## 10. Corrections to `docs/SystemTextNamespaceReviewPlan.md` §14

Historical text preserved; these are appended corrections, each measured.

| § | What §14 says | What was measured (2026-08-03) |
|---|---|---|
| 14.1 | recommends "adding an `IsReadOnly` property and a `Clone()`" | that spelling costs `sizeof(Encoding)` **40 → 48** and, if `Clone()` is virtual, a **vtable slot**. §14.1 states neither. An identity-based `IsReadOnly` plus a code-page-dispatching `Clone()` (**option A′**) has neither cost. |
| 14.4 | "changing the bytes `Encoding::UTF32()` produces" | **two** default factories emit a BOM as payload: `BigEndianUnicode()` = `feff0041` and `UTF32()` = `fffe000041000000`. |
| 14.4 | silent on the decode direction | `GetString` **consumes** a leading U+FEFF in both UTF-16 and UTF-32, so a real ZWNBSP is discarded; a round trip cancels both halves, which is why no test saw either. |
| 14.5 | "route … through its configured fallback" | the fallback surface takes a **`char`**, which cannot carry a non-ASCII scalar. Routing *scalars* needs a public virtual signature change — a second gate §14.5 does not ask about. |
| 14.6 | frames `IsWhiteSpace` as the Unicode-aware sibling | it is Unicode-aware **and divergent**: this port's table contains U+FEFF, which .NET excludes. |
| 14.8 | "any index at or above 1,000,000 begin to throw" | **false.** The shared grammar accepts `{1000000}` … `{9999999}`; the first rejected shapes are eight-digit indices and `{2147483646}`. |
| 14.8 | describes narrowing only | adoption also **widens**: `{0 }` and `{0  ,5}` are `FormatException` today and are accepted by the shared grammar. |
| 14.8 | "validates with `System::detail::runCompositeFormat`'s grammar" | `runCompositeFormat` is a **formatting** engine needing an `argCount` and a `render`, and it **pads while validating** (measured `pad=1000000` for `{0,1000000}`). #2020 must extract a non-rendering scanner into `modules/core` — a much wider blast radius than §14.8 implies. |
| 14.9 | silent on what object is returned | `GetEncoding()` returns **`Encoding::UTF8()` itself**, the shared mutable singleton, so #2021 and #2013 are one decision about one object. |
| §5 / §4.6 | "a fourth" / "a third" composite-format grammar | measured today there are exactly **two** implementations; `CompositeFormat::Parse` is the second and the only divergent one. |
| §18, criterion 2 | every gated ticket "carries a blocked ticket whose notes name the approval sentence" — read together with the batch's "pinned by a permanent test" claim | two blocked findings had **no** pin. Closed by #2022 (§9). |

---

## 11. Evidence status — what could not be verified here

`/rv/tmp/runtime/src/libraries/` re-verified **absent**; `/rv` absent; no .NET
runtime installed. Per row:

| # | Claim a decision rests on | Status |
|---|---|---|
| 1 | the TSan race between `Encoding::UTF8()`'s setter and a concurrent decode | **audit evidence, not re-run here.** #2013's own sanitizer plan re-runs it before/after. |
| 2 | .NET's factory encodings are read-only and their setters throw `InvalidOperationException` | **inferred, not verifiable here.** The *repair* does not depend on .NET's exact exception type — but the approval sentence names one, so a reviewer who wants .NET parity exactly should confirm it. |
| 3 | .NET's `GetString` does not strip a BOM | **inferred.** The port's stripping is nonetheless internally inconsistent with `GetPreamble`'s absence, which is verifiable here. |
| 4 | .NET's exact default Web-encoder allow-list and escape spelling | **not verifiable here** — the reason Approval E is a deferral. |
| 5 | whether .NET's `EncodingInfo.GetEncoding` raises `ArgumentException` or `NotSupportedException` for an unknown page | **not verifiable.** #2021 should adopt this port's own precedent rather than guess. |
| 6 | the Unicode category and case tables themselves | **no data source in this container** — the reason Approval F is a deferral. |
| 7 | everything in §5.2's measured table | **measured here**, `build-probe/2022_probe1_verify.log` §A. |
| 8 | everything in §2.1, §2.2, §4.2, §4.3, §7's tables | **measured here**, same log, §§B–G. |

**No approval in this package is recommended on inferred evidence alone.** The
two that would need it (E and F) are the two marked "defer".
