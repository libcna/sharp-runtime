// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2405. Two independent halves.
//
// HALF A -- PropertyChangedEventArgs and PropertyChangingEventArgs carried a SECOND,
// public `std::string PropertyName` beside the private `std::optional<std::string>`,
// snapshotted in the constructor. .NET's is four lines:
// `public virtual string? PropertyName { get; }` (PropertyChangedEventArgs.cs).
//
// Three defects lived in that one member, and the sites below are chosen so each is
// pinned by the spelling that reaches it:
//
//   * READING it was LOSSY. `value_or("")` collapsed `std::nullopt` and `""` into one
//     state -- the #2295 defect, where an absent value and an empty one become
//     indistinguishable.
//   * WRITING it was possible at all, where .NET's is get-only. A subscriber could
//     retarget the args object mid-dispatch and every later subscriber saw the change.
//   * After such a write the field and `getPropertyNameProperty()` DISAGREED, so the
//     object contradicted itself. That one is only reachable BECAUSE the field is
//     public, which is why the write earns its own site.
//
// The field was kept "for existing sharp-runtime consumers that predate the
// nullable-property port". That reason was measured and is empty: ZERO sites in cna,
// ZERO in mobile-eggbert, and ONE first-party read in this repository's own tests.
//
// Migration: `args.PropertyName` becomes `args.getPropertyNameProperty()`, which returns
// `const std::optional<std::string>&`. A caller that always supplies a name reads
// `.value()`; a caller that handles .NET's all-properties convention checks
// `has_value()` first. There is no replacement for WRITING one, deliberately.
//
// HALF B -- System::ComponentModel::Attribute is REMOVED. There is no
// System.ComponentModel.Attribute in .NET -- measured, no such file anywhere in the
// reference tree. .NET's ComponentModel attributes derive from System.Attribute, and so
// do this port's: 20 from System::Attribute and 11 from ValidationAttribute, and ZERO
// from the removed type. It had no members, no derived classes and no callers; its only
// appearance outside its own header was a test asserting it could be default-constructed.
// This is the #2334 (RuntimeType) / #2281 (UnitySerializationHolder) shape.
//
// Migration: derive from System::Attribute, which is what every attribute in this
// namespace already did.
//
// Records: docs/Migration-ComponentModelPropertyNameAndPhantomAttribute.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=ComponentModel
#include <optional>
#include <string>
#include "System/Attribute.hpp"
#include "System/ComponentModel/CategoryAttribute.hpp"
#include "System/ComponentModel/PropertyChangedEventArgs.hpp"
#include "System/ComponentModel/PropertyChangingEventArgs.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

int main() {
    System::ComponentModel::PropertyChangedEventArgs changed(std::string("Count"));
    System::ComponentModel::PropertyChangingEventArgs changing(std::string("Count"));

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(propertychangedeventargs-legacy-field-is-gone): has no member named
    //     | is private within this context
    const std::string name = changed.PropertyName;
    (void)name;
#else
    const std::optional<std::string>& name = changed.getPropertyNameProperty();
    (void)name;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // The write is the spelling that made the object able to contradict itself.
    // NEGATIVE(propertychangedeventargs-name-cannot-be-written): has no member named
    //     | is private within this context
    //     | cannot assign
    changed.PropertyName = "Retargeted";
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // Both types had the identical shape, so repairing one and not the other would have
    // left the port disagreeing with itself. Pinned on the sibling for that reason.
    // NEGATIVE(propertychangingeventargs-legacy-field-is-gone): has no member named
    //     | is private within this context
    const std::string changingName = changing.PropertyName;
    (void)changingName;
#else
    const std::optional<std::string>& changingName = changing.getPropertyNameProperty();
    (void)changingName;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(componentmodel-attribute-type-is-removed): has not been declared
    //     | is not a member of
    //     | does not name a type
    //     | No such file or directory
    System::ComponentModel::Attribute phantom;
    (void)phantom;
#endif

    // UNCHANGED, and asserted so the fixture proves the change was surgical: the accessor
    // .NET does publish still works and still distinguishes the two states, and the
    // attributes in this namespace still derive from the base they always derived from.
    const System::ComponentModel::PropertyChangedEventArgs allProperties(std::nullopt);
    static_assert(std::is_base_of_v<System::Attribute,
                                    System::ComponentModel::ReadOnlyAttribute>);
    return (allProperties.getPropertyNameProperty().has_value() ||
            !changed.getPropertyNameProperty().has_value())
               ? 1
               : 0;
}
