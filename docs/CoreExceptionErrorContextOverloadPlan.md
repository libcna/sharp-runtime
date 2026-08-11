<!-- SPDX-License-Identifier: MIT -->

# Core exception error-context overloads — SR-AUD-101 review and repair

Tickets: **#2277** (review), **#2278** (implementation). Finding: **SR-AUD-101**,
medium, `modules/core/include/System/IO/IOException.hpp` (owning report),
`DirectoryNotFoundException.hpp`, `CryptographicException.hpp`.
Date: 2026-08-11. No new `SR-AUD-*` identifier was created; the audit numbering
stays frozen at 364.

---

## 1. What the finding says

> The .NET `IOException(string?, int hresult)` constructor is absent from this
> published C++ declaration, so callers cannot preserve an OS/native error code
> while providing a diagnostic message. The related `DirectoryNotFoundException`
> port omits its public `(message, directoryPath, innerException)` overload, and
> `CryptographicException` omits its public composite-format/insertion overload.
> All three headers represent their types as implemented ports, yet their focused
> filter selects zero tests; missing overloads are therefore neither compiled nor
> behaviorally diagnosed.

The inherited ranking called this a small compatible singleton of **pure
additions**. This review treats that as a hypothesis and measures it.

## 2. Verdict

| Member | "Pure addition" accurate? | Compile domain | Runtime domain |
|---|---|---|---|
| `IOException(message, hresult)` | **no** | additive — no spelling stops compiling | **one already-compiling spelling changes meaning**: `IOException(message, 0)` |
| `DirectoryNotFoundException(message, directoryPath, inner)` | **yes** | additive | no existing construction changes |
| `CryptographicException(format, insert)` | **no, as first written** | would have **removed** `CryptographicException(message, nullptr)` | no existing construction changes |

The `CryptographicException` removal was measured and then designed out; the
`IOException` re-targeting is irreducible and is documented rather than hidden.
Section 6 states both precisely.

This is **one finding with three members that do not share a cause**. They share
a *shape* — "a .NET-shaped error-context constructor is missing" — and they were
filed as one finding. They are not a cross-cutting family: the causes are three
separate omissions in three unrelated types, each independently repairable, and
nothing repaired in one is required by another. **No CCF was minted.** This
follows the same standard as #2270 (a shared file is adjacency, not causation)
and #2267 (a shared parser is adjacency).

## 3. Premise corrections

### 3.1 The zero-test claim is executable-scoped, not repository-wide

The finding's validation line records that
`IOExceptionTests.*:DirectoryNotFoundExceptionTests.*:CryptographicExceptionTests.*`
"selected 0 tests". Measured on 2026-08-11 against **all 38 executables**, that
filter selects **15 tests**, not zero:

| Executable | Matching tests |
|---|---:|
| `SharpRuntimeTests_Core_Base` (the owning component) | **0** |
| `SharpRuntimeTests_IO` | 11 |
| `SharpRuntimeTests_Security_Cryptography` | 4 |

`IOExceptionTests` and `DirectoryNotFoundExceptionTests` live in
`modules/io/tests/System/IO/IOTests.cpp`; `CryptographicExceptionTests` lives in
`modules/security-cryptography/tests/System/Security/Cryptography/CryptographySupportTests.cpp`.
Both are the test binaries of **consumer** components, not of `Core.Base`, which
owns all three headers. So the accurate statement is: *the owning component's
test binary asserts nothing about these three types*, and the routes the finding
lists under "Other missing assertions" — the null C-string, the exact default
text, the identity of the stored inner exception, the retained path, the default
crypto HResult, UTF-8 text — really are unasserted anywhere. The repair adds
tests to the owning binary and does not retire the 15 existing ones.

### 3.2 `DirectoryNotFoundException`'s directory-path family has no verifiable .NET counterpart

The finding calls the missing overload "current .NET's public
`(message, directoryPath, innerException)` overload". **This could not be
verified**: `/rv` is absent in this environment, so the .NET reference source
named in the report's "Reference basis" line is unavailable, and this batch is
forbidden from inspecting anything outside this repository. The port's *existing*
`(message, directoryPath)` constructor and `getDirectoryPathProperty()` are
themselves not part of any .NET API this repository can point at — .NET carries a
path-shaped constructor on `FileNotFoundException` (`fileName`), not on
`DirectoryNotFoundException`.

The addition is therefore justified **without** that premise, on two grounds that
are checkable inside this repository:

1. the user-visible gap the finding states is real and reproducible — before this
   change there was no way to construct a `DirectoryNotFoundException` carrying
   both the failed path and its cause; and
2. the port already provides exactly that shape twice, on
   `System::IO::FileNotFoundException` (`(message, fileName, inner)`) and
   `System::IO::FileLoadException`, so the addition completes an existing
   in-repository convention rather than inventing one.

If the .NET reference later becomes available and shows no such overload, nothing
here needs revisiting: the constructor completes this port's own family, and the
doc-comment says so rather than claiming .NET parity.

### 3.3 `IOException(string?, int)` and `CryptographicException(string, string?)` are recorded as .NET-shaped

These two are documented in their headers as C++ counterparts of the .NET
constructors the finding names. The same `/rv` limitation applies, so the
doc-comments state the .NET shape the finding recorded and the behaviour this
port implements; no behaviour was chosen by guessing at unverifiable reference
text. Both bodies are trivially derivable from the finding's own description:
assign the caller's HResult, and compose the message through the port's single
composite-format engine.

---

## 4. Measured construction domain, before and after

`build-probe/2277_probe1_before.cpp` prints, for each of the three types, a
`std::is_constructible_v` row over thirteen argument packs plus the observable
state of every construction the types support. The identical source was compiled
against the tree before and after the repair. Columns: `S` = `std::string`,
`C` = `const char*`, `E` = `std::exception_ptr`, `N` = `std::nullptr_t`,
`I` = `SharpRuntime::intcs`.

| Cell | IOException before → after | DirectoryNotFound before → after | Cryptographic before → after |
|---|---|---|---|
| `()` | yes → yes | yes → yes | yes → yes |
| `(C)` | yes → yes | yes → yes | yes → yes |
| `(S)` | yes → yes | yes → yes | yes → yes |
| `(I)` | no → no | no → no | yes → yes |
| `(S,E)` | yes → yes | yes → yes | yes → yes |
| `(S,N)` | yes → yes | no → no | yes → **yes** (see §5) |
| `(S,I)` | no → **yes** | no → no | no → no |
| `(S,long)` | no → **yes** | no → no | no → no |
| `(S,S)` | no → no | yes → yes | no → **yes** |
| `(S,C)` | no → no | yes → yes | no → **yes** |
| `(C,C)` | no → no | yes → yes | no → **yes** |
| `(S,S,E)` | no → no | no → **yes** | no → no |
| `(S,C,E)` | no → no | no → **yes** | no → no |
| `(S,S,N)` | no → no | no → **yes** | no → no |

**No cell anywhere flips `yes` → `no`.** Nine cells flip `no` → `yes`; those are
the additions.

Observable state, same probe, before → after: every row is byte-identical except
one.

```
IOException("m", 0)   before: hresult=80131620 inner=no  message="m"
IOException("m", 0)   after : hresult=00000000 inner=no  message="m"
```

`sizeof` (168 / 200 / 168), `alignof` (8 / 8 / 8) and `is_polymorphic_v` (all
true) are unchanged for all three types.

---

## 5. The one spelling that had to be designed back in

`build-probe/2277_probe2_shapes.cpp` models the before and after overload sets as
isolated mock classes, so the after-state can be measured without touching the
tree. It found that adding `(const std::string&, const std::string&)` to
`CryptographicException` **removes two spellings that compile today**:

```
Crypto/before   (S,N)=yes (C,N)=yes
Crypto/naive    (S,N)=no  (C,N)=no
```

The mechanism, confirmed by the compiler diagnostic in §7 mutation 5: in C++23
`std::basic_string` declares `basic_string(std::nullptr_t) = delete`. A deleted
function is still a candidate, so `nullptr` reaches `std::string` and
`std::exception_ptr` through **equal-rank user-defined conversions** and the call
is ambiguous. `DirectoryNotFoundException` and `FileNotFoundException` already
carry both shapes and therefore already reject `(message, nullptr)` — measured
above as `(S,N) = no` for `DirectoryNotFound` before *and* after.

That would have been a public source break. `build-probe/2277_probe3_disambiguator.cpp`
measured the fix: an explicit `CryptographicException(const std::string&, std::nullptr_t)`
overload restores both cells to `yes` **with their existing meaning**, disturbs no
other cell, and is an exact match for `nullptr` so it can never be ambiguous:

```
Crypto/proposed (S,N)=yes (C,N)=yes (S,S)=yes (S,C)=yes (C,C)=yes (C,S)=yes
[select] before   ("m", nullptr) -> inner
[select] proposed ("m", nullptr) -> inner
```

It delegates to `(message, std::exception_ptr{})`, which is what `nullptr`
produced before, so the resulting object is identical — pinned by
`NullptrOverload_MatchesTheEmptyExceptionPtrOverloadExactly`.

**This overload is deliberately not added to `DirectoryNotFoundException`,
`FileNotFoundException` or `FileLoadException`.** Its justification is preserving
a spelling that compiles today; on those three types the spelling never compiled,
so adding it there would be a new widening outside this finding, not a
preservation. The asymmetry is intentional and is recorded in the doc-comment.

### 5.1 The `IOException` re-targeting is irreducible

`IOException(message, 0)` compiles before and after and means something different
after. There is no way to avoid it while implementing .NET's `(string?, int)`
shape: the literal `0` has type `int`, indistinguishable at the type level from
any other `int`, and it is simultaneously a null pointer constant, so it reaches
`std::exception_ptr` today by a user-defined conversion and reaches `intcs` by an
exact match afterwards. `NULL`, which GCC and Clang spell as an integer null
pointer constant, behaves the same way. `nullptr` is **not** affected — it does
not convert to an integer — and is pinned by a test.

The consequence is bounded: the message is unchanged and the inner exception is
absent in both states, so the sole difference is the HResult, `0x80131620` → `0`.
Spelling `0` to mean "no inner exception" is degenerate — `IOException(message)`
produces exactly the old object — and **no first-party call site does it**. All
2-argument `IOException` construction sites in this repository pass a real
`std::exception_ptr`:
`NetworkStream.cpp:188/211`, `ZLibException.hpp:35`, `PathTooLongException.hpp:22`,
`FileLoadException.hpp:28/33`, `DriveNotFoundException.cpp:24`,
`FileNotFoundException.cpp:37/43`, `EndOfStreamException.cpp:28`,
`HttpIOException.hpp:63`, `DirectoryNotFoundException.cpp:19`. The behaviour is
stated in the constructor's own doc-comment and pinned by
`LiteralZeroSelectsTheHResultOverload`.

---

## 6. The repair

| File | Change |
|---|---|
| `modules/core/include/System/IO/IOException.hpp` | declares `IOException(const std::string&, SharpRuntime::intcs)`; adds the direct `SharpRuntime/SharpRuntimeHelper.hpp` include the new signature needs |
| `modules/core/src/System/IO/IOException.cpp` | body: assigns the caller's code unconditionally |
| `modules/core/include/System/IO/DirectoryNotFoundException.hpp` | declares `(const std::string&, const std::string&, std::exception_ptr)` |
| `modules/core/src/System/IO/DirectoryNotFoundException.cpp` | body: forwards the inner exception to `IOException`, stores the path, assigns `COR_E_DIRECTORYNOTFOUND` |
| `modules/core/include/System/Security/Cryptography/CryptographicException.hpp` | declares `(format, insert)` out of line; adds the inline `(message, std::nullptr_t)` disambiguator |
| `modules/core/src/System/Security/Cryptography/CryptographicException.cpp` | **new file**: composes the message with `System::String::Format` |

`CryptographicException(format, insert)` is defined out of line specifically so
that `System/String.hpp` stays out of the public header's include set; every other
constructor of that type remains inline. The substitution deliberately reuses
`System::String::Format`, which since #1884 runs the single shared
`System::detail::runCompositeFormat` scanner — writing a bespoke `{0}` replacement
here would have created another composite-format grammar, which is precisely the
defect class CCF-012 and #2020 exist to stop.

Consequently a malformed `format` throws `System::FormatException` from the
constructor. That is the same outcome .NET's `string.Format`-based constructor
has, it is confined to a brand-new overload, and it is pinned by
`FormatCtor_RejectsAMalformedFormatWithFormatException`.

### 6.1 ABI, layout and symbols

No member was added, removed or reordered; `sizeof`, `alignof` and polymorphism
are unchanged and asserted by the probe. No existing signature, return type,
`noexcept` specification or default argument changed. No virtual function was
added, removed or reordered, so no vtable changed. The change is **additive at
the symbol level**: three new constructor symbols
(`IOException(string,int)`, `DirectoryNotFoundException(string,string,exception_ptr)`,
`CryptographicException(string,string)`) appear in `libsharp_runtime_core.a`, the
`nullptr_t` overload is inline, and no previously exported symbol was removed or
changed. No component boundary, `PUBLIC_DEPENDENCIES`, `PRIVATE_DEPENDENCIES` or
`TEST_DEPENDENCIES` declaration changed: `System/String.hpp` and
`CryptographicException.hpp` are both owned by `Core.Base`, and the module graph
stays at 41 modules / 92 edges.

---

## 7. Evidence

**Sanitizers are not discriminating for this defect class and were not run.** The
defect is a *missing* public overload: the compile-time construction domain and
the composed message text. There is no memory-safety, lifetime or arithmetic
component to observe — the added bodies perform one assignment, one string copy
and one call into an already-covered formatter. Running ASan/UBSan here would have
produced a line for the report and no information, which is the practice #2274
explicitly rejected.

**Compile-domain evidence** is the before/after `is_constructible_v` matrix of §4
plus the mock-shape matrices of §5, all from source compiled by the same
toolchain.

**Mutation testing**, five mutations, all caught. Every mutation was rebuilt
before its tests were run; two first-attempt mutations were rejected as invalid
because `-Werror=unused-parameter` stopped the build, which would have left a
stale binary reporting a false pass — they were rewritten to consume the
parameter and re-run.

| # | Mutation | Caught by |
|---|---|---|
| 1 | `IOException` HResult ctor stores `hresult + 1` | 3 tests: `HResultCtor_ReplacesCorEIoWithTheCallerCode`, `HResultCtor_AcceptsTheWholeIntegerRangeIncludingZero`, `LiteralZeroSelectsTheHResultOverload` |
| 2 | `DirectoryNotFoundException` 3-arg ctor stores an empty path | 3 tests: `PathAndInnerCtor_RetainsBothThePathAndTheCause`, `PathAndInnerCtor_AcceptsANullptrCause`, `Utf8MessageAndPathArePreservedByteForByte` |
| 3 | `DirectoryNotFoundException` 3-arg ctor drops the inner exception | 1 test: `PathAndInnerCtor_RetainsBothThePathAndTheCause` |
| 4 | `CryptographicException(format, insert)` ignores the insert | 7 tests, the whole `FormatCtor_*` group |
| 5 | the `std::nullptr_t` disambiguator is deleted | **compile time**: `error: call of overloaded 'CryptographicException(std::string, std::nullptr_t)' is ambiguous`, with both candidates named |

Mutation 5 is a **compile-domain** mutation caught **at compile time**, which is
the correct kind of evidence for a compile-domain claim: §5's assertion is exactly
that removing the overload makes an existing spelling ill-formed, and the compiler
says so with the two candidates the analysis predicted.

---

## 8. Tests

33 tests added to `SharpRuntimeTests_Core_Base`, the binary that owns the three
headers and previously asserted nothing about them:

- `modules/core/tests/System/IOExceptionTests.cpp` — 12
- `modules/core/tests/System/DirectoryNotFoundExceptionTests.cpp` — 8
- `modules/core/tests/System/CryptographicExceptionTests.cpp` — 13

The 15 pre-existing tests in `SharpRuntimeTests_IO` and
`SharpRuntimeTests_Security_Cryptography` are untouched, so the finding's focused
filter now selects **48** tests across three executables instead of 15 across two.
Suite names are deliberately reused so that the filter the audit recorded selects
tests in the owning binary; GoogleTest suite names are per-executable, and the
test names within each suite are distinct from the existing ones.

Beyond the new overloads, the tests close the specific routes the finding lists as
unasserted: the null C-string on `IOException` and `DirectoryNotFoundException`,
`IOException`'s exact default text, the *identity* of a stored inner exception
(rethrown and matched, not merely non-null) on all three types,
`CryptographicException`'s inherited `COR_E_SYSTEM` default, and UTF-8 message,
path and insert text preserved byte for byte.

---

## 9. Disposition

SR-AUD-101 → **remediated**. All three named omissions are implemented, the
zero-coverage half is closed in the owning binary, and the two non-additive
consequences are measured, bounded and documented rather than absorbed silently.

No residual ticket is opened. Nothing in this unit needs a user decision: the one
irreducible consequence (§5.1) affects a degenerate spelling with no first-party
consumer and no behaviour that was previously correct-and-useful, and the one
avoidable source break (§5) was avoided.
