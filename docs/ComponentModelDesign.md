<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# ComponentModel conversion and metadata design

## Scope and requirements

SharpRuntime implements the small, functional `System.ComponentModel` subset
needed by XNA-style value converters and by ordinary native consumers. Before
this work the component contained attributes and event-related contracts, while
`PropertyDescriptorCollection` was only a compatibility shell and no coherent
TypeConverter substrate existed. `UriTypeConverter` consequently exposed a
one-off API and `DefaultValueAttribute(Type, string)` could not perform the
conversion required by .NET.

The implemented surface is intentionally narrower than the complete .NET
ComponentModel stack. It supports conversion, explicit property metadata,
immutable/value-type recreation, converter lookup, and construction descriptors.
It does not implement PropertyGrid UI, designers, CodeDOM, dynamic custom-type
descriptors, or runtime discovery of arbitrary CLR attributes.

## Type conversion

`TypeConverter` supplies context- and culture-aware `CanConvertFrom`,
`CanConvertTo`, `ConvertFrom`, `ConvertTo`, string helpers, property enumeration,
validation, standard-values hooks, and `CreateInstance`. Unsupported conversions
throw `NotSupportedException`. `ExpandableObjectConverter` opts into property
enumeration. Integer, floating-point, Boolean, string, and Uri converters are
available through `TypeDescriptor`.

Numeric converters use `CultureInfo`'s `NumberFormatInfo`; this includes decimal
separators, sign text, and native digits. `TextInfo.ListSeparator` remains
available to composite converters such as XNA's math converters. The culture
table is deliberately finite and deterministic rather than dependent on the
process's installed locales.

## Converter discovery

`TypeDescriptor` has two association paths:

1. intrinsic converters for SharpRuntime primitive and Uri types;
2. explicit `TypeConverterAttribute` registration by a library that owns an
   external type.

Registrations store factories, not converter instances. Lookup constructs a
converter lazily, caches it, and is protected by a mutex. Registration and
refresh advance a generation so a lookup racing a replacement cannot publish a
stale converter. Replacement is deterministic: the latest completed
registration applies. Generic SharpRuntime code has no dependency on CNA or any
other registering library.

Properties use the same explicit registry. This avoids static field scanning and
lets the type-owning library install a stable ordered collection. Consumers that
never ask for a converter or descriptors pay no allocation cost.

## Descriptors and collections

`PropertyDescriptor` is a real polymorphic property abstraction: it carries a
name and attributes, reports component and property types, reads and writes boxed
values, exposes child properties and converters, and defines metadata equality.
Libraries provide concrete descriptors using pointers to members or getters and
setters; general reflection is not required.

`PropertyDescriptorCollection` owns an ordered vector of shared descriptors. It
supports count and read-only state, index and name access, lookup, iteration,
mutation for writable collections, and stable sorting. Copies own their ordering
while sharing immutable descriptor objects, mirroring .NET reference identity
without dangling references. `Empty` is immutable.

## InstanceDescriptor and the reflection boundary

`InstanceDescriptor` stores a construction member, boxed arguments, and the .NET
`IsComplete` flag. `Invoke` recursively evaluates nested InstanceDescriptors and
calls the member, so the descriptor is executable rather than a name-only stub.
It validates argument count on construction and reports invalid argument types as
`ArgumentException`.

The only Reflection functionality added is the explicit invocation boundary
needed for that contract:

- `MemberTypes` and `MemberInfo` identity;
- `MethodBase` and `ParameterInfo` metadata;
- `ConstructorInfo::Of<T, Args...>` with a checked callable;
- `TargetParameterCountException`.

There is no assembly scanning, member enumeration, dynamic method lookup, field
reflection, or custom-attribute engine. Constructor metadata is created at a
compile-time-known call site and remains reusable outside XNA.

## Existing consumer integration

`DefaultValueAttribute(Type, string)` now obtains the registered converter and
uses invariant-culture conversion, retaining an empty value if conversion fails
as .NET does. `UriTypeConverter` now derives from `TypeConverter`, participates in
intrinsic lookup, converts to executable InstanceDescriptors, and retains Uri's
empty-string/null semantics using an empty `std::any`.

## Compatibility boundary

The public shapes follow SharpRuntime's established C++ mapping: boxed objects are
`std::any`, types are `System::Type`, and property mutation receives a mutable
boxed value. `ITypeDescriptorContext` exposes the context functions usable
without implementing the much larger .NET service-container surface. These are
intentional C++ representation choices; the conversion, ordering, reconstruction,
and failure behaviors needed by XNA Design are implemented and tested.
