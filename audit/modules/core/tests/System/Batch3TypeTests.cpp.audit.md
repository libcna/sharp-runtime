# Audit: `modules/core/tests/System/Batch3TypeTests.cpp`

## Metadata

- Audit status: AUDITED (242 lines, fully read; mixed MidpointRounding,
  UInt128, MarshalByRefObject, EventHandler, ReadOnlyMemory, Activator, and
  attribute smoke tests).
- Relevant validation: `ReadOnlyMemoryTests.*` passed 7/7 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

The seven duplicate ReadOnlyMemory tests are shallow smoke coverage alongside
the dedicated Buffers suite: they check default/vector construction, positive
indexing, one ordinary slice, `ToArray`, and Empty.  They omit the constructor,
copy, pin, equality, raw-boundary, and extreme-slice cases that define the
memory-view contract.  No additional finding beyond the dedicated
ReadOnlyMemory audit is independently demonstrated here.

The two `MarshalByRefObjectNewTests` preserve a separate public-contract drift:
`DefaultCtor_DoesNotThrow` directly creates a base that current .NET declares
abstract, while the test never attempts its obsolete throwing remoting members.
See SR-AUD-128.

## Finding references

- **SR-AUD-043:** no raw pointer/length construction or malformed-length
  consumer is exercised.
- **SR-AUD-049:** only `Slice(1, 3)` is tested; one-argument negative/extreme
  start validation is absent.
- **SR-AUD-128:** direct base construction is asserted as success although the
  current .NET type is abstract and retains observable unsupported remoting
  members.

## Required post-audit verification

Keep one focused ReadOnlyMemory smoke path here only if its role as a
Core.Base consumer is intentional.  Put exhaustive boundary, copy, ownership,
and sanitizer coverage in the dedicated Buffers suite to avoid maintaining two
partially overlapping contracts.

## Other missing assertions and diagnostics

- The tests do not check a negative index, invalid slice, raw pointer length,
  or all-empty behavior beyond `Length`/`IsEmpty`.
- No test states the non-owning lifetime/reallocation rule.
- The file combines unrelated type suites, so a failure gives little immediate
  subsystem diagnostic context.

## Final assessment

The ReadOnlyMemory subset is harmless smoke coverage but insufficient to catch
the confirmed boundary defect.  No test was modified during this audit.
