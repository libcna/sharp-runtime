<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `UriTypeConverter` is a `TypeConverter`

`System::UriTypeConverter` now derives from
`System::ComponentModel::TypeConverter`, matching .NET. It is provided by the
`ComponentModel` physical component rather than `Uri`, because its public API
names ComponentModel, globalization, and design-serialization types.

The old string-only convenience surface has been replaced by the common boxed
conversion surface:

```cpp
System::UriTypeConverter converter;

std::any converted = converter.ConvertFrom(std::any(std::string("relative/path")));
const auto& uri = std::any_cast<const System::Uri&>(converted);

std::any text = converter.ConvertTo(std::any(uri), System::Type::From<std::string>());
const auto& original = std::any_cast<const std::string&>(text);
```

An empty input string represents .NET `null` as an empty `std::any`. Unsupported
source or destination types throw `NotSupportedException`. `CanConvertFrom`
reports the base TypeConverter support for `InstanceDescriptor`, but .NET's
`UriTypeConverter::ConvertFrom` override does not consume an InstanceDescriptor;
attempting that conversion throws. Conversion *to* an InstanceDescriptor creates
a working descriptor that reconstructs the Uri.

Code that only needs the converter indirectly should use
`TypeDescriptor::GetConverter(Type::From<System::Uri>())`; Uri is one of the
intrinsic associations and requires no registration.
