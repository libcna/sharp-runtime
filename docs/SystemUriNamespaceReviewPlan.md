<!-- SPDX-License-Identifier: MIT -->

# `System::Uri` namespace review and remediation plan

*Ticket #1987. Written 2026-08-03 against commit `d998bda`
(`feature/remediation-batch-system-runtime-review` tip), on branch
`feature/remediation-batch-system-uri-review`.*

This document is the durable record for the `modules/uri` namespace, in the style of
`docs/ThreadingNamespaceReviewPlan.md` (#1950),
`docs/ThreadingTasksChannelsReviewPlan.md` (#1964) and
`docs/SystemRuntimeNamespaceReviewPlan.md` (#1972). It converts every open audit
finding in this namespace into exactly one disposition, groups them by **root cause**
rather than by source line, states what is compatible and what needs explicit approval,
and records the corrections this review made to the audit record.

Every "current behaviour" claim below is **measured**, not recalled. The measurement is
`build-probe/1987_probe1_uri_boundaries.cpp`, whose output before any change is retained
as `build-probe/1987_probe1_before.log`. Where a claim about **.NET** decides whether a
change widens or narrows accepted input, the evidence is named explicitly and, when it is
absent, the item is deferred rather than guessed — the `/rv/tmp/runtime/src/libraries/`
reference tree is **not present in this environment**.

---

## 1. Why `System::Uri` is next

`docs/SystemRuntimeNamespaceReviewPlan.md` §1 chose `System::Runtime` over this namespace
on **severity**: `runtime` carried three high-severity findings and `uri` carries none.
That comparison was correct and is not revised. `System::Runtime`'s compatible half is now
complete (its §25 scoreboard), so the tie-break no longer applies and `System::Uri` is the
namespace the previous handoff recommended (`NEXT.md` §14).

The independent check this review owes that recommendation, re-measured here rather than
carried over:

| Claim in the handoff | Verified? |
|---|---|
| 14 open findings | **Yes** — SR-AUD-138 … SR-AUD-151, contiguous, no gaps |
| all medium | **Yes** — 0 high, 0 low |
| all still `confirmed` | **Yes** — none is `remediated`, none carries the `confirmed (design-complete)` qualifier |
| no existing `docs/*Plan.md` | **Yes** — this document is the first |
| small enough for one batch | **Yes** — 12 headers + 1 `.cpp` (363 lines) + 12 test files, 2,123 lines total |

`text` (14 findings) remains the alternative and is untouched.

**One structural property makes this namespace unusually tractable**: every parsing
decision lives in a **single 363-line `.cpp`**, `modules/uri/src/System/Uri.cpp`, behind a
**single private entry point**, `Uri::parse`. Nine of the fourteen findings are reachable
from that one function or from the two constructors next to it.

---

## 2. Scope and file inventory

Component `Uri` (`modules/uri`), static target `sharp_runtime_uri`, public dependency
`Core.Base` only. It is depended on by `IO`, `Net`, `Net.Http.Headers`, `Net.WebSockets`,
`Xml` (public) and `Net.Http` (private) — **six downstream components**, which is why
every acceptance change below is examined for consumer impact.

### 2.1 Files in scope

| File | Lines | Findings |
|---|---|---|
| `include/System/Uri.hpp` | 158 | (declares the surface SR-AUD-142/144/145 defects live behind) |
| `src/System/Uri.cpp` | 363 | **SR-AUD-142, 143, 144, 145** |
| `include/System/UriBuilder.hpp` | 251 | **SR-AUD-138, 139, 140, 141** |
| `include/System/UriParser.hpp` | 93 | **SR-AUD-146, 147** |
| `include/System/UriTypeConverter.hpp` | 73 | **SR-AUD-148** |
| `include/System/UriCreationOptions.hpp` | 23 | **SR-AUD-149** |
| `include/System/UriPartial.hpp` | 20 | **SR-AUD-150** |
| `include/System/UriHostNameType.hpp` | 21 | **SR-AUD-151** |
| `include/System/UriComponents.hpp` | 41 | none |
| `include/System/UriFormat.hpp` | 19 | none |
| `include/System/UriFormatException.hpp` | 32 | none |
| `include/System/UriIdnScope.hpp` | 20 | none |
| `include/System/UriKind.hpp` | 5 | none (forwarding header) |
| `CMakeLists.txt`, `README.md` | 9 + 7 | none |
| `tests/System/*.cpp` (12 files) | 999 | — |

`SharpRuntimeTests_Uri` runs **149 tests in 12 suites** at the start of this batch
(`UriTests` 57, `UriBuilderTest` 27, `UriParserTest` 14, `UriComponentsTest` 12,
`UriTypeConverterTest` 7, `UriFormatExceptionTest` 6, `UriHostNameTypeTest` 6,
`UriPartialTest` 5, `UriFormatTest` 4, `UriIdnScopeTest` 4, `UriKindTest` 4,
`UriCreationOptionsTest` 3).

### 2.2 Complete public-surface inventory

**`System::Uri`** — 3 constructors (`(string)`, `(string, UriKind)`, `(const Uri&, string)`),
13 accessors (`AbsoluteUri`, `OriginalString`, `Scheme`, `Host`, `Port`, `AbsolutePath`,
`Query`, `Fragment`, `UserInfo`, `IsAbsoluteUri`, `PathAndQuery`, `Authority`, `IsLoopback`),
`ToString()`, `GetHashCode()`, `operator==`, `operator!=`, and the static `TryCreate`.
Nine private data members; `sizeof(System::Uri) == 240` (measured).

**`System::UriBuilder`** — 6 constructors, 8 getter/setter pairs (`Scheme`, `Host`, `Port`,
`Path`, `Query`, `Fragment`, `UserName`, `Password`), `ToString()`, `getUriProperty()`,
`Equals`, `GetHashCode`. Header-only; `sizeof(System::UriBuilder) == 232` (measured).

**`System::UriParser`** — protected default constructor, virtual destructor, three public
virtuals (`GetComponents`, `IsBaseOf`, `IsWellFormedOriginalString`), one static
(`IsKnownScheme`). **Has a vtable.**

**`System::UriTypeConverter`** — default constructor, virtual destructor, four public
virtuals (`CanConvertFrom`, `CanConvertTo`, `ConvertFrom`, `ConvertTo`). **Has a vtable.**

**Enums** — `UriKind` (3), `UriComponents` (flags, 17 named), `UriFormat` (3),
`UriHostNameType` (5), `UriIdnScope` (3), `UriPartial` (4).
**Value type** — `UriCreationOptions` (one public `bool` field).
**Exception** — `UriFormatException : FormatException` (3 constructors).

### 2.3 Public .NET surface this port does **not** have

Recorded so the plan's "explicit exclusions" are inventory-backed, not vague:
`GetLeftPart`, `CheckHostName`, `CheckSchemeName`, `IsWellFormedOriginalString`,
`IsBaseOf`, `MakeRelativeUri`, `EscapeDataString`, `UnescapeDataString`,
`EscapeUriString`, `Compare`, `HostNameType`, `Segments`, `IsFile`, `IsUnc`,
`LocalPath`, `DnsSafeHost`, `IdnHost`, `IsDefaultPort`, `UserEscaped`,
the `UriScheme*` constants, `UriParser::Register`, and the options-bearing
`Uri`/`TryCreate` overloads. Four of these are the subject of findings
(SR-AUD-146/149/150/151); the rest are unclassified absence.

---

## 3. Confirmed finding inventory — all 14, with the measured current behaviour

Audit numbering is **frozen at 364**. No new `SR-AUD-*` identifier is issued anywhere in
this document; defects found by this review that the audit never named get ordinary ticket
numbers only.

| ID | Sev | File | What it says | Measured now (`1987_probe1_before.log`) | Cause | Disposition |
|---|---|---|---|---|---|---|
| SR-AUD-138 | med | `UriBuilder.hpp` | `user:pass` fused into `UserName`; a later `Password` write corrupts | `copied_user=user:pass`, `copied_password=''`, then `http://user:pass:replacement@example.com:80/path` | **U-F** | **#1993 compatible** |
| SR-AUD-139 | med | `UriBuilder.hpp` | relative string input is not promoted to `http://` | `scheme=''`, `host=''`, `ToString()==":///www.example.com/path"` | **U-H** | **#1996 blocked (design)** |
| SR-AUD-140 | med | `UriBuilder.hpp` | `Equals`/`GetHashCode` compare raw rendered text | credential-only `Equals`=0, fragment-only `Equals`=0 | **U-G** | **#1995 blocked (design)** |
| SR-AUD-141 | med | `UriBuilder.hpp` | `Scheme`/IPv6 `Host` setters skip normalisation and validation | `"HTTP"` kept; `"bad scheme://localhost/"` rendered; `"http://::1/"` rendered | **U-H** | **#1996 blocked (design)** |
| SR-AUD-142 | med | `Uri.cpp` | raw case/default-port text is the semantic identity | `scheme=HTTP`, `host=EXAMPLE.COM`, `a==b` false, hashes differ | **U-G** | **#1995 blocked (design)** |
| SR-AUD-143 | med | `Uri.cpp` | opaque URIs bypass the file's own default-port table | `mailto:` → `port=-1` (table says 25); **`telnet:` → -1 too** | **U-B** | **#1989 compatible** |
| SR-AUD-144 | med | `Uri.cpp` | query-only / fragment-only / network-path references treated as paths | `?new`→`/a/?new`; `#new`→`/a/#new`; `//other.example/c`→`http://example.com//other.example/c` | **U-C** | **#1990 compatible** |
| SR-AUD-145 | med | `Uri.cpp` | malformed bracketed IPv6 accepted; out-of-domain `UriKind` silently ignored | `http://[::1/path` → `host="[:"` **and `port=1`**; `(UriKind)99` and `(UriKind)-1` both accepted | **U-D**, **U-E** | **#1991 + #1992 compatible** |
| SR-AUD-146 | med | `UriParser.hpp` | no `Register`, protected hooks exposed as public stubs | `Register` absent (compile-time) | **U-I** | **#1997 blocked (design)** |
| SR-AUD-147 | med | `UriParser.hpp` | `IsKnownScheme` accepts malformed/empty scheme text | `IsKnownScheme("")`=0, `IsKnownScheme("ht tp")`=0 | **U-J** | **#1998 blocked (design)** |
| SR-AUD-148 | med | `UriTypeConverter.hpp` | empty text throws where .NET returns null | `ConvertFrom("")` throws `UriFormatException` | **U-I** | **#1999 blocked (design)** |
| SR-AUD-149 | med | `UriCreationOptions.hpp` | inert flag, no `Uri` consumer | no `Uri` ctor / `TryCreate` overload accepts it | **U-I**, **U-K** | **#1997 blocked + #1994 disclosure** |
| SR-AUD-150 | med | `UriPartial.hpp` | no `Uri::GetLeftPart` consumer | absent (compile-time) | **U-I** | **#1997 blocked (design)** |
| SR-AUD-151 | med | `UriHostNameType.hpp` | no `Uri::CheckHostName` classifier | absent (compile-time) | **U-I** | **#1997 blocked (design)** |

**Nothing disappears**: 14 findings in, 14 dispositions out. Counted by *finding*, which is
how the index counts them: **4 findings** are closed outright by the compatible tickets —
SR-AUD-138 (#1993), SR-AUD-143 (#1989), SR-AUD-144 (#1990) and SR-AUD-145, whose two
unrelated halves needed both #1991 and #1992 — and the remaining **10** go to the five
approval-gated design tickets #1995–#1999. SR-AUD-149 is in that ten and *additionally*
receives #1994's disclosure half, which does not close it. Counted by *repair* instead of by
finding the compatible column reads six, because SR-AUD-145 is two repairs and SR-AUD-149's
disclosure is a seventh ticket; that is the same fourteen findings seen from the ticket side,
not a different reconciliation. No finding is dismissed, deduplicated away, or marked
already-remediated.

*(Correction, 2026-08-03 recovery pass: this paragraph previously read "6 to compatible
implementation tickets …, 8 to four approval-gated design tickets (#1995–#1999)". Both
numerals were wrong against the index — the gated set is **five** tickets holding **ten**
findings — and the 6/8 split silently mixed the per-repair and per-finding counts. The
dispositions themselves, the §3 table above and the index are unchanged; only this summary
sentence was miscounted. The same miscount is frozen into commit `2ba68fa`'s message and
into `501d866`'s "eight findings whose repairs are approval-gated"; those cannot be
corrected without rewriting history, so they are recorded here instead.)*

---

## 4. Corrections to the audit record

Every correction below was **measured**, not reasoned. Each is appended to the owning
per-file report with the historical text preserved verbatim, per the standing rule.

### 4.1 SR-AUD-145's IPv6 half loses the **port** as well as the host

The finding says the parser "stores host `[:`". Measured, `http://[::1/path` yields
**`host="[:"` and `port=1`**. The mechanism is one `rfind(':')` over an authority whose
brackets were never checked: for `"[::1"` the last colon is at index 2, so `"1"` becomes
the port and `"[:"` becomes the host. A repair that only rejects the missing bracket is
correct, but a reader of the finding alone would not know that a *port number* is also
being fabricated out of address text — which is the more dangerous half, because a caller
that connects to `Port` reaches port 1 rather than failing.

The same measurement found **two further bracketed shapes the finding does not name**:

- `http://[::1]junk/path` → `host="[::1]junk"`, `port=80`. Accepted with a host that is
  neither a literal nor a name.
- `http://[]/path` → `host="[]"`, `port=80`. An empty IP-literal.

All three are the same defect — *the bracket structure of an IP-literal authority is never
validated* — so #1991 repairs the shape, not the named site. This is the same rule
`docs/SystemRuntimeNamespaceReviewPlan.md` §4.1 applied to SR-AUD-155.

### 4.2 SR-AUD-143 is not `mailto`-specific

The finding names `mailto` only. Measured, **every** opaque scheme with a table entry
loses its default port, because the opaque branch assigns `port_ = -1` unconditionally:
`telnet:host.example.com` also reports `-1` where `defaultPortForScheme` says 23. The
repair is one line in the opaque branch, but the ticket's tests must cover more than
`mailto`.

### 4.3 A **second, unnamed** site loses the default port: the empty port

Not in any finding. Measured:

```
http://example.com/     -> port=80     (no colon: default applied)
http://example.com:/    -> port=-1     (empty port: default NOT applied)
http://example.com:8080 -> port=8080
```

`parse` applies `defaultPortForScheme` only on the branch where the authority contains no
colon at all. An authority that ends in a bare `:` takes the other branch, sets the host,
and leaves `port_` at its `-1` initialiser. Two spellings of the same URI therefore
disagree about the port. Folded into #1989 because it is the same root cause — *the
default-port table is not consulted on every path that produces a port* — and separating
them would leave the family half-closed.

### 4.4 The largest defect in this namespace is one the audit never issued a finding for

`Uri::parse` locates the scheme with `uriString.find("://")` — **a search for `"://"`
anywhere in the string** — and only falls back to the grammar-correct `findSchemeColon`
when that search fails. `findSchemeColon`'s own doc-comment states the RFC 3986 rule
("ALPHA *(ALPHA / DIGIT / "+" / "-" / ".") ':'"), so the file holds **two contradictory
notions of where the scheme ends** and consults the wrong one first.

Measured consequences, all of them ordinary input a game or web client produces:

| Input | Current | Why |
|---|---|---|
| `/path?redirect=http://evil.com` | **throws** `URI scheme must start with a letter` | `find("://")` matched inside the *query*, so `scheme_` became `/path?redirect=http` |
| `search?url=https://example.com` | **throws** `Invalid character in URI scheme` | same |
| `mailto:a@b.com?body=see http://x` | **throws** `Invalid character in URI scheme` | same, on an opaque URI |
| `foo:bar://baz` | **throws** `Invalid character in URI scheme` | scheme taken as `foo:bar` |

A relative reference whose query embeds an absolute URL — the single most common shape in
redirect and callback parameters — cannot be constructed at all. This is a **false
rejection of well-formed input**, which the batch prompt ranks above every other class
except crashes and silent wrong components, and it is repaired by #1988.

Its compatibility is not a judgement call, it is a proof (§9.1): every input the current
code *accepts* takes the identical branch and produces the identical components under the
repair, so the change can only convert throwing inputs into parsing ones.

### 4.5 `SR-AUD-140`'s consequence has an availability half the finding does not name

The finding is about `Equals`/`GetHashCode` disagreeing with URI identity. Measured, there
is a sharper failure: `UriBuilder::GetHashCode()` builds a `Uri` from `ToString()`, so for
a builder whose scheme is invalid it **throws** where `Equals` on the same object returns
`true`:

```
self-Equals with invalid scheme      : 1
GetHashCode with invalid scheme      : THROWS Invalid character in URI scheme
```

An object that compares equal to itself has no obtainable hash. Recorded as post-audit
ticket **#2004**; it is *not* folded into #1995, because #1995's repair (delegate identity
to `Uri`) would make the throw *more* likely, not less, and the two need separating.

### 4.6 The audit's "documented no-percent-encoding boundary" is broader than it reads

`Uri.hpp`'s class doc-comment discloses "No percent-encoding/decoding". Measured, the
disclosure is accurate for escaping but **the same header makes a claim that is simply
false**: `getSchemeProperty`'s doc-comment says the scheme is returned *"lower-case as
parsed"*, and the parser never lower-cases anything (`HTTP://…` → `HTTP`). That is not an
adaptation boundary, it is a documentation defect of exactly the shape
`docs/SystemRuntimeNamespaceReviewPlan.md` cause R-J covers (SR-AUD-059). It is repaired
by #1994 without touching behaviour, and it is *not* used to justify closing SR-AUD-142,
which remains open and approval-gated.

### 4.7 Two audit metadata premises that could not be re-verified, and are therefore not relied on

- Every URI report cites probes under `/tmp/sharp-runtimervc-uri-*-audit-probe`. **Those
  paths do not exist in this environment** and `/rv/tmp/runtime/src/libraries/` is absent.
  Where a repair below depends on what .NET does, §7 names the *surviving* evidence
  explicitly; where none survives, the item is deferred (§14) rather than implemented.
- `Uri.cpp`'s report says `UriTests.*` "passed 57/57 on 2026-07-27". Re-measured today:
  still 57, still all passing. No drift.

---

## 5. Root causes

Eleven causes, U-A … U-K. The grouping is by *why the code is wrong*, not by which file or
line it is on; three causes span two files.

### U-A — the scheme is located by substring search rather than by grammar (0 findings, 1 unnamed defect)

`parse` prefers `find("://")` over `findSchemeColon`. Single root cause of §4.4's four
measured false rejections. **Ticket #1988, compatible.**

### U-B — the default-port table is not consulted on every path that produces a port (1 finding + 1 unnamed site)

The opaque branch hard-codes `-1` (SR-AUD-143) and the empty-port branch never calls the
table (§4.3). **Ticket #1989, compatible.**

### U-C — relative-reference resolution implements only the path case of RFC 3986 §5.3 (1 finding)

`Uri(base, relative)` splits off a `?`/`#` tail, merges the remainder as a path, and
re-appends the tail. RFC 3986 §5.3 — which this file already cites twice — distinguishes
four cases; three of them are unimplemented (SR-AUD-144). **Ticket #1990, compatible.**

### U-D — an IP-literal authority's bracket structure is never validated (½ finding)

SR-AUD-145's first half, plus §4.1's two extra shapes. **Ticket #1991, compatible.**

### U-E — a public enum's domain is not checked at the boundary (½ finding)

SR-AUD-145's second half: `static_cast<UriKind>(99)` matches neither guarded branch and
behaves as `RelativeOrAbsolute`. **This is the same cause as `System::Runtime`'s R-F
(#1976, `GCSettings`) and `System::Threading`'s T-C (#1954)** — an out-of-domain enum cast
crossing a public boundary — and it reuses that policy rather than inventing a new one.
**Ticket #1992, compatible.**

### U-F — a compound component is stored whole where the API declares its parts (1 finding)

`UriBuilder::setFieldsFromUri` assigns the entire user-info to `userName_` although the
type publishes `UserName` and `Password` separately (SR-AUD-138). **Ticket #1993,
compatible.**

### U-G — identity is raw text rather than URI semantics (2 findings, approval-gated)

`Uri::operator==`/`GetHashCode` compare `absoluteUri_`; `UriBuilder::Equals`/`GetHashCode`
compare `ToString()` (SR-AUD-142, SR-AUD-140). Changing this changes **equality
semantics**, which the batch prompt gates explicitly. **Ticket #1995, blocked.**

### U-H — public setters and constructors accept text they must normalise or reject (2 findings, approval-gated)

`UriBuilder`'s `Scheme`/`Host` setters and its string constructor (SR-AUD-141,
SR-AUD-139). Every member is either a narrowing at a public boundary or a change to what
`ToString()` emits. **Ticket #1996, blocked.**

### U-I — the public shape itself is absent (4 findings, approval-gated)

`UriParser::Register` + hook participation (SR-AUD-146), `UriCreationOptions`' consumer
overloads (SR-AUD-149), `Uri::GetLeftPart` (SR-AUD-150), `Uri::CheckHostName`
(SR-AUD-151), and `UriTypeConverter`'s unrepresentable null result (SR-AUD-148 — which
needs a *return-type* decision, so it is split into its own ticket). Additive public API.
**Tickets #1997 and #1999, blocked.**

### U-J — a public static accepts malformed argument text (1 finding, approval-gated)

`UriParser::IsKnownScheme` returns `false` for `""` and `"ht tp"` where .NET throws
`ArgumentOutOfRangeException` (SR-AUD-147). A **narrowing** at a public boundary whose
reference basis is a probe that no longer exists here. **Ticket #1998, blocked.**

### U-K — documentation states a contract the code does not implement (0 findings, disclosure work)

`getSchemeProperty`'s "lower-case as parsed" (§4.6) and `UriCreationOptions`' disclosure
half of SR-AUD-149. Same cause as `System::Runtime` R-J. **Ticket #1994, compatible.**

---

## 6. Findings that are *not* in this namespace's queue

- **CCF families**: none of the twenty cross-cutting families names a `modules/uri` file.
  Verified by search over `audit/AUDIT_CROSS_CUTTING_FINDINGS.md` — zero hits for
  `modules/uri` or any `SR-AUD-13x/14x/15x` URI identifier. **U-E is nonetheless governed
  by an existing policy** (the `GCSettings`/`WaitHandle` enum-domain rule) and does not get
  a new family, per the standing "do not create a second policy for a new occurrence"
  instruction.
- **`HttpClient::parseUrl`** (`modules/net-http`) is a *second, independent* URL parser in
  this repository. It is **out of scope** as production code — but it is **in scope as
  evidence** (§7), because it already rejects exactly what #1991 makes `Uri` reject.
- `System::Net`, `System::Xml` and `System::IO` consumers of `Uri` are examined for impact
  (§10) but not reviewed.

---

## 7. Reference evidence actually available, per repair

The `/rv/tmp/runtime/src/libraries/` tree is **absent**. The audit's `/tmp/...` probe
directories are **absent**. This section states, per compatible ticket, what evidence
survives inside the permitted environment — the same discipline
`docs/ThreadingTasksChannelsReviewPlan.md` used to separate #1968 (probe-backed, landed)
from #1963 (no probe, declined).

| Ticket | Does the change alter what input is accepted? | Surviving evidence |
|---|---|---|
| **#1988** | **Widens only** — proved in §9.1 that no accepted input changes | Needs no external reference: it resolves a **contradiction inside one file** in favour of that file's own documented grammar (`findSchemeColon`'s RFC 3986 comment) |
| **#1989** | No. Changes a returned **value**, not acceptance | Resolves a contradiction inside one file: `defaultPortForScheme` already declares `mailto=25`, and the finding records a managed probe printing `25` |
| **#1990** | No. Changes the **result** of a resolution that already succeeds | RFC 3986 §5.3, already cited twice in `Uri.cpp`; plus SR-AUD-144's recorded managed outputs for all three shapes |
| **#1991** | **Narrows** — three malformed bracketed authorities become rejections | **Repository-contained**: `HttpClient::parseUrl` rejects an unterminated IPv6 literal with `UriFormatException`, pinned by `HttpClientUrlParseTests.IPv6Literal_Unterminated_ThrowsUriFormatException`. SR-AUD-145 records .NET doing the same |
| **#1992** | **Narrows** — an out-of-domain enum cast becomes a throw | The repository's own already-approved policy for this cause (#1976 `GCSettings`, #1954 `WaitHandle`), plus SR-AUD-145's statement that .NET throws `ArgumentException` |
| **#1993** | No. Splits a value between two existing getters; `ToString()` is byte-identical | SR-AUD-138 records the reference printing `copied_user=user`, `copied_password=pass` |
| **#1994** | No. Documentation only | none needed |

Two consequences of that table are stated rather than buried:

1. **#1991 and #1992 are the only two narrowings in this batch.** Both are supported by
   evidence that lives *inside this repository*, and both reject input that currently
   produces a demonstrably invalid object (a fabricated port, a host of `"[:"`, or a URI
   kind that is neither of the three named values). Neither is a policy change about
   well-formed URIs.
2. **Nothing in this batch touches percent-encoding, normalisation, canonicalisation,
   equality or IDN.** Those are §15's exclusions and §10's approval package.

---

## 8. Compatible versus approval-sensitive classification

| Ticket | Cause | Findings | Compatible? | Why |
|---|---|---|---|---|
| **#1988** | U-A | (unnamed) | **yes** | strict widening, proved |
| **#1989** | U-B | SR-AUD-143 | **yes** | internal contradiction; no acceptance change |
| **#1990** | U-C | SR-AUD-144 | **yes** | no acceptance change; RFC + recorded probe |
| **#1991** | U-D | SR-AUD-145a | **yes** | narrow rejection of already-invalid objects; repository-internal precedent |
| **#1992** | U-E | SR-AUD-145b | **yes** | existing approved enum-domain policy |
| **#1993** | U-F | SR-AUD-138 | **yes** | `ToString()` unchanged; two getters corrected |
| **#1994** | U-K | (SR-AUD-149 disclosure) | **yes** | documentation only |
| **#1995** | U-G | SR-AUD-142, 140 | **no** | equality semantics + canonicalisation |
| **#1996** | U-H | SR-AUD-141, 139 | **no** | narrowing at public setters + changed rendering |
| **#1997** | U-I | SR-AUD-146, 149, 150, 151 | **no** | additive public API (and a vtable change for `UriParser`) |
| **#1998** | U-J | SR-AUD-147 | **no** | narrowing with no surviving reference evidence |
| **#1999** | U-I | SR-AUD-148 | **no** | requires a nullable return representation |

---

## 9. Compatibility proofs and the source / ABI / layout consequence matrix

### 9.1 Proof that #1988 changes nothing that currently parses

Let *S* be the current code and *S′* the repaired code. *S′* computes
`c = findSchemeColon(u)` and treats `u` as hierarchical iff `c != npos` **and** `u[c+1..c+2] == "//"`.

1. **Currently hierarchical.** *S* took this branch iff `find("://") = k` exists and
   `u[0..k)` passes *S*'s scheme validation. That validation admits only
   `ALPHA (ALPHA|DIGIT|'+'|'-'|'.')*`, which contains **no colon**, so the first colon in
   `u` is at index *k*. `findSchemeColon` scans from index 1 and returns the first colon it
   reaches while every preceding character is admissible — that is exactly *k*. And
   `u[k+1..k+2] == "//"` by construction. So *S′* takes the same branch, with the same
   scheme and the same `rest = u.substr(k+3)`. **Identical.**
2. **Currently opaque.** *S* took this branch iff `find("://") == npos` and
   `findSchemeColon(u) = c` exists. If `u[c+1..c+2]` were `"//"` then `u` would contain
   `"://"`, contradicting the premise. So *S′* also treats it as opaque, with the same *c*.
   **Identical.**
3. **Currently relative.** Both searches returned `npos`; *S′*'s single search also returns
   `npos`. **Identical.**

Therefore *S′* differs from *S* only on inputs *S* **threw** on. ∎

The two now-unreachable throws (`"URI scheme must not be empty"` and the per-character
scheme loop) are removed rather than left as dead code, because leaving two scheme
grammars in the file is the defect. Their inputs do not become accepted-with-a-bad-scheme:
they become **relative** URIs, which is what RFC 3986 calls a reference with no scheme.

### 9.2 Consequence matrix

| Dimension | #1988 | #1989 | #1990 | #1991 | #1992 | #1993 | #1994 |
|---|---|---|---|---|---|---|---|
| public signature | none | none | none | none | none | none | none |
| object layout / `sizeof` | none | none | none | none | none | none | none |
| vtable | none | none | none | none | none | none | none |
| mangled symbols | none | none | none | none | none | none | none |
| `noexcept` specification | none | none | none | none | none | none | none |
| component edge | none | none | none | none | none | none | none |
| **accepted input** | **widens** (§9.1) | none | none | **narrows** (3 bracketed shapes) | **narrows** (out-of-domain `UriKind`) | none | none |
| **canonicalisation** | none | none | none | none | none | none | none |
| **equality / hash** | none | none | none | none | none | none | none |
| **exception boundary** | fewer `UriFormatException`s | none | none | new `UriFormatException`s | new `ArgumentException` | none | none |
| **component values** | new inputs get components | `Port` for opaque + empty-port | `AbsolutePath`/`Query`/`Fragment`/`Host` for 3 shapes | n/a (rejected) | none | `UserName`/`Password` | none |

`sizeof(System::Uri) == 240` and `sizeof(System::UriBuilder) == 232` are recorded in the
probe **before** any change and are re-asserted by permanent tests, so a later ticket
cannot grow either type unnoticed.

---

## 10. Downstream consumer impact

Six components depend on `Uri`. Each was examined for the two narrowings:

| Consumer | Uses | Affected by #1991? | Affected by #1992? |
|---|---|---|---|
| `Net` (`WebProxy`, `CookieContainer`, `CredentialCache`) | host/scheme/port reads | only if it is *given* a malformed bracketed authority, which currently yields a fabricated port | no `UriKind` cast |
| `Net.Http.Headers` | `Uri` values in headers | same | no |
| `Net.WebSockets` (`ClientWebSocket`) | scheme/host/port | same | no |
| `Net.Http` (private) | has its **own** parser that already rejects these | no | no |
| `Xml` (`XmlUrlResolver`, `XmlSecureResolver`) | base/relative resolution | same | no |
| `IO` (`FileFormatException`) | stores a `Uri` | no | no |

No consumer casts an integer to `UriKind`: a repository-wide search for
`static_cast<UriKind>`, `static_cast<System::UriKind>` and `(UriKind)` over every `.hpp`
and `.cpp` under `modules/`, `test/`, `tests/` and `bench/` returns **zero hits** — the
only out-of-domain cast that will exist anywhere is the one #1992's own regression writes.
**No mandatory consumer migration** is created by any compatible ticket in this batch, and
no `docs/Migration-*.md` is required.

---

## 11. Test matrix

Systematic vectors, not hand-picked examples. `SharpRuntimeTests_Uri` is the executable;
every ticket adds **add-only** regressions.

| Vector class | Covered by |
|---|---|
| null/empty string | existing (`""` throws) + #1994 pin |
| relative vs absolute, scheme-only, authority-only | **#1988** (8 shapes incl. embedded `://` in query/fragment) |
| empty host, user-info, empty user-info | #1988 pins current; **#2000** records the open defect |
| default and explicit ports, empty port, leading zeros | **#1989** |
| ports 0, 65535, 65536, `+80`, `-80`, 2^31, 2^64+ | existing + **#1989** boundary sweep |
| IPv4 literal | existing loopback tests |
| bracketed IPv6, unterminated, trailing junk, empty literal, with/without port | **#1991** |
| zone identifiers | not supported — §15 exclusion, pinned as current behaviour by #1991 |
| Unicode host, punycode, IDN | §15 exclusion (`UriIdnScope` has no consumer) |
| mixed-case scheme and host | **#1994** pins current (non-normalising) behaviour; change is #1995 |
| percent escapes, malformed escapes (`%zz`, trailing `%`) | pinned as pass-through by #1988's sweep; repair is §15 exclusion |
| escaped slash / backslash | pinned |
| dot and dot-dot segments, repeated separators | existing + **#1990** |
| query-only, fragment-only, network-path references | **#1990** |
| embedded NUL | pinned by #1988's sweep; defect recorded as **#2003** |
| whitespace (leading, trailing, internal) | pinned; deferred verification **#2005** |
| equality and hash consistency | existing + `sizeof` pins |
| canonical string round trip | `OriginalString`/`AbsoluteUri` existing tests |
| relative resolution | **#1990** |
| no-partial-construction | **#1991**, **#1992** (a rejected construction throws; no object exists) |
| exact exception types and parameter names | **#1992** asserts `ArgumentException` **and** `getParamNameProperty() == "uriKind"` |

## 12. Sanitizer matrix

| Ticket | Sanitizer | What it must show | Why |
|---|---|---|---|
| #1988 | ASan + UBSan + LSan | 0 reports over the full vector sweep | the repair changes substring index arithmetic (`substr(c+3)`, `compare(c+1,2,…)`) — an off-by-one is an out-of-range read |
| #1989 | ASan + UBSan | 0 reports | value change only; a non-discriminating result is expected and must be reported as such |
| #1990 | ASan + UBSan + LSan | 0 reports over base×relative cross-product | new string slicing on both operands |
| #1991 | ASan + UBSan | 0 reports; the pre-repair `[:`/`port=1` route is exercised | bracket scanning is index arithmetic on attacker-shaped text |
| #1992 | UBSan | 0 reports, specifically no invalid-enum-load | the check itself casts an out-of-domain enum to an integer |
| #1993 | ASan + UBSan | 0 reports | `substr` on user-info |
| — | TSan | **not run** | `Uri` and `UriBuilder` have no shared cache, no lazy canonicalisation and no hidden mutable state — every member is set in the constructor and read `const` afterwards. Stated rather than silently skipped. |

Every sanitizer binary must compile `modules/uri/src/System/Uri.cpp` **from source** on the
command line; linking `build/libsharp_runtime_uri.a` would leave the changed body
uninstrumented, and the resulting "clean" is meaningless. Instrumentation is proved by
symbol count before the result is believed, per
`docs/ThreadingNamespaceReviewPlan.md` §19.4.

---

## 13. Recommended execution order

1. **#1988** (U-A) — first, because it changes which branch of `parse` every other ticket's
   vectors take. Landing it after #1989/#1991 would invalidate their before/after logs.
2. **#1989** (U-B) — one-line-per-site, independent.
3. **#1991** (U-D) — must land after #1988 (it edits the authority split that #1988 feeds).
4. **#1992** (U-E) — independent of the parser.
5. **#1990** (U-C) — the largest body change; benefits from #1988 and #1989 being settled.
6. **#1993** (U-F) — `UriBuilder` only, independent of everything above.
7. **#1994** (U-K) — documentation, last, so it can describe the landed state.

Then the approval package (§14) and the post-audit defect queue (§16).

---

## 14. Approval package — the five gated causes

Each has a complete design, an exact approval sentence, and a blocked ticket. **No approval
is requested implicitly and none is assumed.**

### 14.1 #1995 — U-G: URI identity

**Current.** `Uri::operator==` compares `absoluteUri_`; `GetHashCode` hashes it.
`UriBuilder::Equals` compares `ToString()`; `GetHashCode` hashes the built `Uri`.
Measured: `HTTP://EXAMPLE.COM:80/Path` != `http://example.com/Path`, different hashes.

**Proposed.** Canonicalise for comparison only: case-fold scheme and host, treat an
explicit default port as absent, and compare component-wise; `UriBuilder` delegates to
`Uri`. `AbsoluteUri`/`OriginalString` keep returning the raw input, so no *rendered* text
changes.

**Files.** `modules/uri/src/System/Uri.cpp` (`operator==`, `GetHashCode`),
`modules/uri/include/System/UriBuilder.hpp` (`Equals`, `GetHashCode`).

**Impact.** No signature, layout, vtable, mangled-symbol or `noexcept` change. **Equality
semantics change**, and therefore so does the behaviour of any container keyed by `Uri`.
Two currently-unequal pairs become equal; nothing that is equal becomes unequal.

**Alternatives.** (a) add `Uri::Equals(const Uri&, UriComponents)` and leave `operator==`
alone — additive, no semantic change, but leaves SR-AUD-142 open; (b) canonicalise at
*parse* time — larger, changes `AbsoluteUri`, and collides with §15's canonicalisation
exclusion.

**Rollback.** Both bodies are `.cpp`/inline; reverting the commit restores raw comparison
with no ABI consequence.

> **Approval sentence:** *"Change `System::Uri`'s equality and hash to compare
> case-folded scheme and host with the scheme's default port treated as absent, and make
> `System::UriBuilder::Equals`/`GetHashCode` delegate to that identity — changing equality
> semantics for currently-unequal URI pairs while leaving every rendered string, signature,
> layout and vtable unchanged."*

### 14.2 #1996 — U-H: `UriBuilder`'s setters and string constructor

**Current.** `setSchemeProperty("HTTP")` keeps `HTTP`; `setSchemeProperty("bad scheme")`
renders `bad scheme://localhost/`; `setHostProperty("::1")` renders `http://::1/`;
`UriBuilder("www.example.com/path")` renders `:///www.example.com/path`.

**Proposed.** Lower-case a valid scheme and throw `ArgumentException` for an invalid one;
bracket a bare IPv6 literal in `Host`; promote a relative constructor string to the default
`http` scheme and `localhost` host before copying fields.

**Impact.** No signature/layout/vtable change. **Two narrowings** (invalid scheme text now
throws from a setter that never threw) and **three rendering changes**. Any consumer that
stores an unvalidated scheme in a builder breaks at the setter.

**Split so the answer can be partial** — G-1 IPv6 bracketing (rendering only, no throw),
G-2 scheme lower-casing (rendering only), G-3 scheme validation (**the only narrowing**),
G-4 relative promotion. **G-1 + G-2 is the recommended minimum.**

> **Approval sentence (recommended minimum):** *"Make `System::UriBuilder` bracket a bare
> IPv6 literal assigned to `Host` and lower-case a valid `Scheme`, changing the text
> `ToString()` emits for those two inputs, with no signature, layout, vtable or exception
> change."*

### 14.3 #1997 — U-I: the absent public surface

Four findings, four independent additions, split so the answer can be partial:

| Group | Addition | Cost |
|---|---|---|
| **A-1** | `Uri::GetLeftPart(UriPartial)` | pure addition, closes SR-AUD-150 |
| **A-2** | `static UriHostNameType Uri::CheckHostName(const std::string&)` | pure addition, closes SR-AUD-151 |
| **A-3** | `Uri(string, UriCreationOptions)` + `TryCreate` overload | pure addition, closes SR-AUD-149's consumer half |
| **A-4** | `UriParser::Register` + protected hooks + `Uri` participation | **changes `UriParser`'s vtable and access levels**; breaks the existing test that calls `GetComponents` publicly |

**A-1 + A-2 is the recommended minimum**: two new members, no existing declaration touched,
no vtable, no layout change, and they close two findings outright.

> **Approval sentence (recommended minimum):** *"Add `System::Uri::GetLeftPart(UriPartial)`
> and the static `System::Uri::CheckHostName(const std::string&)` — strictly additive,
> changing no existing declaration, object layout, vtable or mangled symbol."*

### 14.4 #1998 — U-J: `IsKnownScheme` argument validation

**Current.** Returns `false` for `""` and `"ht tp"`. **Proposed.** Throw
`ArgumentOutOfRangeException("schemeName")` for text that is not a valid scheme token,
matching SR-AUD-147. **Narrowing at a public static.** Deliberately *not* landed with the
compatible half: unlike #1991 and #1992, **no evidence for the .NET behaviour survives in
this environment** — the audit's C# probe directory is gone and the reference tree is
absent. This is exactly the line #1963 sits on, and it is respected.

> **Approval sentence:** *"Make `System::UriParser::IsKnownScheme` throw
> `ArgumentOutOfRangeException` for scheme text that is not a valid RFC 3986 scheme token
> — including the empty string — instead of returning `false`, accepting that the .NET
> behaviour it matches could not be re-measured in this environment."*

### 14.5 #1999 — U-I/SR-AUD-148: `UriTypeConverter`'s unrepresentable null

**Current.** `ConvertFrom("")` throws `UriFormatException`; the return type is a
by-value `Uri`, which cannot express .NET's `null`. Any repair needs a **return-type
change** (`std::optional<Uri>` or `std::shared_ptr<Uri>`) — a public signature change on a
`virtual`, i.e. a vtable-slot signature change, plus mandatory migration for every
override. Kept separate from #1997 for that reason.

> **Approval sentence:** *"Change `System::UriTypeConverter::ConvertFrom`'s return type
> from `System::Uri` to `std::optional<System::Uri>` so an empty input can return the
> empty state instead of throwing — a public virtual signature change requiring every
> override and every caller to migrate."*

---

## 15. Explicit exclusions

Stated so a later reader does not mistake silence for oversight:

1. **Percent-encoding and -decoding** — disclosed in `Uri.hpp` and unchanged. `%2F`,
   `%zz` and a trailing `%` all pass through verbatim (measured); no repair here.
2. **`AbsoluteUri` canonicalisation** — the port returns the raw input string; making it a
   canonical reconstruction is #1995's alternative (b) and is not proposed.
3. **IDN / punycode** — `UriIdnScope` has no consumer and none is added.
4. **IPv6 zone identifiers (`%25eth0`)** — not supported; #1991 validates bracket
   *structure* only and does not begin validating literal *content*.
5. **`Uri::Compare`, `MakeRelativeUri`, `IsBaseOf`, `Segments`, `LocalPath`, `DnsSafeHost`,
   `IdnHost`, `IsFile`, `IsUnc`, the `UriScheme*` constants** — absent, unclassified by the
   audit, not added.
6. **`HttpClient::parseUrl`'s duplication of URI parsing** — a genuine architectural
   redundancy, out of this namespace's scope, and used here only as evidence.
7. **`Uri::TryCreate`'s catch-all** — it swallows `std::bad_alloc` alongside
   `UriFormatException`. Named by the audit's "other missing assertions", not a finding;
   left alone.

---

## 16. Post-audit defects found by this review (no `SR-AUD-*` identifier)

Audit numbering stays frozen at **364**. Each of these was found by *this review's*
measurement, not by the audit, and each gets an ordinary inactive ticket with its
reproduction:

| Ticket | Defect | Reproduction |
|---|---|---|
| **#2000** | an empty authority is accepted: `http://`, `http:///`, `http:///path` and `http://:80/path` all yield `host=""` with the scheme's default port | probe §G |
| **#2001** | the two-`Uri` constructor with an **opaque** base fabricates an authority: `Uri(Uri("mailto:a@b.com"), "c")` → `mailto:///c` | probe §C |
| **#2002** | a **relative** `Uri` never splits its query or fragment: `Uri("?query-only")` has `AbsolutePath == "?query-only"`, `Query == ""` | probe §F |
| **#2003** | an embedded NUL crosses the parser into every component and into `AbsoluteUri` | probe §H |
| **#2004** | `UriBuilder::GetHashCode()` throws for an object whose `Equals` succeeds (§4.5) | probe §M |
| **#2005** | leading/trailing whitespace is rejected (`"  http://example.com/  "` throws); whether .NET trims could not be verified here — **deferred verification** | probe §H |

---

## 17. Deferred verification

- **#2005** — whether .NET trims surrounding whitespace before parsing. Blocked on the
  absent reference tree, exactly like #1963 and #1983.
- **`Uri::TryCreate` with an out-of-domain `UriKind`.** #1992 makes the *constructors*
  throw; `TryCreate`'s existing catch-all therefore converts that into `false` + `nullptr`.
  Whether .NET propagates the `ArgumentException` out of `TryCreate` instead could not be
  verified here, so #1992 **pins the `false` result with a test** and records the question
  rather than guessing. Recorded on #1992's notes, not as a separate ticket.

---

## 18. Namespace completion criteria

`System::Uri` is complete when:

1. every one of SR-AUD-138 … SR-AUD-151 is `remediated`, or `confirmed (design-complete)`
   with a blocked implementation ticket naming its approval sentence;
2. §16's six post-audit tickets are resolved or explicitly accepted;
3. `SharpRuntimeTests_Uri` covers §11's full vector matrix;
4. the module graph stays **41 / 91** unless an addition genuinely needs a new edge;
5. `docs/ComponentCatalog.md` regenerates unchanged.

After this batch: **7 of 11 causes closed** (U-A, U-B, U-C, U-D, U-E, U-F, U-K), **4
approval-gated** (U-G, U-H, U-I, U-J).

---

## 19. Status

| Cause | Tickets | Status after this batch |
|---|---|---|
| U-A | #1988 | see §20 |
| U-B | #1989 | see §21 |
| U-C | #1990 | see §23 |
| U-D | #1991 | see §22 (§22.1) |
| U-E | #1992 | see §22 (§22.2) |
| U-F | #1993 | see §24 |
| U-K | #1994 | see §25 |
| U-G | #1995 | **blocked**, design complete (§14.1) |
| U-H | #1996 | **blocked**, design complete (§14.2) |
| U-I | #1997, #1999 | **blocked**, designs complete (§14.3, §14.5) |
| U-J | #1998 | **blocked**, design complete (§14.4) |

---

## 20. What #1988 measured (2026-08-03) — cause U-A

### 20.1 The repair

`Uri::parse` now recognises the scheme in exactly one way. `findSchemeColon` — which has
always carried the RFC 3986 grammar in its own doc-comment — returns the colon index, and
the URI is hierarchical iff `"//"` immediately follows it. The `find("://")` search, the
`scheme_.empty()` throw and the per-character re-validation loop are all gone: they were the
second, wrong grammar, and the loop could not fire at all once the first grammar was the
only one used.

### 20.2 Before and after, from the same probe

`build-probe/1987_probe1_before.log` vs `build-probe/1988_probe1_after.log` — a diff of the
whole 100-line sweep, not a selected excerpt. **Six lines differ, and no others.**

| Input | Before | After |
|---|---|---|
| `/path?redirect=http://evil.com` | throws | relative, `AbsolutePath` = the whole reference |
| `search?url=https://example.com` | throws | relative |
| `mailto:a@b.com?body=see http://x` | throws | opaque, `Scheme=mailto`, `Path=a@b.com`, `Query=?body=see http://x` |
| `foo:bar://baz` | throws | opaque, `Scheme=foo`, `Path=bar://baz` |
| `"  http://example.com/  "` | throws | **relative** — see §20.4 |
| `UriBuilder("bad scheme").GetHashCode()` | throws | returns a hash — see §20.4 |

Everything else in the sweep — 25 absolute parses, the whole port boundary sweep, all six
combine shapes, every bracketed-IPv6 case, the escapes, the NUL, the `UriBuilder` sections
— is **byte-identical**, which is the empirical half of the §9.1 proof.

### 20.3 Sanitizers

| Harness | Result |
|---|---|
| Capability control (`1988_probe2_control*.log`) | **ASan heap-buffer-overflow reported**, **UBSan signed-overflow reported**, **LSan 64-byte leak reported** — all three prove the harness is not silently disabled |
| ASan + UBSan + LSan over the full sweep | **0 reports**, exit 0 |
| Instrumentation proof | 35 sanitizer symbols in the instrumented image vs **0** in the plain one; **44 `System::Uri::` symbols**, compiled from `modules/uri/src/System/Uri.cpp` on the command line — `build/libsharp_runtime_uri.a` was **not** linked |

Honest limitation, recorded rather than dressed up: `build/libsharp_runtime_core.a` supplies
the exception classes and is not instrumented, so UBSan says nothing about code inside
`UriFormatException`'s constructors.

### 20.4 Two consequences of the widening that are *not* improvements, stated plainly

1. **`"  http://example.com/  "` stops throwing and becomes a relative URI.** Before, a
   whitespace-padded absolute URI failed loudly; now it silently parses as a relative
   reference whose path is the padded text. .NET trims and produces the absolute URI, so
   *both* answers are wrong — but the new one is wrong **silently**. This is a direct
   consequence of the proof's shape (only previously-throwing inputs change) and it is not
   hidden: ticket **#2005**'s reproduction is updated to record the new behaviour, and it
   stays a deferred verification because whether .NET trims cannot be measured here.
2. **`UriBuilder::GetHashCode()` stops throwing for the `"bad scheme"` case.** `Uri("bad
   scheme://localhost/")` is now a relative URI rather than an error, so that particular
   route into §4.5's equality/hash asymmetry closes. **#2004 is not closed**: the asymmetry
   survives through any builder field that still produces an unparseable string — a `Host`
   of `"h:abc"` renders `http://h:abc/`, whose port is malformed, so `Equals` succeeds and
   `GetHashCode` still throws. #2004's reproduction is updated accordingly.

### 20.5 Consequences

No public signature, object layout, vtable, mangled symbol, `noexcept` specification or
component edge changed. `sizeof(System::Uri)` stays **240**. Eleven add-only regressions;
`SharpRuntimeTests_Uri` **149 → 160**.

---

## 21. What #1989 measured (2026-08-03) — cause U-B

### 21.1 The repair, at both sites

`defaultPortForScheme` is now consulted on **every** path that produces a port:

- the **opaque** branch replaces `port_ = -1` with `port_ = defaultPortForScheme(scheme_)`;
- the **empty-port** branch (`authority` ending in a bare `:`) gains the same call, which it
  never had.

### 21.2 Before and after

`build-probe/1988_probe1_after.log` vs `build-probe/1989_probe1_after.log`. **Four lines
differ, and no others.**

| Input | Before | After |
|---|---|---|
| `mailto:user@example.com` | `port=-1` | **`port=25`** |
| `telnet:host.example.com` | `port=-1` | **`port=23`** (the shape SR-AUD-143 does not name) |
| `mailto:a@b.com?body=see http://x` | `port=-1` | **`port=25`** |
| `http://example.com:/` | `port=-1` | **`port=80`** (the unnamed second site) |

`urn:isbn:…` stays `-1` — it has no table entry — and every explicit-port parse is
unchanged.

### 21.3 The consequence that had to be checked rather than assumed

Both `getAuthorityProperty()` and the two-`Uri` constructor render `:port` only when the
port **differs** from the scheme default. Setting the port *to* the default therefore
changes neither: `Uri("mailto:…").getAuthorityProperty()` is still `""` and
`Uri(Uri("http://example.com:/a/b/"), "c")` still produces
`http://example.com/a/b/c` with no `:80`. Both are asserted by permanent tests rather than
argued, because a repair that started injecting `:25` into every `mailto` authority would
be a rendering change this ticket does not authorise.

### 21.4 Sanitizers and consequences

ASan + UBSan + LSan over the full sweep: **0 reports**, exit 0, 35 sanitizer symbols vs 0,
44 `System::Uri::` symbols compiled from source. This is an honest **non-discriminator**:
the defect was a wrong *value*, not a memory error, so the harness could not have reported
anything either way — its value here is only that the repair introduces nothing.

No signature, layout, vtable, `noexcept`, mangled-symbol or component-edge change. No
acceptance change. Ten add-only regressions; `SharpRuntimeTests_Uri` **160 → 170**.

---

## 22. What #1991 and #1992 measured (2026-08-03) — causes U-D and U-E, together closing SR-AUD-145

SR-AUD-145 is one finding with two unrelated halves. They are repaired by two tickets and
landed in one commit because they edit the same two files in the same run; the halves are
kept separate everywhere else.

### 22.1 #1991 — the IP-literal authority (U-D)

`parse` now validates the bracket **structure** of an authority beginning with `[`, after
the user-info split and **before** the port split: a `]` must exist, the literal must not be
empty, and only a `:` may follow the `]`.

`build-probe/1989_probe1_after.log` vs `build-probe/1991_probe1_after.log` — **three lines
differ, and no others**:

| Input | Before | After |
|---|---|---|
| `http://[::1/path` | `host="[:"`, **`port=1`** | `UriFormatException` |
| `http://[::1]junk/path` | `host="[::1]junk"`, `port=80` | `UriFormatException` |
| `http://[]/path` | `host="[]"`, `port=80` | `UriFormatException` |

Every well-formed literal is untouched: `[::1]`, `[::1]:8080`,
`[2001:db8::8a2e:370:7334]:443`, `[::1]:` (which #1989 gives port 80), and the `IsLoopback`
path. The **fabricated port** is the half the finding does not name and the more dangerous
one — a caller that connects to `Port` reached port 1 rather than failing.

**Why this narrowing is landable without new approval:** the evidence is *inside this
repository*. `HttpClient::parseUrl` (`modules/net-http/src/System/Net/Http/HttpClient.cpp`)
already rejects an unterminated IPv6 literal with `UriFormatException`, pinned by
`HttpClientUrlParseTests.IPv6Literal_Unterminated_ThrowsUriFormatException`. Two parsers in
one repository disagreeing about the same malformed input is the defect; this makes them
agree. All five downstream test executables that consume `Uri` were re-run and are
unchanged (`Net` 240, `Net.Http` 132, `Net.Http.Headers` 373, `Xml` 379, `IO` 599).

An explicit exclusion is **pinned by a test rather than only asserted**: literal *content*
is not validated, so `http://[not-an-address]/p` still parses. A later ticket that starts
validating content will therefore be a deliberate decision.

### 22.2 #1992 — the `UriKind` domain (U-E)

`Uri(const std::string&, UriKind)` rejects a value outside the three declared members with
`System::ArgumentException`, `paramName` `"uriKind"`, message
`The value '{n}' is not valid for this usage of the type UriKind.` — the
`SR.Argument_InvalidEnumValue` spelling `Decimal::Round` and `Math::Round` already use. The
check runs **before** `parse`, so a malformed string still reports the argument error.

`build-probe/1991_probe1_after.log` vs `build-probe/1992_probe1_after.log` — **four lines
differ, and no others**:

| Call | Before | After |
|---|---|---|
| `Uri(absolute, (UriKind)99)` | accepted, `abs=1` | `ArgumentException … (Parameter 'uriKind')` |
| `Uri(relative, (UriKind)99)` | accepted, `abs=0` | `ArgumentException` |
| `Uri(absolute, (UriKind)-1)` | accepted, `abs=1` | `ArgumentException` |
| `TryCreate(absolute, (UriKind)99)` | `true`, non-null | **`false`, null** |

This reuses the policy the repository already approved for this cause (#1976 `GCSettings`,
#1954 `WaitHandle`) rather than creating a family. A repository-wide search for
`static_cast<UriKind>`, `static_cast<System::UriKind>` and `(UriKind)` over `modules/`,
`test/`, `tests/` and `bench/` returns **zero hits**, so the narrowing cannot reach a
consumer.

**One deferred verification, pinned rather than guessed.** `TryCreate` catches every
exception, so the constructor's `ArgumentException` becomes `false` + `nullptr`. Whether
.NET propagates instead cannot be measured here. The `false` result is therefore fixed by a
permanent test and disclosed in `Uri.hpp`'s `@note`, so a future change to it is a decision
rather than a drift.

### 22.3 Sanitizers and consequences

ASan + UBSan + LSan over the full sweep after each ticket: **0 reports**, exit 0, 35
sanitizer symbols vs 0, 44 `System::Uri::` symbols compiled from source. UBSan specifically
reports no invalid enum load from #1992's own out-of-domain casts.

No signature, layout, vtable, `noexcept`, mangled-symbol or component-edge change.
`sizeof(System::Uri)` stays **240**. Nineteen add-only regressions;
`SharpRuntimeTests_Uri` **170 → 189**. **SR-AUD-145 is fully remediated by the pair.**

---

## 23. What #1990 measured (2026-08-03) — cause U-C

### 23.1 The repair

`Uri(const Uri&, const std::string&)` now implements three of RFC 3986 §5.2.2's four
reference cases instead of one. The base-authority reconstruction became a lambda so all
three exits share it verbatim, and two branches were added **above** the existing path
merge:

1. a reference beginning `"//"` is a **network-path** reference: only the base's *scheme*
   survives, so the result is `scheme + ':' + reference`;
2. a reference whose path component is **empty** — one that is only a query and/or a
   fragment — keeps the base's path untouched, and when it is fragment-only it additionally
   keeps the base's query.

The path-bearing case is byte-for-byte the code that was already there.

### 23.2 Before and after

`build-probe/1992_probe1_after.log` vs `build-probe/1990_probe1_after.log` — **three lines
differ, and no others**. They match SR-AUD-144's recorded reference outputs exactly:

| Base | Reference | Before | After |
|---|---|---|---|
| `http://example.com/a/b?old#old` | `?new` | `http://example.com/a/?new` | **`http://example.com/a/b?new`** |
| `http://example.com/a/b?old#old` | `#new` | `http://example.com/a/#new` | **`http://example.com/a/b?old#new`** |
| `http://example.com/a/b` | `//other.example/c` | `http://example.com//other.example/c` | **`http://other.example/c`** |

The first two lost the `b` segment because an empty relative path ran through the merge,
which truncates the base path at its last `/`; the second additionally lost the base query.
The third produced a URI **pointing at the wrong host** — the base's — with the intended
host demoted to a path segment.

Unchanged and asserted: every path-bearing merge, the dot-segment cases, the absolute-path
reference, the opaque-absolute reference that discards the base, the userInfo-preservation
case, and the empty reference.

### 23.3 One case deliberately left alone

RFC 3986 §5.2.2 drops the base's **fragment** for an **empty** reference; this port has
always returned the base verbatim, fragment included. That is not repaired here: no
reference evidence survives in this environment to decide what .NET does with
`new Uri(base, "")`, and SR-AUD-144 does not name it. The current answer is **pinned by a
test** so a later change is deliberate.

The **opaque base** shape is also excluded and is ticket **#2001**:
`Uri(Uri("mailto:a@b.com"), "c")` still produces `mailto:///c`, because detecting that a
base was opaque needs information the object does not record.

### 23.4 Sanitizers and consequences

ASan + UBSan + LSan over the full sweep: **0 reports**, exit 0, 35 sanitizer symbols vs 0,
45 `System::Uri::` symbols compiled from source. All six downstream consumers re-run
unchanged: `Net` 240, `Net.Http` 132, `Net.Http.Headers` 373, `Xml` 379, `IO` 599,
`Net.WebSockets` 24.

No signature, layout, vtable, `noexcept`, mangled-symbol or component-edge change, and **no
acceptance change** — every resolution that succeeded still succeeds; three of them now
produce the correct result. Eleven add-only regressions; `SharpRuntimeTests_Uri`
**189 → 200**.

---

## 24. What #1993 measured (2026-08-03) — cause U-F

### 24.1 The repair

`UriBuilder::setFieldsFromUri` splits the copied user-info at its **first** colon into
`userName_` and `password_`, and clears the password when there is no colon. Both
constructors that copy from a `Uri` share that one function, so the string-taking and the
`Uri`-taking overload are repaired together; both are covered by tests.

### 24.2 Before and after

`build-probe/1993_probe2_before.log` vs `build-probe/1993_probe2_after.log` — **four lines
differ**:

| Input | Before | After |
|---|---|---|
| `http://user:pass@example.com/path` | `user='user:pass'`, `password=''` | **`user='user'`, `password='pass'`** |
| `http://u:p:q@example.com/p` | `user='u:p:q'`, `password=''` | **`user='u'`, `password='p:q'`** |
| the same builder after `Password="replacement"` | `http://user:pass:replacement@example.com:80/path` | **`http://user:replacement@example.com:80/path`** |
| `http://user:@example.com/p` | `user='user:'`, `ToString` `…user:@…` | **`user='user'`, `ToString` `…user@…`** — see §24.3 |

### 24.3 One rendering change, found by measurement and not assumed away

The ticket was written expecting `ToString()` to be byte-identical for every unmodified
builder. Measured, that is true for **three** of the four shapes and **false for one**: a
user-info of `"user:"` — a user name with an explicitly empty password — used to be stored
whole and emitted as `user:@`, and is now `UserName == "user"` with an empty `Password`,
emitted as `user@`. `ToString()` appends `":" + password` only when the password is
non-empty, which is also what .NET's `UriBuilder` does after the identical first-colon
split. The change is recorded here and **pinned by its own test** rather than left to be
discovered by a consumer.

### 24.4 Sanitizers and consequences

ASan + UBSan + LSan: **0 reports**, exit 0, 32 sanitizer symbols vs 0, 16
`System::UriBuilder::` symbols in the image. Header-only type, so instrumenting the probe
recompiles it.

No signature, object layout, vtable, `noexcept`, mangled-symbol or component-edge change;
`sizeof(System::UriBuilder)` stays **232**. No acceptance change. Eight add-only
regressions; `SharpRuntimeTests_Uri` **200 → 208**.

### 24.5 #2004's reproduction, updated by the same probe

§20.4 predicted that #1988 closed one route into the `Equals`/`GetHashCode` asymmetry while
leaving the defect alive. Measured, in the same probe:

```
host="h:abc" ToString        : http://h:abc/
host="h:abc" self-Equals     : 1
host="h:abc" GetHashCode     : THROWS Invalid URI: Invalid port specified.
scheme="bad scheme" self-Equals : 1
scheme="bad scheme" GetHashCode : -390761049      <- no longer throws
```

**#2004 stays open** with the `Host` route as its live reproduction.

---

## 25. What #1994 did (2026-08-03) — cause U-K

Documentation and pinning tests only. **No behaviour changed**, and the probe output is
byte-identical before and after.

### 25.1 `Uri::getSchemeProperty`

The doc-comment promised the scheme "lower-case as parsed". The parser has never
lower-cased anything. The **claim** was corrected, not the behaviour, because the behaviour
is SR-AUD-142 and its repair changes equality and hash semantics — approval-gated as #1995.
The new comment states the measured contract, names the finding, and points at the gated
ticket.

### 25.2 `UriCreationOptions`

The header disclosed that its flag is "not enforced in this stub". Measured, the situation
is stronger than that reads: **no `Uri` operation accepts the type at all** — there is no
`Uri(string, UriCreationOptions)` and no options-bearing `TryCreate`, so a value can be
constructed and read but can never reach a URI operation. A `@warning` now says so and
points at #1997 group A-3. This is SR-AUD-149's **disclosure half**; the finding itself
stays `confirmed`.

### 25.3 Why this is not "documentation theatre"

Five permanent tests make the corrected documentation **testable**: case-preserving scheme,
case-preserving host, the case-differing pair that is deliberately still unequal *and*
differently hashed, the consequence that a mixed-case scheme gets no default port, and the
empty-string rejection. The third of these is the important one — it **must be updated when
#1995 lands**, which turns an identity change from a silent one into a visible one.

`SharpRuntimeTests_Uri` **208 → 213**. No signature, layout, vtable, `noexcept`,
mangled-symbol or component-edge change. Closes neither SR-AUD-142 nor SR-AUD-149.

---

## 26. What #2000 measured and decided (2026-08-03) — the empty authority

*Post-audit ticket, no `SR-AUD-*` identifier. Numbering stays frozen at **364**.*

### 26.1 The complete reproduction matrix

`build-probe/2000_probe1_empty_authority.cpp`, output retained as
`build-probe/2000_probe1_before.log` (before) and `build-probe/2000_probe1_after.log`
(after). Every parse and construction path that can reach the authority branch with nothing
where the host belongs was exercised, across five scheme classes.

| Shape class | Before | After |
|---|---|---|
| `http://`, `http:///`, `http:///path`, `http://:`, `http://:/path`, `http://:80/path`, `http://?query`, `http://#fragment`, `http://?q#f` | accepted, `host=""`, `port=80`, `Authority=""` | **`UriFormatException`** |
| `https:///`, `ws:///`, `wss:///`, `ftp:///`, `gopher:///`, `nntp:///`, `telnet:///`, `ldap:///`, `net.tcp:///` | accepted with each scheme's default port | **`UriFormatException`** |
| `http://user@/path`, `http://user:pass@/path`, `http://@/path`, `http://user@:8080/path` | accepted, `host=""` (the last with `Authority=":8080"`) | **`UriFormatException`** |
| `file:///path`, `file:///tmp/bad.docx`, `file://`, `file:///`, `file://:/path` | accepted, `host=""`, **`port=-1`** | **unchanged** |
| `file://host/share` | accepted, `host="host"` | unchanged |
| `file://:8080/path` | accepted, `host=""`, `port=8080` | **`UriFormatException`** |
| `unknown://`, `unknown:///path`, `git+ssh:///repo`, `ssh:///host` | accepted, `host=""`, `port=-1` | **unchanged** |
| `mailto:a@b.com`, `mailto:`, `urn:isbn:…`, `telnet:host.example.com` (opaque branch) | accepted | **unchanged** |
| `mailto://`, `mailto:///c` (authority-bearing spelling of an opaque scheme) | accepted, `host=""`, `port=25` | **`UriFormatException`** |
| `http://@example.com/` (empty **user-info**, host present) | accepted | **unchanged** |
| `//`, `//other.example/c`, `/path`, `path`, `?q`, `#f` (relative) | relative | **unchanged** |
| `http://[]/path`, `http://[::1/path` | already rejected by #1991 | unchanged |
| `HTTP:///path` (mixed-case scheme) | accepted, `port=-1` | **unchanged — see §26.4** |

`TryCreate` turns the new rejection into `false` + `nullptr`; `UriBuilder::getUriProperty()`
with an empty `Host` now throws; `Uri(base, "//")` now throws because #1990 turns that
reference into `scheme + ":" + "//"`. Copy construction and assignment of a host-less `file:`
URI are unaffected.

### 26.2 The contract, stated once

> **A hierarchical (authority-bearing) URI may have an empty host only when no port applies
> to it — neither an explicit port in the authority nor a default port for the scheme.
> Otherwise the empty host is rejected with
> `UriFormatException("Invalid URI: The hostname could not be parsed.")`.**

It is deliberately **not** "an authority must be non-empty", which would reject
`file:///path`. It is also not a scheme allow-list: it is the same invariant #1991 restored —
*the object must never report a port for a host that does not exist* — and it happens to
partition the scheme classes correctly because `defaultPortForScheme` is exactly this file's
own record of which schemes are port-bearing network schemes.

### 26.3 The evidence, per clause

| Clause | Surviving evidence inside this repository |
|---|---|
| host-required schemes reject an empty host | `HttpClient::parseUrl` rejects `http://:8080/path` with `UriFormatException`, pinned by `HttpClientUrlParseTests.EmptyHostThrowsUriFormatException`. Two parsers in one repository disagreeing about the same malformed text is the defect; this makes them agree — the identical argument #1991 was landed on |
| `file:///path` stays accepted | `XmlUrlResolverTests.GetEntity_MissingFile_Throws` and `IOTests`' `FileFormatException` case both construct it today and pass; RFC 8089 makes it the canonical host-less form |
| unknown schemes stay accepted | no evidence either way survives, so nothing is narrowed there — the deferral discipline of #1998 and #1963 |
| the message spelling | a **consistency choice**, not a measured fact: it follows this file's existing `"Invalid URI: …"` family. Recorded as a choice rather than presented as reference-derived |

The `/rv/tmp/runtime/src/libraries/` tree and the audit's `/tmp/…` probe directories are still
absent; **re-verified on 2026-08-03**, not carried over.

### 26.4 Two consequences stated plainly rather than buried

1. **`HTTP:///path` is still accepted.** `defaultPortForScheme` matches lower-case names only,
   so a mixed-case scheme has no default port and therefore no contradiction to reject. That
   is a direct artefact of the still-open SR-AUD-142, it is **pinned by its own test**, and it
   closes when #1995 lands. Not tidied here, because tidying it would be #1995.
2. **`mailto:///c` — #2001's headline output — became a rejection.** #2000 was not written to
   repair #2001 and does not: see §28.

### 26.5 Consequences

No public signature, object layout, vtable, mangled symbol, `noexcept` specification or
component edge changed. `sizeof(System::Uri)` stays **240**. Accepted input **narrows**;
canonicalisation, equality and hash are untouched. All six downstream consumers re-run
unchanged: `Net` 240, `Net.Http` 132, `Net.Http.Headers` 373, `Net.WebSockets` 24, `Xml` 379,
`IO` 599. `SharpRuntimeTests_Uri` **213 → 235**.

---

## 27. What #2004 measured (2026-08-03) — the equality/hash asymmetry

### 27.1 A premise from the batch prompt, corrected

The work order described #2004 as a **`Uri`** equality/hash asymmetry. Measured, it is not:
`Uri::operator==` compares `absoluteUri_` and `Uri::GetHashCode` hashes the same member, so
they have always agreed. The asymmetry is **`UriBuilder`-only** and is between two operations
that read the same rendered text by different routes — `Equals` compares strings, `GetHashCode`
*parsed* them. `Uri`'s own pairing is asserted by a test in this ticket so the distinction
stays visible.

### 27.2 The repair and its compatibility proof

`GetHashCode()` now hashes `ToString()` directly instead of hashing
`getUriProperty().GetHashCode()`. This is **value-identical** wherever the old code returned
at all: `Uri::parse` assigns `absoluteUri_ = uriString` on every branch it accepts and
`Uri::GetHashCode` hashes exactly that member, so
`Uri(s).GetHashCode() == std::hash<std::string>{}(s)` for every `s` a `Uri` accepts. ∎

Measured, `build-probe/2000_probe1_after.log` vs `build-probe/2004_probe1_after.log`:
**six lines differ, and every one is a throw becoming a hash.** No previously returned value
moved — the empty-host builder still hashes to `632251956`, the same number it produced
before #2000 landed.

| Builder | Before | After |
|---|---|---|
| `Host = "h:abc"` | `Equals(self)=1`, `GetHashCode` **throws** *Invalid port specified.* | `208857098` |
| `Host = "h:99999"` | throws | `1715227682` |
| `Host = "[::1"` | throws (#1991) | `-1773657833` |
| `Host = ""` | throws (#2000) | `632251956` |

### 27.3 Separation from #1995, and what stays gated

The repair closes **only** the internal asymmetry. It introduces no case-folded scheme or host
identity, no default-port equivalence, no canonical identity and no `UriBuilder`→`Uri`
delegation. `DeliberatelyUnequalPairsStayUnequal_PinsTheGatedIdentityChange` fixes the three
pairs #1995 would make equal (case-differing host, case-differing scheme, explicit default
port) as **still unequal and still differently hashed**, so landing #1995 is a visible change.
SR-AUD-140 and SR-AUD-142 remain `confirmed (design-complete)`; #1995 remains **blocked**.

`Equals` and `GetHashCode` are now total on exactly the same set: the only remaining failure
is `ToString()` itself throwing for an empty `UserName` with a non-empty `Password`, and then
both raise it. Asserted.

### 27.4 Consequences

No signature, object layout, vtable, mangled-symbol, `noexcept` or component-edge change;
`sizeof(System::UriBuilder)` stays **232**. Header-only change, so every consumer recompiles.
`SharpRuntimeTests_Uri` **264 → 272** (this ticket's eight; the numbering reflects the commit
order, in which #2001/#2002 landed first).

---

## 28. What #2001 measured (2026-08-03) — resolution against an opaque base

### 28.1 A premise from §23.3, corrected

§23.3 wrote that the opaque-base shape was excluded "because detecting that a base was opaque
needs information the object does not record." **That is wrong, and measurement is what
showed it.** Opacity is exactly `parse()`'s own rule applied to the base's original string —
a scheme token whose colon is followed by `"//"` — and both operands are already members
(`absoluteUri_`, `scheme_`). `build-probe/2001_probe1_before.log` §C evaluates that predicate
over eight bases and it separates the three opaque ones from the three hierarchical ones
(including `file:///a` and `unknown:///a`, whose hosts are empty) with no false answers. The
repair therefore needs **no new member, no virtual and no layout change**;
`sizeof(System::Uri)` stays **240**. The historical §23.3 text is preserved above; this is the
correction.

### 28.2 The defect is broader than the ticket names

The ticket names `Uri(Uri("mailto:a@b.com"), "c")` → `mailto:///c`. Measured, the same root
cause — the reconstruction emitted `"scheme://"` unconditionally, feeding the base's **opaque
path** through `parse()`'s authority splitter — produces two strictly worse outcomes the
ticket does not name:

| Base | Reference | Before | Why it is worse |
|---|---|---|---|
| `mailto:a@b.com` | `?q` | `mailto://a@b.com?q` with **`Host == "b.com"`**, `Port == 25` | the mailbox local-part became user-info and the **domain became a host name** — a silent wrong component, not a malformed string |
| `urn:isbn:0-395-36341-1` | `?q` | **throws** *Invalid URI: Invalid port specified.* | `isbn:0-395-36341-1` read as `host:port` — a **false rejection** of well-formed input |

`parse()` classifies both bases as opaque with `host_ == ""`, so the resolver was
contradicting the parser about the very same base.

### 28.3 The contract selected

RFC 3986 §5.2.2/§5.3 with an **undefined base authority** — the same specification this
function already cites twice and the same evidence basis #1990 was landed on:

| Base | Reference | Before | After |
|---|---|---|---|
| `mailto:a@b.com` | `c` | `mailto:///c` (then `UriFormatException` after #2000) | **`mailto:c`** |
| `mailto:a@b.com` | `/c` | `mailto:///c` | **`mailto:/c`** |
| `mailto:a@b.com` | `?q` | `mailto://a@b.com?q`, host `b.com` | **`mailto:a@b.com?q`**, host `""` |
| `mailto:a@b.com` | `#f` | `mailto://a@b.com#f`, host `b.com` | **`mailto:a@b.com#f`** |
| `urn:isbn:0-395-36341-1` | `c` | `urn:///c` | **`urn:c`** |
| `urn:isbn:0-395-36341-1` | `?q` | throws | **`urn:isbn:0-395-36341-1?q`** |
| `news:`, `tag:`, `about:`, `javascript:`, `telnet:` opaque bases | `c` | `scheme:///c` or a throw | **`scheme:c`** |

**Rejecting the operation was considered and not chosen**: it is a narrowing with no surviving
reference evidence, where resolution per the RFC the file already names is neither.

Deliberately unchanged: a **network-path** reference (`//other/c`) still supplies its own
authority whatever the base was — `mailto://other/c` — which is what RFC 3986 §5.2.2 says; an
**absolute** reference still discards the base; an **empty** reference still returns the base
verbatim (§23.3's other deferral, untouched). Every hierarchical resolution is byte-identical:
`build-probe/2001_probe1_before.log` §B and §C are unchanged in the after log.

### 28.4 Consequences

No public signature, object layout, vtable, mangled symbol, `noexcept` or component-edge
change. **No acceptance narrowing** — two shapes that used to throw now resolve. Equality and
hash are untouched; every resolved value round-trips through `Uri(result.AbsoluteUri)` to an
equal object, asserted. `SharpRuntimeTests_Uri` **243 → 254**.

---

## 29. What #2002 measured (2026-08-03) — a relative reference's components

### 29.1 The defect and the evidence

The relative branch of `parse()` stored the whole reference as the path: `Uri("a?b#c")`
reported `AbsolutePath == "a?b#c"` with `Query` and `Fragment` empty; `Uri("?b")` reported
`AbsolutePath == "?b"`. RFC 3986 §4.2 spells
`relative-ref = relative-part [ "?" query ] [ "#" fragment ]` — the same three components an
absolute URI has — and **both other branches of the same function already split them**. Only
this one did not. That is the identical "one file, two answers" shape #1988 (two scheme
grammars) and #1989 (a port table consulted on some paths only) were landed on.

The impact was **measured, not assumed**: a repository-wide search for
`getAbsolutePathProperty`, `getQueryProperty()` and `getFragmentProperty()` outside
`modules/uri` returns nine call sites, in `WebProxy`, `CookieContainer`, `CredentialCache` and
`XmlUrlResolver`, and **not one of them can see a relative `Uri`** —
`XmlUrlResolver::GetEntity` explicitly branches on `IsAbsoluteUri` and uses `AbsoluteUri` for
the relative case. Relative resolution consumes the raw reference string, never these
components, so `Uri(base, relative)` is byte-identical (asserted).

### 29.2 Before and after

`build-probe/2002_probe1_before.log` vs `build-probe/2002_probe1_after.log`. Thirteen lines
differ, all in §A, and no others.

| Input | `AbsolutePath` before | after | `Query` | `Fragment` |
|---|---|---|---|---|
| `a?b` | `a?b` | `a` | `?b` | `` |
| `a#c` | `a#c` | `a` | `` | `#c` |
| `a?b#c` | `a?b#c` | `a` | `?b` | `#c` |
| `?b` | `?b` | `` | `?b` | `` |
| `#c` | `#c` | `` | `` | `#c` |
| `a?` / `a#` / `a?#` | verbatim | `a` | `?` / `` / `?` | `` / `#` / `#` |
| `/p/q?b#c` | verbatim | `/p/q` | `?b` | `#c` |
| `//host/p?b#c` | verbatim | `//host/p` | `?b` | `#c` |
| `a?b?c` / `a#c#d` / `a#c?b` | verbatim | `a` | `?b?c` / `` / `` | `` / `#c#d` / `#c?b` |
| `a%3Fb`, `a%23c` | verbatim | **unchanged** — the no-percent-decoding boundary holds | | |

`OriginalString`, `AbsoluteUri`, `ToString()`, `operator==` and `GetHashCode` all read
`absoluteUri_`, which is still the verbatim input, and are unchanged. `PathAndQuery` changes
only for a reference carrying a fragment, and only by dropping it — which is what
`PathAndQuery` already does for every absolute URI.

### 29.3 Three pre-existing assertions updated

`SchemeDetection_RelativeWithAbsoluteUrlInQuery_IsRelative`,
`SchemeDetection_RelativeWithAbsoluteUrlInFragment_IsRelative` (both #1988's) and this batch's
own `EmptyAuthority_RelativeReferencesAreUnaffected` asserted the old whole-reference path.
Each was updated to assert `OriginalString` **plus** the new split, with an inline note
recording what it said before. No assertion was deleted.

### 29.4 Consequences

No public signature, object layout, vtable, mangled symbol, `noexcept` or component-edge
change; `sizeof(System::Uri)` stays **240**. **No acceptance change** — nothing new is
rejected and nothing new is accepted. Three public properties change value for relative URIs
only. `SharpRuntimeTests_Uri` **254 → 272** (with §30's and §31's pins).

---

## 30. #2003 — the embedded NUL: pinned, not changed

`build-probe/2002_probe1_after.log` §E–§G is the complete inventory: constructors, every
component position, relative and opaque forms, leading/trailing/only-NUL, `UriBuilder`'s
`Scheme`/`Host`/`Path`/`Query`/`Fragment`/`UserName` setters, `ToString`, round trip and
equality.

**The measured behaviour is preserve-as-data, and it is internally consistent:**

- the NUL reaches `Host`, `AbsolutePath`, `Query`, `Fragment`, `UserInfo` and `AbsoluteUri`
  intact; the **port** position already rejects it, because a NUL is not a digit;
- there is **no prefix-only parse and no truncation**: `Uri("http://h/a\0b")` keeps all
  **12** bytes, `ToString().size() == 12`, it re-parses to an equal object, and it is **not**
  equal to `Uri("http://h/a")` and does not share its hash;
- `UriBuilder` renders it through without loss (`size == 32` for the multi-component case).

The ticket's four candidate contracts are *reject*, *preserve*, *escape*, *truncate*. Escape
is excluded by §15.1 (no percent-encoding). Truncate is the failure mode the measurement rules
out, and is not proposed. That leaves **reject** versus **preserve**, and choosing *reject*
would be a narrowing at a public boundary whose reference basis cannot be measured here —
exactly the line #1998 and #1963 sit on, and it is respected. **#2003 stays open, design-
complete, and its current behaviour is now pinned by three permanent tests** so a later change
is a decision rather than a drift.

---

## 31. #2005 — deferred verification, re-checked

**Re-verified on 2026-08-03, not carried over:** `/rv/tmp/runtime/src/libraries/` does not
exist in this environment (nor does `/rv`), and the audit's `/tmp/sharp-runtimervc-uri-*`
probe directories are still absent. **No authoritative reference evidence has become
available**, so #2005 stays deferred and is **not** converted into an implementation ticket.

The measurement did refine the reproduction a second time, and the refinement contradicts the
ticket's original one-line summary in **both** directions:

| Input | Measured now |
|---|---|
| `"  http://example.com/  "` | **relative**, path is the padded text (this is #1988's §20.4 consequence) |
| `" http://example.com/"` | **relative** |
| `"\thttp://h/"` | **relative** |
| `"http://example.com/ "` | **absolute**, `Host == "example.com"`, `AbsolutePath == "/ "` |
| `"http://exa mple.com/"` | absolute, `Host == "exa mple.com"` |
| `"http://h/a b"` | absolute, `AbsolutePath == "/a b"` |

So it is not "whitespace is rejected": **leading** whitespace silently demotes the reference to
relative, while **trailing** whitespace is accepted straight into the path. The exact missing
evidence is named: whether .NET trims surrounding whitespace before parsing, and whether it
rejects internal whitespace. Five permanent tests pin the current answers.

---

## 32. Further corrections this follow-up made to the record

Each preserves the historical text and is appended, per the standing rule. No `SR-AUD-*`
identifier was created; numbering stays frozen at **364**.

1. **§9.2's layout claim was untrue.** It said `sizeof(System::Uri) == 240` and
   `sizeof(System::UriBuilder) == 232` "are re-asserted by permanent tests, so a later ticket
   cannot grow either type unnoticed." Measured on 2026-08-03, **no test in the repository
   referenced `sizeof` at all** — the probe recorded the numbers, nothing pinned them. Two
   tests (`UriTests.Layout_SizeOfUriIsPinned`, `UriBuilderTest.Layout_SizeOfUriBuilderIsPinned`)
   were added by this batch, so the claim is now true.
2. **§23.3's opacity claim was wrong** — see §28.1.
3. **The batch prompt's description of #2004 as a `Uri` asymmetry was wrong** — see §27.1.
4. **#2001's, #2003's and #2005's recorded reproductions were incomplete or stale** — see
   §28.2, §30 and §31. All three ticket rows in `plan.sqlite3` carry an appended, separated
   correction note; none of the historical text was rewritten.
5. **§11's test matrix rows for "empty host, user-info, empty user-info", "embedded NUL" and
   "whitespace" now have owners**: #2000, #2003 and #2005 respectively, rather than "#1988
   pins current".

### 32.1 Status after this follow-up

| Ticket | Cause | Before | After |
|---|---|---|---|
| #2000 | post-audit, empty authority | `todo` | **done** (§26) |
| #2001 | post-audit, opaque base | `todo` | **done** (§28) |
| #2002 | post-audit, relative components | `todo` | **done** (§29) |
| #2004 | post-audit, identity asymmetry | `todo` | **done** (§27) |
| #2003 | post-audit, embedded NUL | `todo` | **blocked**, design complete, behaviour pinned (§30) |
| #2005 | post-audit, whitespace | `todo` | **todo**, deferred verification, behaviour pinned (§31) |
| #1995–#1999 | U-G/U-H/U-I/U-J | blocked | **blocked, unchanged — no approval was requested or assumed** |

§18's completion criteria: criterion 2 (§16's six post-audit tickets resolved or explicitly
accepted) is now met for four of six, with the remaining two design-complete and pinned.
Criterion 4 holds — the module graph is still **41 / 91** — and criterion 5 holds:
`docs/ComponentCatalog.md` regenerates unchanged.
