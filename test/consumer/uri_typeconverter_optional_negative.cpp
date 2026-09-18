// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for migration from the intermediate optional-returning
// UriTypeConverter API to the shared TypeConverter std::any surface. The historical
// filename is retained so fixture inventories remain stable.
//
// Records: docs/Migration-UriTypeConverterIsATypeConverter.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=ComponentModel
#include <any>
#include <optional>
#include <string>
#include <type_traits>

#include "System/Uri.hpp"
#include "System/UriTypeConverter.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Uri;
using System::UriTypeConverter;

int main() {
    const UriTypeConverter converter;

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(uritypeconverter-optional-result-removed): conversion from
//     | cannot convert
    std::optional<Uri> result = converter.ConvertFrom(std::any(std::string("relative/path")));
    (void)result;
#else
    const std::any result = converter.ConvertFrom(std::any(std::string("relative/path")));
    (void)std::any_cast<const Uri&>(result);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(uritypeconverter-direct-member-access-on-any): has no member named
    //     | no member named
    const std::string host = converter.ConvertFrom(
        std::any(std::string("http://example.com"))).getHostProperty();
    (void)host;
#else
    const std::any absolute = converter.ConvertFrom(
        std::any(std::string("http://example.com")));
    const std::string host = std::any_cast<const Uri&>(absolute).getHostProperty();
    (void)host;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(uritypeconverter-old-override-removed): marked 'override', but does not override
    //     | does not override
    struct MyConverter final : UriTypeConverter {
        std::optional<Uri> ConvertFrom(const std::string&) const override { return std::nullopt; }
    };
#else
    struct MyConverter final : UriTypeConverter {
        std::any ConvertFrom(System::ComponentModel::ITypeDescriptorContext* context,
                             const System::Globalization::CultureInfo* culture,
                             const std::any& value) const override {
            return UriTypeConverter::ConvertFrom(context, culture, value);
        }
    };
#endif

    static_assert(std::is_base_of_v<System::ComponentModel::TypeConverter, UriTypeConverter>);
    const std::any empty = converter.ConvertFrom(std::any(std::string{}));
    return !empty.has_value() ? 0 : 1;
}
