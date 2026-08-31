<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::Xml.Serialization

Header-only (`INTERFACE`) XML serialization component. Public dependencies:
`Core.Base` and `Xml`; tinyxml2 arrives transitively through the public `Xml`
surface.

`XmlSerializer<T>` is the C++ counterpart of .NET
`System.Xml.Serialization.XmlSerializer`. Reflection is a permanent deviation
in this runtime, so a type opts in explicitly with `SHARP_XML_SERIALIZABLE`
and `SHARP_XML_M` — a compile-time customization point in the same shape
`JsonSerializer` uses through `nlohmann`'s ADL hooks.

Scope, evidence, deviations and the golden-fixture corpus are documented in
[`docs/XmlSerializationScope.md`](../../docs/XmlSerializationScope.md).

See the [generated component catalogue](../../docs/ComponentCatalog.md) for
authoritative dependency metadata.
