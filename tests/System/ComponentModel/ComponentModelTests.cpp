// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Note: DefaultValueAttribute.hpp is excluded — it redefines DefaultValueAttribute already
// present in DescriptionAttribute.hpp, causing a redefinition error in the same TU.
#include <gtest/gtest.h>
#include "System/ComponentModel/Attribute.hpp"
#include "System/ComponentModel/DescriptionAttribute.hpp"
#include "System/ComponentModel/CategoryAttribute.hpp"
#include "System/ComponentModel/INotifyPropertyChanged.hpp"
#include "System/ComponentModel/INotifyPropertyChanging.hpp"
#include "System/ComponentModel/EditorBrowsableAttribute.hpp"

using System::ComponentModel::DescriptionAttribute;
using System::ComponentModel::DefaultValueAttribute;
using System::ComponentModel::CategoryAttribute;
using System::ComponentModel::BrowsableAttribute;
using System::ComponentModel::ReadOnlyAttribute;
using System::ComponentModel::DisplayNameAttribute;
using System::ComponentModel::ImmutableObjectAttribute;
using System::ComponentModel::LocalizableAttribute;
using System::ComponentModel::TypeConverterAttribute;
using System::ComponentModel::EditorBrowsableAttribute;
using System::ComponentModel::EditorBrowsableState;
using System::ComponentModel::INotifyPropertyChanged;
using System::ComponentModel::INotifyPropertyChanging;
using System::ComponentModel::PropertyChangedEventArgs;
using System::ComponentModel::PropertyChangingEventArgs;

// ===========================================================================
// ComponentModel::Attribute (base stub)
// ===========================================================================

TEST(ComponentModelAttributeTests, DefaultCtor_NoThrow) {
    EXPECT_NO_THROW(System::ComponentModel::Attribute{});
}

// ===========================================================================
// DescriptionAttribute
// ===========================================================================

TEST(DescriptionAttributeTests, Constructor_StoresDescription) {
    DescriptionAttribute attr("A helpful description");
    EXPECT_EQ(attr.Description, "A helpful description");
}

TEST(DescriptionAttributeTests, EmptyDescription) {
    DescriptionAttribute attr("");
    EXPECT_EQ(attr.Description, "");
}

// ===========================================================================
// DefaultValueAttribute (from DescriptionAttribute.hpp)
// ===========================================================================

TEST(DefaultValueAttributeTests, Constructor_String) {
    DefaultValueAttribute attr(std::string("default"));
    EXPECT_EQ(attr.Value, "default");
}

TEST(DefaultValueAttributeTests, Constructor_Int) {
    DefaultValueAttribute attr(42);
    EXPECT_EQ(attr.Value, "42");
}

TEST(DefaultValueAttributeTests, Constructor_Double) {
    DefaultValueAttribute attr(3.14);
    EXPECT_FALSE(attr.Value.empty());
}

TEST(DefaultValueAttributeTests, Constructor_BoolTrue) {
    DefaultValueAttribute attr(true);
    EXPECT_EQ(attr.Value, "true");
}

TEST(DefaultValueAttributeTests, Constructor_BoolFalse) {
    DefaultValueAttribute attr(false);
    EXPECT_EQ(attr.Value, "false");
}

// ===========================================================================
// CategoryAttribute
// ===========================================================================

TEST(CategoryAttributeTests, DefaultConstructor_CategoryIsMisc) {
    CategoryAttribute attr;
    EXPECT_EQ(attr.getCategoryProperty(), "Misc");
}

TEST(CategoryAttributeTests, Constructor_StoresCategory) {
    CategoryAttribute attr("Appearance");
    EXPECT_EQ(attr.getCategoryProperty(), "Appearance");
}

// ===========================================================================
// BrowsableAttribute
// ===========================================================================

TEST(BrowsableAttributeTests, Constructor_True) {
    BrowsableAttribute attr(true);
    EXPECT_TRUE(attr.Browsable);
}

TEST(BrowsableAttributeTests, Constructor_False) {
    BrowsableAttribute attr(false);
    EXPECT_FALSE(attr.Browsable);
}

TEST(BrowsableAttributeTests, StaticYes_IsTrue) {
    EXPECT_TRUE(BrowsableAttribute::Yes.Browsable);
}

TEST(BrowsableAttributeTests, StaticNo_IsFalse) {
    EXPECT_FALSE(BrowsableAttribute::No.Browsable);
}

// ===========================================================================
// ReadOnlyAttribute
// ===========================================================================

TEST(ReadOnlyAttributeTests, Constructor_True) {
    ReadOnlyAttribute attr(true);
    EXPECT_TRUE(attr.IsReadOnly);
}

TEST(ReadOnlyAttributeTests, Constructor_False) {
    ReadOnlyAttribute attr(false);
    EXPECT_FALSE(attr.IsReadOnly);
}

TEST(ReadOnlyAttributeTests, StaticYes_IsReadOnly) {
    EXPECT_TRUE(ReadOnlyAttribute::Yes.IsReadOnly);
}

TEST(ReadOnlyAttributeTests, StaticNo_IsNotReadOnly) {
    EXPECT_FALSE(ReadOnlyAttribute::No.IsReadOnly);
}

// ===========================================================================
// DisplayNameAttribute
// ===========================================================================

TEST(DisplayNameAttributeTests, DefaultConstructor_EmptyName) {
    DisplayNameAttribute attr;
    EXPECT_EQ(attr.getDisplayNameProperty(), "");
}

TEST(DisplayNameAttributeTests, Constructor_StoresName) {
    DisplayNameAttribute attr("My Property");
    EXPECT_EQ(attr.getDisplayNameProperty(), "My Property");
}

// ===========================================================================
// ImmutableObjectAttribute / LocalizableAttribute
// ===========================================================================

TEST(ImmutableObjectAttributeTests, Constructor_True) {
    ImmutableObjectAttribute attr(true);
    EXPECT_TRUE(attr.Immutable);
}

TEST(ImmutableObjectAttributeTests, Constructor_False) {
    ImmutableObjectAttribute attr(false);
    EXPECT_FALSE(attr.Immutable);
}

TEST(LocalizableAttributeTests, Constructor_True) {
    LocalizableAttribute attr(true);
    EXPECT_TRUE(attr.IsLocalizable);
}

TEST(LocalizableAttributeTests, Constructor_False) {
    LocalizableAttribute attr(false);
    EXPECT_FALSE(attr.IsLocalizable);
}

// ===========================================================================
// TypeConverterAttribute
// ===========================================================================

TEST(TypeConverterAttributeTests, DefaultConstructor_EmptyTypeName) {
    TypeConverterAttribute attr;
    EXPECT_EQ(attr.getConverterTypeNameProperty(), "");
}

TEST(TypeConverterAttributeTests, Constructor_StoresTypeName) {
    TypeConverterAttribute attr("System.Int32Converter");
    EXPECT_EQ(attr.getConverterTypeNameProperty(), "System.Int32Converter");
}

// ===========================================================================
// EditorBrowsableAttribute
// ===========================================================================

TEST(EditorBrowsableAttributeTests, DefaultConstructor_Always) {
    EditorBrowsableAttribute attr;
    EXPECT_EQ(attr.getStateProperty(), EditorBrowsableState::Always);
}

TEST(EditorBrowsableAttributeTests, Constructor_Never) {
    EditorBrowsableAttribute attr(EditorBrowsableState::Never);
    EXPECT_EQ(attr.getStateProperty(), EditorBrowsableState::Never);
}

TEST(EditorBrowsableAttributeTests, Constructor_Advanced) {
    EditorBrowsableAttribute attr(EditorBrowsableState::Advanced);
    EXPECT_EQ(attr.getStateProperty(), EditorBrowsableState::Advanced);
}

TEST(EditorBrowsableAttributeTests, Always_ValueIsZero) {
    EXPECT_EQ(static_cast<int>(EditorBrowsableState::Always), 0);
}

TEST(EditorBrowsableAttributeTests, Never_ValueIsOne) {
    EXPECT_EQ(static_cast<int>(EditorBrowsableState::Never), 1);
}

TEST(EditorBrowsableAttributeTests, Advanced_ValueIsTwo) {
    EXPECT_EQ(static_cast<int>(EditorBrowsableState::Advanced), 2);
}

// ===========================================================================
// INotifyPropertyChanged
// ===========================================================================

class TestObservable : public INotifyPropertyChanged {
    int value_ = 0;
public:
    int getValue() const { return value_; }
    void setValue(int v) {
        value_ = v;
        OnPropertyChanged("Value");
    }
};

TEST(INotifyPropertyChangedTests, PropertyChangedEventArgs_StoresName) {
    PropertyChangedEventArgs args("MyProp");
    EXPECT_EQ(args.PropertyName, "MyProp");
}

TEST(INotifyPropertyChangedTests, OnPropertyChanged_FiresHandlers) {
    TestObservable obj;
    std::string captured;
    obj.PropertyChanged.push_back([&](void*, const PropertyChangedEventArgs& e) {
        captured = e.PropertyName;
    });
    obj.setValue(42);
    EXPECT_EQ(captured, "Value");
}

TEST(INotifyPropertyChangedTests, MultipleHandlers_AllFired) {
    TestObservable obj;
    int count = 0;
    obj.PropertyChanged.push_back([&](void*, const PropertyChangedEventArgs&) { ++count; });
    obj.PropertyChanged.push_back([&](void*, const PropertyChangedEventArgs&) { ++count; });
    obj.setValue(1);
    EXPECT_EQ(count, 2);
}

TEST(INotifyPropertyChangedTests, NoHandlers_DoesNotThrow) {
    TestObservable obj;
    EXPECT_NO_THROW(obj.setValue(5));
}

// ===========================================================================
// INotifyPropertyChanging
// ===========================================================================

class TestChangingObservable : public INotifyPropertyChanging {
    int value_ = 0;
public:
    void setValue(int v) {
        OnPropertyChanging("Value");
        value_ = v;
    }
    int getValue() const { return value_; }
};

TEST(INotifyPropertyChangingTests, PropertyChangingEventArgs_StoresName) {
    PropertyChangingEventArgs args("MyProp");
    EXPECT_EQ(args.PropertyName, "MyProp");
}

TEST(INotifyPropertyChangingTests, OnPropertyChanging_FiresHandlers) {
    TestChangingObservable obj;
    std::string captured;
    obj.PropertyChanging.push_back([&](void*, const PropertyChangingEventArgs& e) {
        captured = e.PropertyName;
    });
    obj.setValue(99);
    EXPECT_EQ(captured, "Value");
}

TEST(INotifyPropertyChangingTests, HandlerFiredBeforeValueChanges) {
    TestChangingObservable obj;
    int valueAtFireTime = -1;
    obj.PropertyChanging.push_back([&](void*, const PropertyChangingEventArgs&) {
        valueAtFireTime = obj.getValue();
    });
    obj.setValue(7);
    EXPECT_EQ(valueAtFireTime, 0);   // old value, not 7
    EXPECT_EQ(obj.getValue(), 7);    // new value set after
}

TEST(INotifyPropertyChangingTests, NoHandlers_DoesNotThrow) {
    TestChangingObservable obj;
    EXPECT_NO_THROW(obj.setValue(3));
}
