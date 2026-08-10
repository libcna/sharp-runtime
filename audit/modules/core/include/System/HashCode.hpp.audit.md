# Audit: `modules/core/include/System/HashCode.hpp`

## Metadata

- Audit status: AUDITED (156-line header-only implementation, fully read).
- Validation: `HashCodeTests.*` passed 25/25 in `SharpRuntimeTests_Core_Base`
  on 2026-07-25.  The name filter includes 11 tests in the dedicated
  `HashCodeTests.cpp` plus 14 duplicate-named tests in the still-unreviewed
  multi-surface `SystemTypesRemainingTests.cpp`.
- Sanitizer probe: `/tmp/sharp-runtimervc-hashcode-audit-probe.cpp`, built with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` on 2026-07-25.

## Assessment

The normal FNV-1a accumulator uses defined unsigned arithmetic, a process-wide
seed, and consistent `Combine`, vector, span, and custom-comparer behavior for
valid input.  Exact .NET hash values need not match because the header clearly
documents its FNV adaptation rather than the runtime's xxHash32 algorithm.

The `ReadOnlySpan<uint8_t>` overload trusts its signed length then casts it to
`size_t`.  The local public `ReadOnlySpan` constructor accepts a negative
length, so `HashCode::AddBytes(ReadOnlySpan<uint8_t>(oneByte, -1))` becomes an
effectively unbounded raw-pointer walk.  This is not merely an invalid caller
precondition hidden in the raw pointer extension: it is reachable through the
managed-shaped span overload and the documented .NET constructor rejects a
negative length.  The ASan probe confirms the resulting read past a one-byte
stack array.

References: [current .NET HashCode source](https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/HashCode.cs.html)
and [ReadOnlySpan constructor contract](https://learn.microsoft.com/en-us/dotnet/api/system.readonlyspan-1.-ctor?view=net-10.0).

## Finding references

### SR-AUD-043 — high — AddBytes converts a negative public span length into an unbounded raw read

`ReadOnlySpan<uint8_t>(oneByte, -1)` is accepted locally, then
`HashCode::AddBytes(const ReadOnlySpan<uint8_t>&)` casts `-1` to
`std::size_t` and forwards it to the unchecked pointer loop.  ASan reports a
stack-buffer-overflow in `HashCode::AddBytes(const uint8_t*, size_t)` on the
second byte.  The defect is reachable without constructing a raw pointer/size
pair and causes a process crash or disclosure-oriented out-of-bounds read.
`Span.hpp` is the now-confirmed upstream enabling cause and must be repaired
together, but every public byte-consuming span overload must independently
avoid signed-to-unsigned length escalation.

## Required post-audit verification

Reject negative lengths in `Span` and `ReadOnlySpan` construction with the
project's `ArgumentOutOfRangeException` convention, then have `AddBytes` check
its span length before converting to `size_t` as defense in depth.  Add ASan
tests for negative span length, default/empty span, a one-byte span, and the
raw-pointer overload's documented null/length preconditions.  Audit all
`getLengthProperty()` → unsigned conversions, not only HashCode.

## Other missing assertions and diagnostics

- No dedicated test gives `AddBytes` a negative, empty-default, or malformed
  span; all calls use vectors or valid positive raw ranges.
- The raw pointer overload has neither a non-null precondition nor a defensive
  diagnostic for `data == nullptr && length != 0`.
- `ToHashCode_DefaultIsNonZero` and several tests require unequal inputs to
  hash differently, although hash collisions and a zero final hash are valid;
  the associated test-contract pattern extends SR-AUD-018.
- The header should explicitly distinguish its non-.NET raw pointer/vector
  convenience overloads from the safe `ReadOnlySpan<byte>` counterpart.

## Final assessment

The valid-input accumulator behaves consistently, but the span path contains
an ASan-confirmed high-severity out-of-bounds read caused by unchecked signed
length conversion.  No implementation was modified during this audit.

### Partially remediated (SR-AUD-043a) — ticket #1852 (2026-07-30)

The out-of-bounds read is closed at its source rather than in `AddBytes`. Ticket
#1852 makes `ReadOnlySpan(const T*, intcs)` reject a negative length at
construction, so `ReadOnlySpan<uint8_t>(oneByte, -1)` — the exact input that drove
`AddBytes` past its buffer — can no longer be built. ASan confirmed the pre-fix
`heap-buffer-overflow READ of size 1` at `HashCode.hpp:76 in AddBytes`
(`build-probe/1852_span_hashcode_prefix.log`) and a clean construction-time throw
post-fix (`…_postfix.log`); a `HashCodeTests` case asserts the negative-length
span cannot be constructed.

`HashCode::AddBytes(const ReadOnlySpan<uint8_t>&)` (L92) is still `noexcept` and
still casts the span length straight to `size_t` — that defense-in-depth guard is
**SR-AUD-043b**, which cannot throw without dropping `noexcept`. It is tracked as
approval-gated ticket #1854 (`needs_user`) and stays open; the reachable exploit
is already closed by 043a. `docs/ConversionBoundaryFamilyPlan.md` §19.3.

### Fully remediated (SR-AUD-043b) — ticket #1854 (2026-07-31)

Approved by the batch instruction in the exact words of
`docs/RemainingApprovalDecisions.md` §A.10 (option A: drop `noexcept`, throw).
`HashCode::AddBytes(const ReadOnlySpan<uint8_t>&)` dropped `noexcept` and now
throws `ArgumentOutOfRangeException("value")` for a negative length; the three
`ReadOnlyMemory<T>` constructors dropped `noexcept` — and the pointer/length one
its `constexpr` — and now reject a negative length
(`ArgumentOutOfRangeException("length")`), an oversized `std::vector` (through
the shared `detail::checkedSpanLength`, the same guard 043a introduced) and a
negative-offset/count `ArraySegment`. The fields stay signed `intcs`; no
parameter list, return type, layout, vtable or mangled name changed, and an
Itanium mangled name does not encode `noexcept`, so there is **no exported-symbol
break**. +11 tests (`Batch3TypeTests.cpp`, `HashCodeTests.cpp`).

**Reachability, measured rather than assumed** (`build-probe/1854_prefix_plain.log`,
cases A21/A22/A27/A28): the `ReadOnlyMemory(const T*, intcs)` constructor **was**
directly reachable and stored `-1` and `INTCS_MIN` verbatim — that half was a
real, reachable defect, not defence in depth as §A.5 of the decision packet
described the ticket as a whole. The other two halves are genuinely unreachable
and are recorded as such: `ArraySegment` validates its own offset/count, so no
segment carrying a negative one can be handed to the segment constructor; and
since 043a no negative-length `ReadOnlySpan` can be constructed, so `AddBytes`'s
guard has no reachable caller. For those two, the permanent tests pin the
approved exception *specification* (`static_assert(!noexcept(…))`) plus the
unchanged valid behaviour, and say plainly that the throw itself cannot be
triggered through the public surface — an unreachable guard must not be reported
as a covered behaviour. `SR-AUD-043 → remediated` (043a #1852, 043b #1854).
`docs/ConversionBoundaryFamilyPlan.md` §19.3, §19.6.
