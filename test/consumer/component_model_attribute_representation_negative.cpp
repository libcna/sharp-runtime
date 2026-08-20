// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #2403.
//
// Six System::ComponentModel attributes published a bare MUTABLE public data member
// where .NET publishes a get-only property. Landed under SA-8 ("where this port
// publishes a mutable or public representation and .NET's is private, readonly or
// absent, match .NET and migrate the first-party sites").
//
// WHAT MADE IT MORE THAN A STYLE POINT IS THAT THE PORT WAS INCONSISTENT WITH ITSELF
// IN ONE FILE: CategoryAttribute, BrowsableAttribute, DisplayNameAttribute and
// DescriptionAttribute already had the correct shape, in that very header and its
// neighbour. The other six did not.
//
//   ReadOnlyAttribute             public bool IsReadOnly       -> getIsReadOnlyProperty()
//   ImmutableObjectAttribute      public bool Immutable        -> getImmutableProperty()
//   LocalizableAttribute          public bool IsLocalizable    -> getIsLocalizableProperty()
//   MergablePropertyAttribute     public bool AllowMerge       -> getAllowMergeProperty()
//   NotifyParentPropertyAttribute public bool NotifyParent     -> getNotifyParentProperty()
//   RefreshPropertiesAttribute    public Refresh RefreshProperties_
//                                                              -> getRefreshPropertiesProperty()
//
// Two spellings are outlawed per type and they are NOT the same claim, which is why
// the sites below are chosen the way they are:
//
//   * READING the field. Loud, and every reader is named by the compiler.
//   * WRITING the field. This is the one the change exists to stop -- a metadata
//     attribute that a caller can retarget after construction is not metadata -- and
//     it is ALSO the one that would have gone unnoticed longest, because a write
//     compiles silently against the old shape and changes the attribute's meaning.
//
// A third site pins the ENUM's scope: .NET's RefreshProperties is a top-level enum
// (RefreshProperties.cs), and this port nested it as RefreshPropertiesAttribute::Refresh.
// The nested spelling must stop resolving.
//
// Measured before landing: 8 first-party sites, all in this module's own test file;
// ZERO derivations anywhere; ZERO sites of any kind in cna and mobile-eggbert.
//
// Migration: read through getXxxProperty(). There is no replacement for WRITING one,
// deliberately -- .NET publishes no setter, and a caller that needs a different value
// constructs a different attribute (or uses the Yes/No/Default statics, which #2403
// also completed).
//
// Records: docs/Migration-ComponentModelAttributeRepresentation.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=ComponentModel
#include "System/ComponentModel/CategoryAttribute.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using namespace System::ComponentModel;

int main() {
#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(readonlyattribute-field-is-private): is private within this context
    //     | has no member named
    ReadOnlyAttribute attribute(true);
    const bool value = attribute.IsReadOnly;
    (void)value;
#else
    ReadOnlyAttribute attribute(true);
    const bool value = attribute.getIsReadOnlyProperty();
    (void)value;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // The write is the spelling this change exists to stop, and the one that would have
    // survived a careless migration longest: it compiles silently against the old shape.
    // NEGATIVE(readonlyattribute-field-cannot-be-written): is private within this context
    //     | has no member named
    //     | cannot assign
    ReadOnlyAttribute retargeted(false);
    retargeted.IsReadOnly = true;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(immutableobjectattribute-field-is-private): is private within this context
    //     | has no member named
    ImmutableObjectAttribute immutable(true);
    const bool immutableValue = immutable.Immutable;
    (void)immutableValue;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(localizableattribute-field-is-private): is private within this context
    //     | has no member named
    LocalizableAttribute localizable(true);
    const bool localizableValue = localizable.IsLocalizable;
    (void)localizableValue;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(mergablepropertyattribute-field-is-private): is private within this context
    //     | has no member named
    MergablePropertyAttribute mergable(true);
    const bool mergableValue = mergable.AllowMerge;
    (void)mergableValue;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(notifyparentpropertyattribute-field-is-private): is private within this context
    //     | has no member named
    NotifyParentPropertyAttribute notify(true);
    const bool notifyValue = notify.NotifyParent;
    (void)notifyValue;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(refreshpropertiesattribute-field-is-private): is private within this context
    //     | has no member named
    RefreshPropertiesAttribute refresh(RefreshProperties::All);
    const auto refreshValue = refresh.RefreshProperties_;
    (void)refreshValue;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // .NET's RefreshProperties is a TOP-LEVEL enum; the nested spelling must not resolve.
    // NEGATIVE(refreshproperties-is-not-nested): has not been declared
    //     | is not a member of
    //     | does not name a type
    const auto nested = RefreshPropertiesAttribute::Refresh::All;
    (void)nested;
#endif

    // UNCHANGED, and asserted so the fixture proves the change was surgical: the four
    // attributes in this same header that ALREADY had the correct shape still have it,
    // and the statics #2403 completed are readable.
    const bool browsable = BrowsableAttribute::Default.getBrowsableProperty();
    const bool merged = MergablePropertyAttribute::Default.getAllowMergeProperty();
    const auto category = CategoryAttribute::getDefaultProperty().getCategoryProperty();
    (void)browsable;
    (void)merged;
    (void)category;
    return 0;
}
