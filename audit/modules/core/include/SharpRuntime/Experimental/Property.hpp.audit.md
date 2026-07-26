# Audit: `modules/core/include/SharpRuntime/Experimental/Property.hpp`

## Metadata

- AUDITED: 124-line experimental property-wrapper/macro header, fully read.
- Validation: `ExperimentalPropertyTests.*` passed 4/4 on 2026-07-27.
- Reproduction: a standalone C++20 property probe prints
  `stored=new assignment_result=`; an auto-property macro probe fails because
  `SharpRuntime::Property` does not exist.
- Reference basis: the header's own declared complete-but-unused contract and
  its direct integration fixture; this is a SharpRuntime helper, not a .NET
  parity type.

## SR-AUD-179 — medium — Property assignment writes through the setter but returns a disconnected stale cache value

`operator=(const T&)` invokes the supplied setter then returns `cachedValue` by
`T&`.  That member is never synchronized with the getter/setter and is merely
default-initialized.  With a string-backed property, the standalone probe
assigns `"new"`: the captured backing storage becomes `stored=new`, but the
assignment expression copies `assignment_result=` from the empty cache.

The API is marked experimental and has no first-party production use, which
limits reachability, but the header calls its getter/setter delegation fully
implemented.  A public C++ assignment expression must not expose an unrelated
default/stale object; it should return the property or a value that reflects
the operation rather than an unsynchronized cache.

## SR-AUD-181 — low — advertised DEF_PROP_AUTO and custom property macros name a nonexistent Property type

The macros expand to `SharpRuntime::Property<type>`, while the class is
`SharpRuntime::Experimental::Property<T>`.  A minimal class using documented
`DEF_PROP_AUTO(int, Value, 0)` and `IMPL_PROP_AUTO` fails to compile at the
macro's type name, followed by a missing constructor field error.  Search
finds no first-party use of this macro family, explaining why the green direct
Property tests do not compile it.

This is a low-severity unused experimental API failure, but it makes the
public macro documentation nonfunctional.

## Assessment

Direct `get`, `set`, implicit read, and the read-only unsupported-set
exception use the supplied callbacks and pass their nominal integration tests.
They do not validate assignment-expression semantics, generic
non-default-constructible values, copying/moving a callback that captures an
owner, or the broken macro entry points.

## Other missing assertions and diagnostics

- Add an assignment-expression test that compares the returned object with
  the new getter value, not only a subsequent `get` call.
- Compile each DEF/IMPL auto/custom/readonly macro pair in a minimal class;
  use an external owner to test copied/moved wrapper callback lifetime.
- Document or remove the default-constructible `cachedValue` requirement if
  the cache is retained; it makes a getter/setter wrapper needlessly reject
  otherwise valid `T` types.

## Final assessment

The experimental header has SR-AUD-179 and SR-AUD-181.  No source or test was
modified.
