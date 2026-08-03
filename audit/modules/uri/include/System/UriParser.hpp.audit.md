# Audit: `modules/uri/include/System/UriParser.hpp`

## Metadata

- AUDITED: 93-line inline declaration/implementation, fully read.
- Validation: `UriParserTest.*` passed 14/14 on 2026-07-27.
- Reference/probe: local current `UriScheme.cs`/`UriSyntax.cs` and functional
  tests, with C++/C# probes under `/tmp/sharp-runtimervc-uri-parser-audit-*`.

## SR-AUD-146 — medium — UriParser cannot register or participate in custom scheme parsing and exposes protected hooks as public stubs

Current .NET provides public `UriParser.Register` and protected virtual parser
hooks (`GetComponents`, `IsBaseOf`, `InitializeAndValidate`, `Resolve`,
`OnNewUri`, and `OnRegister`) that take part in URI construction and component
retrieval. C++ has no `Register` member — the standalone probe fails to compile
with `Register is not a member of System::UriParser` — and exposes a smaller
subset of hooks as public calls that return fixed values or throw
NotImplementedException. Thus a custom parser can neither be registered nor
used by `Uri`; the advertised extensibility point is inert.

## SR-AUD-147 — medium — IsKnownScheme accepts malformed and empty scheme text instead of validating the public argument

The method lowercases every `std::string` then returns false. C++ probe output
is `empty=0` and `space=0` for `""` and `"ht tp"`; current .NET's public
method throws ArgumentOutOfRangeException for both invalid scheme strings.
The C++ type cannot express null, but it can and should distinguish malformed
input from a valid unknown scheme.

## Other missing assertions and diagnostics

- Tests enumerate ordinary built-ins but omit empty/whitespace/invalid scheme
  strings and all custom registration/error paths.
- The direct test deliberately calls public `GetComponents` on a subclass and
  asserts its stub throw; this locks the wrong access/participation shape rather
  than testing a functional protected override.
- Thread safety, duplicate registration, default-port bounds, and registered
  custom-scheme Uri construction have no representable C++ coverage.

## Final assessment

The builtin lookup table is accurate for ordinary names, but public validation
and the main custom-parser contract are absent. No source or test was modified.

---

## Post-audit review note — ticket #1987 (2026-08-03)

The historical text above is preserved verbatim.

SR-AUD-146 and SR-AUD-147 are both confirmed by re-measurement:
`UriParser::Register` is absent (compile-time), and `IsKnownScheme("")` and
`IsKnownScheme("ht tp")` both return `false`
(`build-probe/1987_probe1_before.log` §O).

SR-AUD-147's repair is nonetheless **blocked** as ticket **#1998** rather than landed with
this review's compatible half. It is a **narrowing** at a public static, and the reference
basis this report cites — the C#/C++ probes under
`/tmp/sharp-runtimervc-uri-parser-audit-*` — **no longer exists in this environment**, as
`/rv/tmp/runtime/src/libraries/` also does not. That is the same line ticket #1963 sits on
and it is respected: `docs/SystemUriNamespaceReviewPlan.md` §14.4 carries the design and the
exact approval sentence. SR-AUD-146 is blocked as ticket **#1997** group A-4, whose cost
includes a `UriParser` vtable and access-level change.
