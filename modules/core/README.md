<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::Core.Base

Compiled acyclic foundation component. It owns fundamental `System` APIs and
the small cross-namespace primitives that must remain below every higher
layer. It has no production dependency.

This directory also declares the `SharpRuntime::Core` compatibility umbrella
over `Core.Base`, `Console`, `Uri`, and `TimeZone`. See
[Core ownership](../../docs/CoreOwnership.md) and the
[generated component catalogue](../../docs/ComponentCatalog.md).
