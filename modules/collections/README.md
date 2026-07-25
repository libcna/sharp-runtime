<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::Collections.Core

Header-only physical component for synchronous generic, specialized,
immutable, and concurrent collections. Public dependencies: `Core.Base` and
`Threading`; tests additionally use `IO` and `Text`.

`Threading` is a known temporary isolation regression caused only by
`BlockingCollection`. `NEXT.md` proposes extracting that header into a narrow
`Collections.Blocking` physical component; do not weaken the Text.Json
negative assertion to normalize the broader closure.

This directory also declares the `SharpRuntime::Collections` compatibility
umbrella over `Collections.Core`, `Collections.Async`, and
`Collections.ObjectModel`.

See the [generated component catalogue](../../docs/ComponentCatalog.md) for
authoritative dependency metadata.
