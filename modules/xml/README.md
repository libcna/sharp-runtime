<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::Xml

Compiled XML and XPath component. Public dependencies: `Core.Base` and `Uri`;
`Diagnostics` is implementation-only and `Xml.Linq` is test-only. Vendored
tinyxml2 is public because XML headers expose its types.

`SharpRuntime::Xml.XPath` aliases this physical archive because XML and XPath
have mutual binary dependencies.

See the [generated component catalogue](../../docs/ComponentCatalog.md) for
authoritative dependency metadata.
