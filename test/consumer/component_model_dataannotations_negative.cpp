// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2406.
//
// #2406 gave two DataAnnotations attributes .NET's shape:
//
//   * DataTypeAttribute held a public MUTABLE `std::string DataType`, so "a known data
//     type" and "a custom one named by the caller" -- which .NET separates with TWO
//     constructors (DataTypeAttribute.cs:20,55) -- were the same thing, and any string
//     at all was accepted where only seventeen values are meaningful (DataType.cs).
//   * ScaffoldColumnAttribute published a mutable `bool Scaffold` where .NET's is
//     `public bool Scaffold { get; }`.
//
// Migration: DataTypeAttribute(DataType::X) or DataTypeAttribute(std::string) for a
// custom one; read with getDataTypeProperty() / getCustomDataTypeProperty(), and
// GetDataTypeName() for the display name. ScaffoldColumnAttribute reads through
// getScaffoldProperty(); there is no replacement for writing it, deliberately.
//
// WHAT THIS FIXTURE DOES NOT CLAIM, and it is the larger half of #2406: this whole
// namespace VALIDATES NOTHING. .NET's ValidationAttribute carries IsValid, Validate,
// FormatErrorMessage and RequiresValidationContext, and every subclass overrides IsValid;
// none of those exist here. An absence cannot be proved by a fixture -- a fixture proves
// a spelling is REJECTED -- so it is declared in the header and pinned inside the
// repository instead, by DataAnnotations2406Tests.
//
// Records: docs/Migration-DataAnnotationsDataTypeAndScaffold.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=ComponentModel
#include <optional>
#include <string>
#include "System/ComponentModel/DataAnnotations/DataAnnotationAttributes.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using namespace System::ComponentModel::DataAnnotations;

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // The old single-string member is gone, and with it the reading that made a custom
    // name and a known kind indistinguishable.
    // NEGATIVE(datatypeattribute-string-member-is-gone): has no member named
    //     | is private within this context
    DataTypeAttribute attribute(std::string("EmailAddress"));
    const std::string kind = attribute.DataType;
    (void)kind;
#else
    DataTypeAttribute attribute(DataType::EmailAddress);
    const DataType kind = attribute.getDataTypeProperty();
    (void)kind;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // The kind was mutable, so an attribute could be retargeted after construction.
    // NEGATIVE(datatypeattribute-kind-cannot-be-written): has no member named
    //     | is private within this context
    //     | cannot assign
    DataTypeAttribute retargeted(DataType::Text);
    retargeted.DataType = "Password";
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(scaffoldcolumnattribute-field-is-private): is private within this context
    //     | has no member named
    ScaffoldColumnAttribute scaffold(true);
    const bool included = scaffold.Scaffold;
    (void)included;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // A DataType value outside the seventeen cannot be spelled as an enumerator. This is
    // what the enum buys over the old string: the domain is closed at the type level.
    // NEGATIVE(datatype-has-no-invented-enumerator): has not been declared
    //     | is not a member of
    //     | does not name
    const DataType invented = DataType::SocialSecurityNumber;
    (void)invented;
#endif

    // UNCHANGED, and asserted so the fixture proves the change was surgical: the custom
    // route still works and still keeps the kind and the name as two separate facts, and
    // the untouched attributes in this namespace still read as they did.
    const DataTypeAttribute custom(std::string("SocialSecurityNumber"));
    const std::optional<std::string>& name = custom.getCustomDataTypeProperty();
    const RangeAttribute range(1.0, 10.0);
    const StringLengthAttribute length(50);
    return (name.has_value() && custom.getDataTypeProperty() == DataType::Custom &&
            range.getMinimumProperty() == 1.0 && length.getMaximumLengthProperty() == 50)
               ? 0
               : 1;
}
