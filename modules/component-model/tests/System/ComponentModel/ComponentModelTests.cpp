// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
#include <gtest/gtest.h>
#include <any>
#include <optional>
#include "System/IServiceProvider.hpp"
#include "System/ComponentModel/CancelEventArgs.hpp"
#include "System/ComponentModel/IChangeTracking.hpp"
#include "System/ComponentModel/IEditableObject.hpp"
#include "System/ComponentModel/IRevertibleChangeTracking.hpp"
#include "System/ComponentModel/Attribute.hpp"
#include "System/ComponentModel/DefaultValueAttribute.hpp"
#include "System/ComponentModel/DescriptionAttribute.hpp"
#include "System/ComponentModel/CategoryAttribute.hpp"
#include "System/ComponentModel/INotifyPropertyChanged.hpp"
#include "System/ComponentModel/INotifyPropertyChanging.hpp"
#include "System/ComponentModel/ISupportInitialize.hpp"
#include "System/ComponentModel/PropertyChangedEventArgs.hpp"
#include "System/ComponentModel/PropertyChangingEventArgs.hpp"
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
    EXPECT_EQ(attr.getDescriptionProperty(), "A helpful description");
}

TEST(DescriptionAttributeTests, EmptyDescription) {
    DescriptionAttribute attr("");
    EXPECT_EQ(attr.getDescriptionProperty(), "");
}

TEST(DescriptionAttributeTests, DefaultCtor_IsEmptyString) {
    DescriptionAttribute attr;
    EXPECT_EQ(attr.getDescriptionProperty(), "");
}

TEST(DescriptionAttributeTests, Default_IsEmptyStringAndIsDefault) {
    EXPECT_EQ(DescriptionAttribute::Default.getDescriptionProperty(), "");
    EXPECT_TRUE(DescriptionAttribute::Default.getIsDefaultAttributeProperty());
}

TEST(DescriptionAttributeTests, IsDefaultAttribute_FalseForNonEmpty) {
    DescriptionAttribute attr("not default");
    EXPECT_FALSE(attr.getIsDefaultAttributeProperty());
}

TEST(DescriptionAttributeTests, Equals_ComparesByValue) {
    DescriptionAttribute a("same text");
    DescriptionAttribute b("same text");
    DescriptionAttribute c("different text");
    EXPECT_TRUE(a.Equals(b));
    EXPECT_FALSE(a.Equals(c));
}

TEST(DescriptionAttributeTests, GetHashCode_MatchesForEqualValues) {
    DescriptionAttribute a("same text");
    DescriptionAttribute b("same text");
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

// ===========================================================================
// DefaultValueAttribute
// ===========================================================================

TEST(DefaultValueAttributeTests, Constructor_String) {
    DefaultValueAttribute attr(std::string("default"));
    EXPECT_EQ(std::any_cast<std::string>(attr.getValueProperty()), "default");
}

TEST(DefaultValueAttributeTests, Constructor_Int) {
    DefaultValueAttribute attr(42);
    EXPECT_EQ(std::any_cast<int>(attr.getValueProperty()), 42);
}

TEST(DefaultValueAttributeTests, Constructor_Double) {
    DefaultValueAttribute attr(3.14);
    EXPECT_DOUBLE_EQ(std::any_cast<double>(attr.getValueProperty()), 3.14);
}

TEST(DefaultValueAttributeTests, Constructor_Float) {
    DefaultValueAttribute attr(1.5f);
    EXPECT_FLOAT_EQ(std::any_cast<float>(attr.getValueProperty()), 1.5f);
}

TEST(DefaultValueAttributeTests, Constructor_BoolTrue) {
    DefaultValueAttribute attr(true);
    EXPECT_TRUE(std::any_cast<bool>(attr.getValueProperty()));
}

TEST(DefaultValueAttributeTests, Constructor_BoolFalse) {
    DefaultValueAttribute attr(false);
    EXPECT_FALSE(std::any_cast<bool>(attr.getValueProperty()));
}

TEST(DefaultValueAttributeTests, Constructor_Char) {
    DefaultValueAttribute attr('X');
    EXPECT_EQ(std::any_cast<char>(attr.getValueProperty()), 'X');
}

TEST(DefaultValueAttributeTests, Constructor_Long) {
    DefaultValueAttribute attr(100L);
    EXPECT_EQ(std::any_cast<long>(attr.getValueProperty()), 100L);
}

TEST(DefaultValueAttributeTests, Constructor_UnsignedValues) {
    DefaultValueAttribute uintAttribute(static_cast<SharpRuntime::uintcs>(42));
    DefaultValueAttribute ulongAttribute(static_cast<SharpRuntime::ulongcs>(99));
    EXPECT_EQ(std::any_cast<SharpRuntime::uintcs>(uintAttribute.getValueProperty()), 42U);
    EXPECT_EQ(std::any_cast<SharpRuntime::ulongcs>(ulongAttribute.getValueProperty()), 99U);
}

TEST(DefaultValueAttributeTests, NullableString_NullIsRepresentedByAnEmptyAny) {
    DefaultValueAttribute attr(std::optional<std::string>{});
    EXPECT_FALSE(attr.getValueProperty().has_value());
}

TEST(DefaultValueAttributeTests, Equals_UsesStoredTypeAndValue) {
    DefaultValueAttribute first(42);
    DefaultValueAttribute same(42);
    DefaultValueAttribute different(43);
    DefaultValueAttribute differentType(42.0);
    EXPECT_TRUE(first.Equals(same));
    EXPECT_FALSE(first.Equals(different));
    EXPECT_FALSE(first.Equals(differentType));
}

TEST(DefaultValueAttributeTests, TypeStringConstructor_RequiresTheDeferredTypeConverterSystem) {
    EXPECT_THROW(DefaultValueAttribute(System::Type::From<int>(), std::string("42")),
                 System::PlatformNotSupportedException);
}

TEST(DefaultValueAttributeTests, ValueHasType_AfterStringCtor) {
    DefaultValueAttribute attr(std::string("test"));
    EXPECT_EQ(attr.getValueProperty().type(), typeid(std::string));
}

TEST(DefaultValueAttributeTests, IsA_SystemAttribute) {
    DefaultValueAttribute attr(0);
    System::Attribute& base = attr;
    (void)base;
    SUCCEED();
}

// ===========================================================================
// CategoryAttribute
// ===========================================================================

TEST(CategoryAttributeTests, DefaultConstructor_CategoryIsDefault) {
    CategoryAttribute attr;
    EXPECT_EQ(attr.getCategoryProperty(), "Default");
}

TEST(CategoryAttributeTests, Constructor_StoresCategory) {
    CategoryAttribute attr("Appearance");
    EXPECT_EQ(attr.getCategoryProperty(), "Appearance");
}

TEST(CategoryAttributeTests, StaticProperties_ExposeTheDotNetCategories) {
    EXPECT_EQ(CategoryAttribute::getActionProperty().getCategoryProperty(), "Action");
    EXPECT_EQ(CategoryAttribute::getAppearanceProperty().getCategoryProperty(), "Appearance");
    EXPECT_EQ(CategoryAttribute::getAsynchronousProperty().getCategoryProperty(), "Asynchronous");
    EXPECT_EQ(CategoryAttribute::getBehaviorProperty().getCategoryProperty(), "Behavior");
    EXPECT_EQ(CategoryAttribute::getDataProperty().getCategoryProperty(), "Data");
    EXPECT_EQ(CategoryAttribute::getDefaultProperty().getCategoryProperty(), "Default");
    EXPECT_EQ(CategoryAttribute::getDesignProperty().getCategoryProperty(), "Design");
    EXPECT_EQ(CategoryAttribute::getDragDropProperty().getCategoryProperty(), "DragDrop");
    EXPECT_EQ(CategoryAttribute::getFocusProperty().getCategoryProperty(), "Focus");
    EXPECT_EQ(CategoryAttribute::getFormatProperty().getCategoryProperty(), "Format");
    EXPECT_EQ(CategoryAttribute::getKeyProperty().getCategoryProperty(), "Key");
    EXPECT_EQ(CategoryAttribute::getLayoutProperty().getCategoryProperty(), "Layout");
    EXPECT_EQ(CategoryAttribute::getMouseProperty().getCategoryProperty(), "Mouse");
    EXPECT_EQ(CategoryAttribute::getWindowStyleProperty().getCategoryProperty(), "WindowStyle");
}

class LocalizedCategoryAttribute final : public CategoryAttribute {
public:
    using CategoryAttribute::CategoryAttribute;

protected:
    std::optional<std::string> GetLocalizedString(const std::string& value) const override {
        return value == "Appearance" ? std::optional<std::string>("Vzhled") : std::nullopt;
    }
};

TEST(CategoryAttributeTests, DerivedLocalizationHook_IsUsedForValueAndEquality) {
    LocalizedCategoryAttribute localized("Appearance");
    LocalizedCategoryAttribute sameLocalized("Appearance");
    EXPECT_EQ(localized.getCategoryProperty(), "Vzhled");
    EXPECT_TRUE(localized.Equals(sameLocalized));
}

// ===========================================================================
// BrowsableAttribute
// ===========================================================================

TEST(BrowsableAttributeTests, Constructor_True) {
    BrowsableAttribute attr(true);
    EXPECT_TRUE(attr.getBrowsableProperty());
}

TEST(BrowsableAttributeTests, Constructor_False) {
    BrowsableAttribute attr(false);
    EXPECT_FALSE(attr.getBrowsableProperty());
}

TEST(BrowsableAttributeTests, StaticYes_IsTrue) {
    EXPECT_TRUE(BrowsableAttribute::Yes.getBrowsableProperty());
}

TEST(BrowsableAttributeTests, StaticNo_IsFalse) {
    EXPECT_FALSE(BrowsableAttribute::No.getBrowsableProperty());
}

TEST(BrowsableAttributeTests, DefaultAndEquality_MatchDotNet) {
    BrowsableAttribute yes(true);
    BrowsableAttribute no(false);
    EXPECT_TRUE(BrowsableAttribute::Default.getBrowsableProperty());
    EXPECT_TRUE(yes.Equals(BrowsableAttribute::Default));
    EXPECT_FALSE(no.getIsDefaultAttributeProperty());
    EXPECT_EQ(yes.GetHashCode(), 1);
    EXPECT_EQ(no.GetHashCode(), 0);
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

TEST(DisplayNameAttributeTests, DefaultAndEquality_MatchDotNet) {
    DisplayNameAttribute empty;
    DisplayNameAttribute sameEmpty("");
    DisplayNameAttribute named("Name");
    EXPECT_TRUE(empty.getIsDefaultAttributeProperty());
    EXPECT_TRUE(empty.Equals(DisplayNameAttribute::Default));
    EXPECT_TRUE(empty.Equals(sameEmpty));
    EXPECT_FALSE(empty.Equals(named));
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

TEST(EditorBrowsableAttributeTests, DerivesFromAttributeAndComparesState) {
    EditorBrowsableAttribute first(EditorBrowsableState::Advanced);
    EditorBrowsableAttribute same(EditorBrowsableState::Advanced);
    EditorBrowsableAttribute different(EditorBrowsableState::Never);
    EXPECT_TRUE((std::is_base_of_v<System::Attribute, EditorBrowsableAttribute>));
    EXPECT_TRUE(first.Equals(same));
    EXPECT_FALSE(first.Equals(different));
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
    void notifyAllPropertiesChanged() { OnPropertyChanged(std::nullopt); }
};

TEST(INotifyPropertyChangedTests, PropertyChangedEventArgs_StoresName) {
    PropertyChangedEventArgs args("MyProp");
    EXPECT_EQ(args.getPropertyNameProperty(), std::optional<std::string>("MyProp"));
    System::EventArgs& base = args;
    (void)base;
}

TEST(INotifyPropertyChangedTests, OnPropertyChanged_FiresHandlers) {
    TestObservable obj;
    std::string captured;
    obj.PropertyChanged += [&](void*, const PropertyChangedEventArgs& e) {
        captured = *e.getPropertyNameProperty();
    };
    obj.setValue(42);
    EXPECT_EQ(captured, "Value");
}

TEST(INotifyPropertyChangedTests, MultipleHandlers_AllFired) {
    TestObservable obj;
    int count = 0;
    obj.PropertyChanged += [&](void*, const PropertyChangedEventArgs&) { ++count; };
    obj.PropertyChanged += [&](void*, const PropertyChangedEventArgs&) { ++count; };
    obj.setValue(1);
    EXPECT_EQ(count, 2);
}

TEST(INotifyPropertyChangedTests, NoHandlers_DoesNotThrow) {
    TestObservable obj;
    EXPECT_NO_THROW(obj.setValue(5));
}

TEST(INotifyPropertyChangedTests, NullPropertyName_IsPreservedForAllPropertiesNotification) {
    TestObservable obj;
    std::optional<std::string> propertyName = "unexpected";
    obj.PropertyChanged += [&](void*, const PropertyChangedEventArgs& args) {
        propertyName = args.getPropertyNameProperty();
    };
    obj.notifyAllPropertiesChanged();
    EXPECT_FALSE(propertyName.has_value());
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
    EXPECT_EQ(args.getPropertyNameProperty(), std::optional<std::string>("MyProp"));
    System::EventArgs& base = args;
    (void)base;
}

TEST(INotifyPropertyChangingTests, OnPropertyChanging_FiresHandlers) {
    TestChangingObservable obj;
    std::string captured;
    obj.PropertyChanging += [&](void*, const PropertyChangingEventArgs& e) {
        captured = *e.getPropertyNameProperty();
    };
    obj.setValue(99);
    EXPECT_EQ(captured, "Value");
}

TEST(INotifyPropertyChangingTests, HandlerFiredBeforeValueChanges) {
    TestChangingObservable obj;
    int valueAtFireTime = -1;
    obj.PropertyChanging += [&](void*, const PropertyChangingEventArgs&) {
        valueAtFireTime = obj.getValue();
    };
    obj.setValue(7);
    EXPECT_EQ(valueAtFireTime, 0);   // old value, not 7
    EXPECT_EQ(obj.getValue(), 7);    // new value set after
}

TEST(INotifyPropertyChangingTests, NoHandlers_DoesNotThrow) {
    TestChangingObservable obj;
    EXPECT_NO_THROW(obj.setValue(3));
}

// ===========================================================================
// IServiceProvider
// ===========================================================================

class SimpleServiceProvider : public System::IServiceProvider {
    int service_ = 42;
    std::string strService_ = "hello";
public:
    void* GetService(const std::type_info& type) const override {
        if (type == typeid(int))         return const_cast<int*>(&service_);
        if (type == typeid(std::string)) return const_cast<std::string*>(&strService_);
        return nullptr;
    }
};

TEST(IServiceProviderTests, GetService_KnownType_ReturnsNonNull) {
    SimpleServiceProvider sp;
    EXPECT_NE(sp.GetService(typeid(int)), nullptr);
}

TEST(IServiceProviderTests, GetService_ReturnsCorrectValue) {
    SimpleServiceProvider sp;
    int* val = static_cast<int*>(sp.GetService(typeid(int)));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 42);
}

TEST(IServiceProviderTests, GetService_StringService_ReturnsCorrectValue) {
    SimpleServiceProvider sp;
    std::string* s = static_cast<std::string*>(sp.GetService(typeid(std::string)));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, "hello");
}

TEST(IServiceProviderTests, GetService_UnknownType_ReturnsNull) {
    SimpleServiceProvider sp;
    EXPECT_EQ(sp.GetService(typeid(double)), nullptr);
}

TEST(IServiceProviderTests, IsAbstractInterface) {
    EXPECT_TRUE(std::is_abstract_v<System::IServiceProvider>);
}

// ===========================================================================
// CancelEventArgs
// ===========================================================================

using System::ComponentModel::CancelEventArgs;

TEST(CancelEventArgsTests, DefaultCtor_CancelIsFalse) {
    CancelEventArgs e;
    EXPECT_FALSE(e.getCancelProperty());
}

TEST(CancelEventArgsTests, Ctor_True) {
    CancelEventArgs e(true);
    EXPECT_TRUE(e.getCancelProperty());
}

TEST(CancelEventArgsTests, Ctor_False) {
    CancelEventArgs e(false);
    EXPECT_FALSE(e.getCancelProperty());
}

TEST(CancelEventArgsTests, Cancel_CanBeSetToTrue) {
    CancelEventArgs e;
    e.setCancelProperty(true);
    EXPECT_TRUE(e.getCancelProperty());
}

TEST(CancelEventArgsTests, Cancel_CanBeToggled) {
    CancelEventArgs e(true);
    e.setCancelProperty(false);
    EXPECT_FALSE(e.getCancelProperty());
    e.setCancelProperty(true);
    EXPECT_TRUE(e.getCancelProperty());
}

TEST(CancelEventArgsTests, InheritsFromEventArgs) {
    CancelEventArgs e;
    System::EventArgs& base = e;
    (void)base;
    SUCCEED();
}

TEST(CancelEventArgsTests, CancelEventHandler_InvokesAndCanSetCancel) {
    System::ComponentModel::CancelEventHandler handler =
        [](void*, CancelEventArgs& e) { e.setCancelProperty(true); };
    CancelEventArgs e;
    handler(nullptr, e);
    EXPECT_TRUE(e.getCancelProperty());
}

// ===========================================================================
// ISupportInitialize
// ===========================================================================

class InitializableObject : public System::ComponentModel::ISupportInitialize {
    bool initializing_ = false;
    int beginCalls_ = 0;
    int endCalls_ = 0;

public:
    void BeginInit() override { initializing_ = true; ++beginCalls_; }
    void EndInit() override { initializing_ = false; ++endCalls_; }
    [[nodiscard]] bool getInitializingProperty() const noexcept { return initializing_; }
    [[nodiscard]] int getBeginCallsProperty() const noexcept { return beginCalls_; }
    [[nodiscard]] int getEndCallsProperty() const noexcept { return endCalls_; }
};

TEST(ISupportInitializeTests, ImplementerReceivesTheInitializationBoundaryCalls) {
    InitializableObject object;
    object.BeginInit();
    EXPECT_TRUE(object.getInitializingProperty());
    object.EndInit();
    EXPECT_FALSE(object.getInitializingProperty());
    EXPECT_EQ(object.getBeginCallsProperty(), 1);
    EXPECT_EQ(object.getEndCallsProperty(), 1);
}

TEST(ISupportInitializeTests, IsAbstractInterface) {
    EXPECT_TRUE((std::is_abstract_v<System::ComponentModel::ISupportInitialize>));
}

// ===========================================================================
// IChangeTracking
// ===========================================================================

class TrackedObject : public System::ComponentModel::IChangeTracking {
    bool changed_ = false;
public:
    void Modify() { changed_ = true; }
    bool getIsChangedProperty() const override { return changed_; }
    void AcceptChanges() override { changed_ = false; }
};

TEST(IChangeTrackingTests, InitialState_IsNotChanged) {
    TrackedObject obj;
    EXPECT_FALSE(obj.getIsChangedProperty());
}

TEST(IChangeTrackingTests, AfterModify_IsChanged) {
    TrackedObject obj;
    obj.Modify();
    EXPECT_TRUE(obj.getIsChangedProperty());
}

TEST(IChangeTrackingTests, AcceptChanges_ResetsIsChanged) {
    TrackedObject obj;
    obj.Modify();
    obj.AcceptChanges();
    EXPECT_FALSE(obj.getIsChangedProperty());
}

TEST(IChangeTrackingTests, AcceptChanges_Idempotent_WhenNotChanged) {
    TrackedObject obj;
    EXPECT_NO_THROW(obj.AcceptChanges());
    EXPECT_FALSE(obj.getIsChangedProperty());
}

TEST(IChangeTrackingTests, IsAbstractInterface) {
    EXPECT_TRUE(std::is_abstract_v<System::ComponentModel::IChangeTracking>);
}

// ===========================================================================
// IEditableObject
// ===========================================================================

class EditableRecord : public System::ComponentModel::IEditableObject {
    std::string value_;
    std::string snapshot_;
    bool editing_ = false;
public:
    explicit EditableRecord(std::string v) : value_(std::move(v)) {}
    const std::string& getValue() const { return value_; }
    void setValue(const std::string& v) { value_ = v; }

    void BeginEdit() override { snapshot_ = value_; editing_ = true; }
    void EndEdit() override   { editing_ = false; }
    void CancelEdit() override { if (editing_) { value_ = snapshot_; editing_ = false; } }
};

TEST(IEditableObjectTests, EndEdit_CommitsChanges) {
    EditableRecord r("original");
    r.BeginEdit();
    r.setValue("modified");
    r.EndEdit();
    EXPECT_EQ(r.getValue(), "modified");
}

TEST(IEditableObjectTests, CancelEdit_RollsBack) {
    EditableRecord r("original");
    r.BeginEdit();
    r.setValue("modified");
    r.CancelEdit();
    EXPECT_EQ(r.getValue(), "original");
}

TEST(IEditableObjectTests, CancelEdit_WithoutBeginEdit_DoesNotThrow) {
    EditableRecord r("original");
    EXPECT_NO_THROW(r.CancelEdit());
    EXPECT_EQ(r.getValue(), "original");
}

TEST(IEditableObjectTests, BeginEdit_EndEdit_NoRollback) {
    EditableRecord r("a");
    r.BeginEdit();
    r.setValue("b");
    r.EndEdit();
    r.CancelEdit();   // no-op: editing_ is false
    EXPECT_EQ(r.getValue(), "b");
}

TEST(IEditableObjectTests, IsAbstractInterface) {
    EXPECT_TRUE(std::is_abstract_v<System::ComponentModel::IEditableObject>);
}

// ===========================================================================
// IRevertibleChangeTracking
// ===========================================================================

class RevertibleRecord : public System::ComponentModel::IRevertibleChangeTracking {
    std::string value_;
    std::string saved_;
    bool changed_ = false;
public:
    explicit RevertibleRecord(std::string v) : value_(std::move(v)), saved_(value_) {}
    void Modify(const std::string& v) { value_ = v; changed_ = true; }
    const std::string& getValue() const { return value_; }

    bool getIsChangedProperty() const override { return changed_; }
    void AcceptChanges() override { saved_ = value_; changed_ = false; }
    void RejectChanges() override { value_ = saved_; changed_ = false; }
};

TEST(IRevertibleChangeTrackingTests, InitialState_IsNotChanged) {
    RevertibleRecord r("x");
    EXPECT_FALSE(r.getIsChangedProperty());
}

TEST(IRevertibleChangeTrackingTests, AfterModify_IsChanged) {
    RevertibleRecord r("x");
    r.Modify("y");
    EXPECT_TRUE(r.getIsChangedProperty());
}

TEST(IRevertibleChangeTrackingTests, AcceptChanges_CommitsAndClearsFlag) {
    RevertibleRecord r("x");
    r.Modify("y");
    r.AcceptChanges();
    EXPECT_FALSE(r.getIsChangedProperty());
    EXPECT_EQ(r.getValue(), "y");
}

TEST(IRevertibleChangeTrackingTests, RejectChanges_RollsBackAndClearsFlag) {
    RevertibleRecord r("x");
    r.Modify("y");
    r.RejectChanges();
    EXPECT_FALSE(r.getIsChangedProperty());
    EXPECT_EQ(r.getValue(), "x");
}

TEST(IRevertibleChangeTrackingTests, InheritsIChangeTracking) {
    RevertibleRecord r("x");
    System::ComponentModel::IChangeTracking& base = r;
    (void)base;
    SUCCEED();
}

TEST(IRevertibleChangeTrackingTests, IsAbstractInterface) {
    EXPECT_TRUE(std::is_abstract_v<System::ComponentModel::IRevertibleChangeTracking>);
}
