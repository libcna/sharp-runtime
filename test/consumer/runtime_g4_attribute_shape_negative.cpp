// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1980 group G-4 (SR-AUD-160, SR-AUD-164).
//
// G-4 aligned three attribute shapes with the reference, and the port was wrong in BOTH
// directions:
//
//   * CompilerFeatureRequiredAttribute published a full `setIsOptionalProperty`. .NET's is
//     `public bool IsOptional { get; init; }` -- settable AT CONSTRUCTION and immutable
//     afterwards. So the port was too PERMISSIVE. C++ has no `init`; the exact analogue of that
//     pair of facts is a constructor parameter with no setter, which is why the setter is
//     removed and a two-argument constructor added. Removing one without adding the other would
//     have been a narrowing.
//
//   * ObsoletedOSPlatformAttribute and RequiresPreviewFeaturesAttribute took `url` as a trailing
//     constructor parameter and exposed it read-only. .NET declares NO such parameter --
//     `(platformName)`, `(platformName, message)` and `(message)` respectively -- and exposes
//     `public string? Url { get; set; }`. So the port was too RESTRICTIVE on the property and too
//     inventive on the constructor.
//
// Migration: pass the URL after construction via setUrlProperty(), and pass IsOptional to the
// constructor instead of assigning it.
//
// Records: docs/Migration-RuntimeAttributeShapesG4.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Runtime
#include <string>
#include <type_traits>

#include "System/Runtime/CompilerServices/CompilerFeatureRequiredAttribute.hpp"
#include "System/Runtime/Versioning/VersioningAttributes.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Runtime::CompilerServices::CompilerFeatureRequiredAttribute;
using System::Runtime::Versioning::ObsoletedOSPlatformAttribute;
using System::Runtime::Versioning::RequiresPreviewFeaturesAttribute;

int main() {
    CompilerFeatureRequiredAttribute feature("RefStructs");

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(compilerfeaturerequired-setter): has no member named 'setIsOptionalProperty'
    //     | no member named
    feature.setIsOptionalProperty(true);
#else
    CompilerFeatureRequiredAttribute optional("RefStructs", true);
    (void)optional;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(obsoletedosplatform-url-ctor-arg): no matching function
    //     | no matching constructor
    //     | cannot convert
    ObsoletedOSPlatformAttribute obsoleted("ios", "Use X", "https://example.com");
    (void)obsoleted;
#else
    ObsoletedOSPlatformAttribute obsoleted("ios", "Use X");
    obsoleted.setUrlProperty("https://example.com");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(requirespreviewfeatures-url-ctor-arg): no matching function
    //     | no matching constructor
    //     | cannot convert
    RequiresPreviewFeaturesAttribute preview("Preview", "https://aka.ms/preview");
    (void)preview;
#else
    RequiresPreviewFeaturesAttribute preview("Preview");
    preview.setUrlProperty("https://aka.ms/preview");
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(compilerfeaturerequired-still-one-arg-only): static assertion failed
    //     | static_assert
    // The shape that breaks SILENTLY: a trait query, or a template constrained on the
    // single-argument form being the only one.
    static_assert(!std::is_constructible_v<CompilerFeatureRequiredAttribute, std::string, bool>,
                  "expected no two-argument constructor");
#else
    static_assert(std::is_constructible_v<CompilerFeatureRequiredAttribute, std::string, bool>,
                  "#1980 G-4: IsOptional must still be settable at construction");
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: the URL is readable, the
    // one-argument forms still work, and the getters are all still there.
    ObsoletedOSPlatformAttribute minimal("android");
    RequiresPreviewFeaturesAttribute empty;
    return (obsoleted.getUrlProperty() == "https://example.com" &&
            preview.getUrlProperty() == "https://aka.ms/preview" &&
            minimal.getMessageProperty().empty() &&
            empty.getUrlProperty().empty() &&
            !feature.getIsOptionalProperty()) ? 0 : 1;
}
