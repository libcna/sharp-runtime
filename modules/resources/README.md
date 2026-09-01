<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::Resources

Compiled resource lookup component. Public dependency: `Globalization`; `Core.Base` is private.

Sharp Runtime has no reflection or Assembly metadata. `ResourceManager` therefore consumes an AOT
exact-resource callback supplied by generated or hand-authored resource code, while retaining the
framework-owned full-culture, parent-culture, and invariant fallback order.

See `docs/ComponentCatalog.md` for authoritative dependency metadata.
