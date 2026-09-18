<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::ComponentModel

Compiled physical component for attributes, notifications, initialization,
change tracking, async-completion metadata, and the reusable type-conversion and
property-metadata subset used by design-time consumers. Public dependencies:
`Core.Base`, `Collections.Core`, `Globalization`, and `Uri`.

The implemented design-time surface includes `TypeConverter`,
`ExpandableObjectConverter`, `TypeDescriptor`, explicit property descriptors,
`PropertyDescriptorCollection`, and `Design.Serialization.InstanceDescriptor`.
It uses explicit factories and constructor metadata rather than general runtime
reflection. See `docs/ComponentModelDesign.md` for the supported surface,
registration model, and deliberate scope boundary.

See `docs/ComponentCatalog.md` for authoritative dependency metadata.
