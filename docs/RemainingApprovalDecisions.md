<!-- SPDX-License-Identifier: MIT -->

# Remaining approval decisions — authoritative packet (2026-08-01)

## 1. Authority, scope, and how to use this packet

This file supersedes its earlier six-group packet. Groups A, D, E, and F are
retained as the exact historical authority for the independently approved and
delivered #1932 Option 2R contract, the coordinated #1934 then bounded #1925
contract, the independently approved #1926 `wontfix` disposition, and the
independently approved #1936 Option 1 contract; none is an open choice. Every
still-pending contract remains independent. The evidence authority is:

- docs/NetworkExceptionHResultPropagationDesign.md for #1932;
- docs/OwnedTreeLifetimeContractPlan.md §§42, 45, and 46 for #1894/#1899;
- docs/DateTimeValidationBoundaryPlan.md §§20–22 and
  docs/TextSubsetCompatibilityDecision.md for #1929;
- docs/CompositeFloatingKeyPolicyDesign.md for #1925/#1934; and
- docs/CollectionsComparisonContractPlan.md §20 for #1926;
- docs/ImmutableSortedSetFloatingEqualityDesign.md for #1936; and
- docs/NullableFloatingHashSetPerformanceEvidence.md for #1937.

The existing Groups B/C .NET comparisons use the official dotnet/runtime
snapshot 0eb5481340ea675857c7a7abf18f68a60b52a686. #1936 separately uses the
linked current `main` source observed on 2026-08-01; the local environment has
no dotnet/Mono executable. No item below is implemented merely because .NET
differs. No item grants authority to inspect a downstream consumer. CNA and
mobile-eggbert were not inspected.

There is deliberately no “approve all fixes” choice:

| Group | Tickets | Contract | Recommendation | Can be grouped? |
|---|---|---|---|---|
| **A (delivered)** | #1932 | two networking exceptions' causal HResult rule | Option 2R approved and implemented | closed independently |
| **B** | #1899, then #1894 | Xml.Linq borrowed-view policy and diagnostic closure | retain #1898 contract; close #1899 wontfix; do not claim a visitor prevents escape | #1894 depends on the chosen #1899 classification |
| **C1–C4** | #1929 rows 1–4 | four independent date/time acceptance/API choices | retain and document rows 1–3; design row 4 separately | decide by row, not as one grammar switch |
| **D (delivered)** | #1934 then #1925 | direct nullable-floating default comparison and key policy | bounded direct-optional group approved and implemented | closed in dependency order |
| **E (closed)** | #1926 | libstdc++ long-double hash caching | `wontfix` approved and recorded | closed independently |
| **F (delivered)** | #1936 | generic ImmutableSortedSet comparator-equivalence equality | Option 1 approved and implemented | closed independently |
| **G (closed)** | #1937 | nullable-floating HashSet performance isolation | not reproducible; no optimization | evidence-only closure |

#1933 is not an approval item: its performance isolation is complete and no
production optimization was justified. #1937 is likewise not an approval
item. #1888, #1889, and #1896 remain declined and are not re-proposed. #1773
remains blocked on work outside this repository.

---

## 2. Delivered Group A — #1932 network-exception inner HResult

### 2.1 Current port and current .NET

The port stores one HResult in System::Exception. It preserves the
std::exception_ptr and all networking metadata, but it never inspects the
inner object while constructing these two outer types.

| Constructor shape | Current sharp-runtime outer HResult | Current .NET outer HResult |
|---|---:|---:|
| HttpRequestException default or message-only (H1/H2) | 0x80131500 | 0x80131500 |
| HRE message + null inner (H3) | 0x80131500 | 0x80131500 |
| HRE message + non-null inner (H3) | 0x80131500 | exact inner HResult |
| HRE message + inner + HttpStatusCode (H4) | 0x80131500 | exact inner HResult; status has no precedence |
| HRE HttpRequestError + message + inner + optional status (H5) | 0x80131500 | exact inner HResult; error/status have no precedence |
| WebException default or message-only (W1/W2) | 0x80131509 | 0x80131509 |
| WebException message + null inner (W3) | 0x80131509 | 0x80131509 |
| WebException message + non-null inner (W3) | 0x80131509 | exact inner HResult |
| WebException message + status, no inner (W4) | 0x80131509 | 0x80131509 |
| WebException message + inner + status (W5) | 0x80131509 | exact inner HResult; status has no precedence |

.NET's condition is only “inner is non-null.” It copies zero, a base
Exception value, a type-specific value, a custom value, and a nested
network-exception value exactly. HttpRequestError, HttpStatusCode, and
WebExceptionStatus are orthogonal metadata. The two types intentionally share
the conditional rule but retain different no-inner base values.

C++ adds one case .NET cannot express: an exception_ptr can hold a
std::runtime_error rather than a System::Exception. Recommended Option 2R keeps
the outer base value for that case.

Concrete examples:

| Construction | Current outer | Option 2R / .NET |
|---|---:|---:|
| HRE("m", FormatException) | 0x80131500 | 0x80131537 |
| HRE(ConnectionError, "m", custom 0x81234567, 503) | 0x80131500 | 0x81234567 |
| WebException("m", zero-HResult System exception, Timeout) | 0x80131509 | 0x00000000 |
| WebException("m", std::runtime_error, Timeout) | 0x80131509 | 0x80131509, port-defined control |

Copy, move, and exception_ptr rethrow preserve whatever outer value was stored
at original construction. There are no serialization or clone paths in the
port. Current built-in HTTP producers supply no inner exception, so their
results remain unchanged. Synchronous and asynchronous HTTP forwarding retain
the same exception object rather than wrapping it.

### 2.2 Options and recommendation

| Option | Exact rule | Compatibility and consequence | Verdict |
|---|---|---|---|
| **1** | always retain the outer base/type HResult | no change; permanent divergence on H3/H4/H5/W3/W5 | reject |
| **2R** | copy exact HResult only when a non-null pointer rethrows System::Exception; otherwise keep the outer base | observable constructor result changes only; matches .NET on the represented surface | **recommend** |
| **3** | copy selected network/platform values | invents an unstable classifier and rejects valid zero/custom/type-specific .NET cases | reject |
| **4** | add a separate native/network field | new state/API/layout and redundant metadata | reject |
| **5** | preserve and document divergence | same behavior as option 1 | reject unless compatibility risk is judged controlling |
| **6** | add a constructor or factory | new public surface while old constructors remain divergent | reject |

Option 2R changes no declaration, overload, default argument, return or
parameter type, mangled name, symbol set, base, field, offset, vtable, virtual
slot, sizeof/alignof, noexcept, or constexpr state. Measured sizes remain HRE
176/8 and WebException 168/8. It does change an observable outer HResult for
direct causal construction. Because the constructor bodies are inline, all
consumers must rebuild to avoid mixed old/new semantics.

The only added cost is one exception_ptr rethrow/catch during causal
construction. No current request hot path supplies an inner pointer. Rollback
is a two-header/test revert; there is no persisted-state migration.

Permanent tests must cover every H1–H5 and W1–W5 row, null, default,
type-specific, custom, zero, non-System, nested HRE/WebException, explicit
status/error precedence, exact message and inner identity, copy/move,
exception_ptr, and sync/async forwarding. #1932 is atomic; no base-Exception
helper, unrelated networking type, producer wrapping, new field, or public API
belongs in the implementation.

### 2.3 Copyable approval wording

> Approve #1932 Option 2R only: for every existing
> HttpRequestException and WebException constructor that accepts
> std::exception_ptr, if the non-null pointer rethrows a System::Exception,
> copy that inner exception's HResult exactly, including zero; if the pointer
> is null or contains a non-System exception, retain the current outer base
> HResult. HttpRequestError, HttpStatusCode, and WebExceptionStatus do not
> override this rule. Do not add constructors, fields, accessors, or producer
> wrapping, and do not change public declarations, ABI, layout, vtables,
> symbols, noexcept, or constexpr. Preserve messages, inner identity, and all
> error/status fields, and add the complete direct/copy/move/exception-pointer
> and sync/async transport matrix from
> docs/NetworkExceptionHResultPropagationDesign.md.

No-change alternative:

> Decline #1932 and permanently document that HttpRequestException retains
> 0x80131500 and WebException retains 0x80131509 even when a System inner
> exception has another HResult. Keep the retained divergence matrix.

### 2.4 Delivery record (2026-08-01)

The user approved Option 2R verbatim and no other packet group. Ticket #1932 is
done. H3/H4/H5/W3/W5 now copy an exact System inner HResult, including zero;
null and non-System pointers retain their family base, and status/error
metadata stays orthogonal. Thirteen permanent tests cover the complete direct,
copy/move/assignment, exception-pointer, and synchronous/asynchronous transport
matrix. Full implementation, sanitizer, layout, and symbol evidence is in
`docs/NetworkExceptionHResultPropagationDesign.md` §11. Groups B through E
remain undecided and unchanged by this closure.

---

## 3. Group B — #1899 borrowed views and dependent #1894

### 3.1 Actual public surface and reference behavior

The port has four header-template ancestor overloads, not two:

1. Ancestors(range);
2. Ancestors(range, XName);
3. AncestorsAndSelf(range); and
4. AncestorsAndSelf(range, XName).

They materialize vector<XElement*>. XElement::getAttributesProperty() returns a
const reference to the element's attribute vector. Ancestors can therefore
leave a raw pointer after the tree dies (probe X15), and the attribute-vector
reference can outlive its element (X17). Both produce retained ASan
use-after-free reports when the documented lifetime precondition is violated.
XElement::Attributes() is already an owning by-value alternative for X17.

.NET's four corresponding extension overloads return managed
IEnumerable<XElement> sequences, and XElement.Attributes() returns managed
XAttribute references. A retained managed reference keeps its object alive.
The port also permits automatic-storage XObject instances, so it cannot
manufacture a shared_ptr ownership handle for every ancestor: such objects have
no control block.

#1898 already states and permanently tests the ordinary C++ borrowed contract.
The focused Xml.Linq sanitizer suite was clean for 184/184 supported operations;
X15/X17 intentionally exercise use after the stated precondition and remain
outside that supported matrix.

### 3.2 Correct meanings of options D and G

The earlier design has two corrections:

- D must mirror all four range/name-filtered overloads. A single-node,
  unfiltered ForEachAncestor is a counterpart to none of them.
- A visitor cannot enforce a borrow lifetime in C++23. Any callback given an
  XElement pointer, reference, or equivalent observer can save it and use it
  after the call. A non-copyable facade does not prevent saving its address or
  an observer obtained through it.

The authoritative meanings are now:

| Option | Exact meaning | X15/X17 effect | #1894 effect |
|---|---|---|---|
| **D** | add four range-equivalent visitor conveniences, preserving all existing overloads | safer idiom for adopters, but callback escape remains; X17 already has Attributes() | no outlawed or diagnosed old spelling; #1894 remains blocked |
| **G** | D plus deprecate the four borrowed ancestor overloads and getAttributesProperty() | five compile-time warnings; ordinary calls still compile; visitor escape remains | a checker may turn the five warnings into errors, but that is diagnostic coverage, not ordinary ill-formedness |

Therefore D+G is a migration aid, not a memory-safety proof. It does not satisfy
#1899's original acceptance statement that X15/X17 become unreachable, and it
must not be reported as doing so.

### 3.3 Full option set and recommendation

| Option | Source/API | ABI/layout/symbol | Runtime/performance | Recommendation |
|---|---|---|---|---|
| **A / #1898** retain and pin the borrowed contract | none | none | none | **already done; retain** |
| **B** return getAttributesProperty() by value | address-of-current-result stops compiling | silent calling-convention break under the unchanged Itanium symbol; layout unchanged | vector copy plus shared_ptr increments per call | reject outside a breaking release |
| **C** return owning ancestor handles but omit automatic-storage ancestors | hard type change | template/source change | incomplete result | disqualified on correctness |
| **D** four additive visitor overloads | additive | no existing symbol/layout change | callback per element; avoids result vector for adopters | optional ergonomics only |
| **E** B + D + removal of borrowed APIs | hard source break | B's silent accessor break; templates are source-only | migration plus B cost | reject; requires coordinated breaking release |
| **F** off-by-default debug registry | macro/configuration surface | must be cpp-local to avoid layout/ODR change | zero when off; instrumentation when on | detects only; optional separate tooling |
| **G** D + five deprecations | no rejected call, but new warnings and in-repo migration | none | none beyond D | optional migration diagnostics only |

Option B is especially poor: getAttributesProperty() is one of eighteen public
borrowed const-reference accessors, yet only it would silently change return
calling convention while keeping the same mangled name. This is the same
mixed-object hazard class for which #1889 was declined.

**Recommendation:** retain #1898 and close #1899 wontfix under the current C++
ownership model. Record that exact .NET lifetime parity is unavailable without
a much broader ownership/source contract. Close #1894's negative-fixture half
as not applicable while preserving its already-complete sanitizer evidence.
D+G may instead be approved as a separate migration aid, but not under a claim
that it makes borrowed observers non-escapable.

### 3.4 #1894 consequences and exact tests

Current repository totals are 10 negative fixtures / 74 sites and 2 version
seams / 18 sites. D creates no negative site. If G is chosen, exactly one new
Xml.Linq fixture can pin five deprecated spellings by compiling each with
-Werror=deprecated-declarations:

- Ancestors(range);
- Ancestors(range, name);
- AncestorsAndSelf(range);
- AncestorsAndSelf(range, name); and
- getAttributesProperty().

That would make the checker total 11 fixtures / 79 sites if no other fixture
changes first. It would not make those calls ill-formed for ordinary consumers.
No Text.Json negative site should be invented: #1888 was declined. The
sanitizer half remains the existing supported-operation matrix; sanitizers can
detect the retained misuse when deliberately run, but cannot prove an observer
never escapes.

For D, permanent tests cover all four overloads, empty/multi-node ranges,
null/detached inputs, order, self-first ordering, and filters. A runtime test
may show the intended scoped idiom, but no compile-time test may claim pointer
escape is impossible. For G, permanent tests pin exact deprecation diagnostics
and migrate all in-repository uses. Rollback removes the four additions and/or
attributes; no stored data or layout migrates.

### 3.5 Copyable decision wording

Recommended no-change classification:

> Retain #1898's documented and tested ordinary C++ borrowed-view contract.
> Close #1899 as wontfix under the current ownership model: automatic-storage
> Xml.Linq ancestors have no shared ownership handle, and a C++ visitor callback
> can retain any pointer or reference it receives, so options D and G cannot
> make X15 structurally unreachable. Do not implement B or E and do not reopen
> declined #1889. Record #1894's permanent sanitizer half as complete and close
> its negative-fixture half as not applicable because no approved repair
> outlaws a spelling. Preserve the retained X15/X17 misuse probes and do not
> claim .NET lifetime parity.

Alternative migration-aid approval:

> Approve #1899 options D plus G as migration aids only: add four additive,
> range-taking visitor overloads matching Ancestors and AncestorsAndSelf with
> and without XName, then deprecate those four existing borrowed-vector
> overloads and XElement::getAttributesProperty(). Preserve every existing
> signature and symbol, migrate in-repository callers, and make no layout,
> vtable, ABI, noexcept, or constexpr change. I understand that ordinary
> downstream calls continue to compile with warnings and that a visitor
> callback can retain its observer, so this does not make X15/X17 impossible.
> For #1894, add one Xml.Linq diagnostic fixture with exactly five sites using
> -Werror=deprecated-declarations and describe it as diagnostic coverage, not
> as proof that the APIs are ordinarily ill-formed. Do not implement B, E, F,
> or any #1888/#1889 change.

B must use its own breaking-release wording and is not recommended:

> In a separately coordinated ABI-breaking release only, design the migration
> for returning XElement::getAttributesProperty() by value, acknowledging the
> unchanged Itanium mangled name, incompatible return calling convention,
> vector-copy cost, and consistency decision for the other seventeen borrowed
> const-reference accessors. This wording does not approve implementation.

---

## 4. Groups C1–C4 — #1929 remaining date/time rows

Rows 5–6 are complete. DateTime and TimeOnly accept one through seven
fractional digits at exact 100-nanosecond resolution; all five date/time
families accept approved surrounding invariant whitespace. #1931's TimeOnly
representation correction is complete. The remaining rows are:

| Row | Current sharp-runtime | Current .NET | Recommended decision |
|---|---|---|---|
| **1** unpadded month/day | rejects 2024-6-15 and 2024-06-5 | accepts | retain/document subset |
| **2** more than seven fractional digits | rejects an eighth digit | reads the whole digit run and rounds the fraction to 100-ns ticks | retain/document seven-digit exact boundary |
| **3** short/compact offset | rejects +2, +2:5, +0205 | accepts; +2:5 means +125 minutes | retain/document subset; do not silently reverse #1879 |
| **4** exact/provider/kind surface | ParseExact, TryParseExact, provider overloads, DateTimeKind absent | present | separate API design; no implementation approval |

For row 2, current .NET's ParseFraction consumes every ASCII digit, accumulates
a double fraction, then applies Math.Round(fraction * 10,000,000). For example,
a .12345678 fraction is accepted and contributes 1,234,568 ticks; the current
port rejects it. An implementation would need to match current .NET's exact
rounding and boundary behavior rather than merely truncate after seven digits.

### 4.1 Public surface and compatibility

- Row 1 affects DateTime, DateTimeOffset, and DateOnly general Parse/TryParse
  grammar.
- Row 2 affects the general time-bearing parsers, including DateTime,
  DateTimeOffset, and TimeOnly.
- Row 3 affects offset-bearing DateTime/DateTimeOffset general parsing.
- Row 4 adds public overload families and a kind/provider/style contract across
  date/time types.

Rows 1–3 need no declaration or representation change, but they expand
accepted input. Callers using parse failure as validation would silently accept
new text. Row 3 is additionally a policy reversal: #1879 explicitly changed
+2:5 from accepted to rejected after an earlier premise wrongly called its
125-minute interpretation a bad answer. Each row therefore needs its own
semantic approval and commit.

Row 4 is additive at source/symbol level but requires exact overloads,
format/provider semantics, DateTimeKind representation, size/alignment/field
offset analysis, mangled/undefined symbol evidence, and interaction with
offset application. It is design-first and cannot be bundled with grammar
widening.

No remaining row is motivated by #1933 performance. Rollback for rows 1–3 is
the parser/test commit; row 4 would need its own API rollback/migration plan.
Tests for any widening must pin Parse/TryParse agreement, exact ticks/offset,
minimum/maximum and rollover, malformed and trailing text, all currently
accepted forms, exception type/message, and unchanged output on TryParse
failure. Row 2 additionally needs 8, 9, long digit runs, below/at/above half-
tick boundaries, carry into the next second, and out-of-range controls.

### 4.2 Copyable decisions

Recommended retained-subset decision for rows 1–3:

> For #1929 rows 1–3, retain and document sharp-runtime's invariant subset:
> month and day remain two digits, fractions remain limited to one through
> seven exact 100-nanosecond digits, and offsets remain the currently approved
> fixed-width forms. Keep unpadded dates, an eighth fractional digit, and
> +2/+2:5/+0205 rejected by both Parse and TryParse. This deliberately differs
> from current .NET and preserves #1879's accepted-input boundary. Do not alter
> public declarations, representation, or the completed rows 5–6 behavior.

Separate row-1 widening alternative:

> Approve #1929 row 1 only: make DateTime, DateTimeOffset, and DateOnly general
> Parse/TryParse accept one- or two-digit month/day fields such as 2024-6-15,
> while preserving exact values and every current rejection outside that row.
> This is an accepted-input widening with no declaration or layout change.

Separate row-2 widening alternative:

> Approve #1929 row 2 only: make the general time-bearing Parse/TryParse paths
> accept more than seven fractional digits and match the pinned current .NET
> conversion to 100-nanosecond ticks, including rounding and second carry.
> Preserve one-through-seven-digit exact ticks and all unrelated rejections.
> This is an accepted-input/value rule change with no declaration or layout
> change.

Separate row-3 reversal alternative:

> Approve #1929 row 3 only: re-accept current .NET's short and compact offsets
> +2, +2:5, and +0205, with +2:5 interpreted as +125 minutes. I understand that
> this reverses the accepted-input decision made in #1879. Preserve every
> other offset boundary and make no declaration or layout change.

Row-4 design wording:

> Authorize a design-only ticket for #1929 row 4 to inventory exact
> ParseExact/TryParseExact, provider/style, and DateTimeKind surface and
> semantics, including representation and ABI alternatives. Do not implement
> any new overload or DateTimeKind representation without a later explicit
> approval.

---

## 5. Delivered Group D — #1925 plus #1934 direct nullable-floating policy

### 5.1 Corrected current behavior and .NET mapping

The original #1925 premise wrongly grouped optional, pair, tuple, and arbitrary
composites as one recursive policy. Current measurements are:

| Shape | Hashed collection | Ordered collection |
|---|---|---|
| optional<double> | compiles; copied NaN inserts twice and neither key is findable | NaN, 1, and 2 collapse to one node |
| nested optional<double> | same defect | same collapse |
| pair/tuple/array | default Dictionary is ill-formed because libstdc++ has no invocable hash | distinct values collapse around NaN |
| variant<double,int> | compiles with unfindable NaN | values collapse |
| arbitrary user type | follows its own operators/hash | follows its own operators |

The generic defaults are separately inconsistent (#1934):

| Surface for optional<double> | NaN equality | equal hashes | Compare NaN vs 1 |
|---|---:|---:|---:|
| generic Comparer/EqualityComparer default | false | false | 0 |
| dedicated Nullable comparers | true | true | -1 |
| current .NET nullable default | true | true | -1 |

.NET Nullable delegates two present values to the underlying default comparer,
equality, and hash; null is ordered first. ValueTuple delegates per field.
.NET does not reflect over arbitrary user fields. Raw lifted nullable equality
still treats NaN according to the primitive operator, so raw optional equality
must not be changed.

### 5.2 Recommended bounded option and affected surface

Recommended Option B covers only direct std::optional<F> where F is float,
double, or long double:

1. decide presence first;
2. null equals null, hashes to zero, and orders before present;
3. two present values use the existing .NET-compatible floating comparer,
   equality, and canonical NaN/signed-zero hash;
4. generic Comparer/EqualityComparer and the three DefaultKey policies agree;
5. dedicated Nullable comparers remain controls; and
6. raw optional/Nullable operators and explicit caller comparers remain
   unchanged.

#1934 must land first for helper/default-comparer semantics, then bounded #1925
for key selection. The affected key policies flow through sixteen collection
headers: Dictionary, HashSet, frozen/read-only/immutable/concurrent/ordered and
sorted variants. Public MapType/SetType aliases and deduced iterator return
types move for the newly affected direct-optional family.

Measured candidate sizes were unchanged for raw standard containers
(unordered_map 56/8 and iterator 8/8; set 48/8 and iterator 8/8), but comparator
template arguments make them different C++ types and change inline symbols.
Equal size is not ABI compatibility. A coordinated consumer rebuild is
required. Public function declarations and comparer virtual slots can remain;
every affected outer collection, iterator/proxy, defined/undefined symbol,
sizeof/alignof, field offset where observable, noexcept, and constexpr property
must nevertheless be re-measured.

The runtime cost is one presence branch plus the existing floating policy for
present operands. Benchmarks must cover null, finite, NaN, signed zero,
insert/lookup, ordinary controls, and explicit comparers. Migration matches
#1919 for a narrower additional instantiation family. Rollback restores the
helper/selectors and the deliberate divergence test; persisted container data
is not serialized by this change.

Permanent tests cover all direct optional floating types; null; finite extrema;
signed zero; infinities; multiple NaN payloads; duplicate, lookup, removal,
growth/rebuild and projections across all affected collection families;
ordered exact NaN/1/2 count/order; generic/dedicated comparer agreement; raw
operator controls; explicit-comparer authority; and negative controls for
optional<int>, nested optional, pair, tuple, array, variant, vector, and user
types. Pair/tuple Dictionary compile failures remain retained evidence unless a
future ticket explicitly adds hash capability.

### 5.3 Alternatives

| Option | Consequence | Verdict |
|---|---|---|
| preserve current behavior | no compatibility change; unfindable/collapsed keys and generic-default split remain | available no-change choice |
| direct optional floating (#1934 + bounded #1925) | fixes one exact .NET Nullable mapping with known source/ABI migration | **recommend** |
| recurse into pair/tuple | must separately decide mapping, new hash capability, combination, arity, and public types | design later |
| recurse through all standard composites | no single .NET mapping | reject |
| recurse through user fields | unavailable in C++23 and contrary to .NET type-defined equality | reject |
| public opt-in trait/factory | new API and ODR contract | separate design |

### 5.4 Copyable approval wording

> Approve the coordinated direct nullable-floating default-comparison change
> for tickets #1925 and #1934 only. For std::optional<float>,
> std::optional<double>, and std::optional<long double>, make
> Comparer<T>.Default, EqualityComparer<T>.Default, and
> DefaultKeyLess/DefaultKeyHash/DefaultKeyEqual use null-first ordering and the
> existing underlying floating .NET-compatible comparison, equality, and
> canonical NaN/signed-zero hash rules. I understand that this changes the
> comparator-bearing backing standard-container type, public MapType/SetType
> aliases, and affected iterator/deduced return types for that additional key
> family, requiring coordinated consumer rebuilds, even where measured
> size/alignment remains unchanged. Preserve raw nullable operator==, all
> public function declarations, vtable slots, object layout, noexcept, and
> constexpr state. Do not extend this approval to nested optional, pair, tuple,
> array, variant, vector, arbitrary user-defined types, new hash support, or
> ticket #1926.

No-change alternative:

> Do not implement tickets #1925 or #1934. Retain and document the current
> nullable/composite default-comparison divergences and compile limitations.

### 5.5 Delivery record (2026-08-01)

The user approved §5.4's bounded direct-nullable-floating contract and its
source/template consequences. Ticket #1934 landed first, followed only after
its focused gates by #1925. Direct `optional<float>`, `optional<double>`, and
`optional<long double>` now use null-first ordering, null hash zero, NaN-
reflexive equality/canonical hashing, signed-zero equality/canonical hashing,
and the existing finite/infinity policy through generic and dedicated
comparers and all sixteen inventoried collection consumers. Raw nullable
operators and explicit caller comparers remain unchanged.

The design premise that every affected iterator moves is corrected by measured
libstdc++ 14 evidence: nullable-float and nullable-double hash iterators and all
ordered iterators retain their spellings; nullable-long-double hash iterators
move because the selected hasher changes private hash-code caching. All 48
outer object size/alignment measurements are unchanged, but backing aliases
and 4,285 predicate-bearing/template symbols move, so coordinated clean rebuild
remains mandatory. Pair/tuple hashing remains ill-formed and no excluded
composite gained capability. Complete closure evidence is in
`docs/CompositeFloatingKeyPolicyDesign.md` §§11–12.

---

## 6. Closed Group E — #1926 libstdc++ long-double cache policy

#1926 has no direct .NET reference behavior: System.Double maps to C++ double,
while C++ long double and libstdc++'s node-cache trait are implementation-
specific. The controlled three-hasher probe established:

| Shape | std::hash<long double> before #1919 | DefaultHash<long double> today | candidate reserved-trait specialization |
|---|---|---|---|
| libstdc++ fast-hash trait | false | true | false |
| hash cached in node | yes | no | yes |
| node size on measured x86-64 | 64 | 48 | 64 |
| iterator cache template argument | true | false | true |
| insert median relative to before | 1.000 | 1.319, slower 24/25 rounds | 0.895 median; mechanism restored |

The old claim that today's node “loses a word” is corrected: it loses 16 bytes
because long double is 16-byte aligned, so today's node is 25% smaller. The old
0.791 lookup improvement is also withdrawn; 25 rounds reversed it to 1.210 and
the series was inside a 3.0–6.3 max/min noise floor.

The proposed implementation specializes reserved libstdc++ internal
std::__is_fast_hash behind __GLIBCXX__. The C++ standard does not designate
that internal template for user specialization. libc++ and MSVC STL have
neither this mechanism nor this fix.

There is no public declaration, field, vtable, noexcept, or constexpr change,
but the standard-container node and iterator type return to their pre-#1919
forms on libstdc++. That is a public template/ABI consequence and grows every
affected node 48 to 64 bytes. Performance improves only long-double hashed
insertion on this standard library. Rollback removes the specialization.
Approval would require 25+ alternating post-change rounds, node/iterator/symbol
evidence, all floating correctness tests, and other-toolchain compile controls.

**Decision delivered: close `wontfix`.** The user accepted that a stable 1.319
insertion cost on a rare, toolchain-specific key is not enough to justify
undefined/nonportable use of a reserved standard-library internal plus 16
bytes per node. CCF-010's corrected NaN equality and hashing remain mandatory.
This decision is separate from #1925/#1934 and grants them no authority.

Copyable recommended wording:

> Decline specializing std::__is_fast_hash or any other reserved libstdc++
> internal for #1926. Close #1926 as wontfix, retain today's 48-byte measured
> node and iterator type, and preserve the 25-round evidence that long-double
> hashed insertion is 1.319 times the pre-#1919 shape in 24 of 25 rounds.
> Withdraw the noisy lookup claim. Do not change correctness behavior or
> ticket #1919.

Alternative implementation wording:

> Approve #1926 only on libstdc++ by specializing
> std::__is_fast_hash<System::detail::DefaultHash<long double>> to false behind
> __GLIBCXX__. I accept the reserved-internal portability risk, the measured
> node growth from 48 to 64 bytes, and the iterator/type-symbol transition back
> to the pre-#1919 shape in exchange for recovering the 1.319-times insertion
> regression. Require at least 25 alternating post-change rounds, full
> representation/symbol evidence, and all correctness gates. Do not change
> float/double policy or other standard libraries.

---

## 7. Delivered Group F — #1936 ImmutableSortedSet comparator equivalence

The complete direct `float`, `double`, and `long double` matrix reproduces
NaN-nonreflexive `SetEquals`, and both equal-set proper predicates incorrectly
return true. The three direct nullable-floating controls are correct under the
bounded #1925 branch. A case-insensitive string set reproduces the same
contradiction, proving a generic algorithm defect rather than a missing
floating alias.

The user approved Option 1 exactly: retain rebuilding `other` under this set's
comparer and the post-collapse count check, then compare the ordered ranges
using `!less(a,b) && !less(b,a)` for every `T`. A shared-backing-data true fast
path is included and implemented. This matches current .NET's comparer-based
scan and the port's already-correct `SortedSet` algorithm. It changes only one
inline method body, no declaration, alias, iterator, object layout, or vtable.
Emitted template body code/helper symbols can move, and custom-comparer as well
as direct-floating observable results intentionally change. The approved
semantic and template effects are fully measured in the owning design.

Copyable approval wording:

> Approve ticket #1936 Option 1 exactly: change only
> `ImmutableSortedSet<T>::SetEquals` so that, after rebuilding `other` under
> this set's existing ordering comparer and checking the post-collapse count,
> it compares the two ordered ranges by the comparator-equivalence relation
> `!less(a,b) && !less(b,a)` for every `T`; a shared-backing-data true fast path
> is also approved. This intentionally fixes direct `float`, `double`, and
> `long double` NaN reflexivity and the same generic custom-comparer defect,
> and consequently makes equal sets not proper subsets or supersets. Preserve
> this-comparer precedence, all other set-operation results, raw floating and
> optional operators, public declarations and aliases, iterator types, object
> layout, and vtables. Add the complete direct/nullable/custom-comparer matrix,
> mutation proof, performance comparison, and source/symbol/layout evidence.
> Do not change #1934/#1925 policy selection or any other collection.

This wording was approved and delivered. Selected equality, delegation, mutual
lookup, documented divergence, and direct-floating-only specialization are
rejected for the reasons in the owning design.

---

## 8. Closed Group G — #1937 performance isolation

The retained 2.092x historical line came from one warm-up and two separate
seven-row campaigns, not 25 alternating pairs, and recorded no bucket history.
The corrected O2/O3 campaign retains 360 warm-up rows and 1,800 measured rows
(900 pairs). The exact-work nullable-double finite rehash-hit paired median is
1.026 at O2 and 0.964 at O3, with wide p05/p95 ranges crossing 1 and 12/13 and
14/11 wins/losses. Direct-double and optional-int controls show the same host
noise. Finite hashes, accepted sets, buckets, loads, and collisions match.
NaN/mixed comparisons are not like-for-like because the old policy cannot
find its own NaNs; null hashes intentionally differ.

Disposition: `done`, not reproducible on the tested supported GCC/libstdc++
toolchain and host. No optimization or approval request is proposed. Reopen
only under the exact stability, identical-work, control, and preservation
conditions in the owning evidence document.

---

## 9. Dependency order, declined items, and closure boundaries

The only remaining dependency chains are:

1. #1899 classification or migration choice, then any accurately re-scoped
   #1894 diagnostic fixture;
2. each #1929 row independently, with row 3 explicitly acknowledging #1879.

#1934 then #1925 was delivered in its approved order. #1926 was standalone and
is now closed. Delivered #1932 and #1936 no longer participate in the
dependency order. #1929 row 4 is design-first. No choice depends on #1933,
and no performance ticket authorizes a semantic widening.

| Ticket | State preserved by this packet |
|---|---|
| #1773 | blocked; external/downstream work prohibited |
| #1888 | declined; not re-proposed |
| #1889 | declined; not re-proposed |
| #1894 | blocked pending an honest classification of what a #1899 choice can prove |
| #1896 | declined; not re-proposed |
| #1899 | blocked pending Group B decision |
| #1925 | done; bounded direct optional subset delivered after #1934 |
| #1926 | `wontfix`; accepted independently, evidence retained |
| #1929 | todo/partial; rows 1–4 unchanged |
| #1932 | done; exact Option 2R delivered independently |
| #1933 | done; optimization designed but not implemented |
| #1934 | done; direct nullable-floating defaults delivered first |
| #1936 | done; exact generic Option 1 delivered with full evidence |
| #1937 | done; not reproducible, evidence-only, no optimization |

No new SR-AUD identifier is issued. Audit numbering remains frozen at 364.
