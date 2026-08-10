# Audit: `modules/runtime/include/System/Runtime/Versioning/VersioningAttributes.hpp`

## Metadata

- AUDITED: 142-line inline versioning/platform-metadata declaration, fully
  read.
- Validation: all seven Versioning fixture suites passed 11/11 on 2026-07-27.
- Reference/probes: local current-.NET TargetFramework, RequiresPreviewFeatures,
  PlatformAttributes, and System.Runtime reference surface; a C++ probe cannot
  name `OSPlatformAttribute`, while a managed TargetFramework probe prints
  `True` for default `FrameworkDisplayName == null`.

## SR-AUD-163 — medium — platform annotations omit the public OSPlatformAttribute hierarchy and any common native consumer

Current .NET exposes public abstract `OSPlatformAttribute`, and all five
supported/unsupported/obsoleted/guard attribute classes derive from it.  The
C++ header declares each type directly under `System::Attribute` and does not
define an `OSPlatformAttribute` equivalent.  The standalone C++ probe fails at
`OSPlatformAttribute* base`, so generic code cannot accept, inspect, or
dispatch platform annotations through the managed public base type.

There is also no C++ declaration-attachment or analyzer consumer: searches
find only the aggregate object-construction fixture.  The header’s prose
describes APIs/guards as annotations but provides no native replacement for
the common hierarchy or the caller analysis that uses it.

## SR-AUD-164 — medium — nullable and mutable versioning metadata is collapsed into constructor-only strings

`std::string` fields erase `null` versus explicitly empty values for
TargetFramework.FrameworkDisplayName, UnsupportedOSPlatform.Message,
ObsoletedOSPlatform.Message/Url, and RequiresPreviewFeatures.Message/Url.  The
managed probe confirms the first default is null, whereas the C++ getter and
direct test expose empty text.

Current .NET exposes settable nullable `Url` properties on ObsoletedOSPlatform
and RequiresPreviewFeatures.  C++ instead accepts `url` as a constructor
parameter (including a non-managed three-argument Obsoleted constructor and
two-argument Requires constructor) and exposes no setter.  A C++ probe
compiles both constructor routes, while the local current-.NET sources expose
only one/two-argument Obsoleted constructors and one-argument Requires with a
mutable Url property.  Callers cannot represent the same state transitions or
metadata values.

## Other missing assertions and diagnostics

- Tests omit OSPlatformAttribute polymorphism, derived sealing, target
attachment, multiple annotations, guard boolean semantics, and analyzer
warnings/suppression behavior.
- TargetFramework tests lock empty display text instead of distinguishing null;
  none test null/empty platform messages or nullable URL/message states.
- Obsoleted tests exercise the incompatible constructor URL route; no test
  checks a post-construction Url transition for Obsoleted or RequiresPreview.
- No fixture validates standard-platform/version spelling, malformed text,
  UTF-8/embedded-NUL content, or native handling policy for absent metadata.

## Final assessment

Ordinary stored text is coherent, but public platform hierarchy and nullable/
mutable metadata contracts are materially flattened.  No source or test was
modified.

## Post-audit disposition — the `System::Runtime` namespace review, ticket #1972 (2026-08-03)

This file's open finding(s) were re-verified against current source on 2026-08-03 and
remain **confirmed** with no correction to premise, count, severity or consequence.
They belong to cause **R-G** — *the public shape itself diverges* — the runtime
analogue of `docs/ThreadingNamespaceReviewPlan.md`'s cause **T-H**, and are owned by
the **approval-gated** ticket **#1980**. Each one changes a declaration a consumer can
already name (a base class, a sealing decision, a constructor set, a public enum value,
a field default, a field type, or a property's presence), so none is implemented
without approval.

`docs/SystemRuntimeNamespaceReviewPlan.md` §10.2 splits R-G into five groups so the
answer can be partial, and recommends **G-1** — `OSPlatform`'s default constructor,
`RuntimeInformation`'s two absent identity properties, and `ExternalException`'s
error-code surface — as the cheapest minimum, because it is **purely additive** and
cannot break a consumer.

**No new `SR-AUD-*` identifier was issued**; numbering stays frozen at **364**.
