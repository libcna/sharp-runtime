<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::Collections.Blocking

Header-only physical component for `BlockingCollection<T>`. Public
dependencies are `Collections.Core`, `Core.Base`, and `Threading` because the
blocking API exposes concurrent collections, core exception/value types, and
cancellation/timeout primitives.

`SharpRuntime::Collections` remains the compatibility umbrella for the full
collections header surface. New consumers that need blocking semantics should
select this narrow component directly.

See the [generated component catalogue](../../docs/ComponentCatalog.md) for
authoritative dependency metadata.
