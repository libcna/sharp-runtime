<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::Collections.Core

Header-only physical component for synchronous generic, specialized,
immutable, and non-blocking concurrent collections. Its only public
dependency is `Core.Base`; tests additionally use `IO` and `Text`.

`BlockingCollection<T>` is intentionally owned by the narrow
`Collections.Blocking` component so cancellation and timeout support do not
leak `Threading` into ordinary collection consumers.

This directory also declares the `SharpRuntime::Collections` compatibility
umbrella over `Collections.Core`, `Collections.Blocking`, `Collections.Async`,
and `Collections.ObjectModel`.

See the [generated component catalogue](../../docs/ComponentCatalog.md) for
authoritative dependency metadata.
