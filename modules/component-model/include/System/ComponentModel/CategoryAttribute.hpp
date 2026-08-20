// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include "System/Attribute.hpp"
#include "System/Type.hpp"

namespace System::ComponentModel {

    /**
     * Specifies the category in which a property or event is displayed.
     *
     * C++ counterpart of .NET System.ComponentModel.CategoryAttribute. The default implementation
     * retains the English category names because this runtime has no ComponentModel resource set;
     * a derived class can override GetLocalizedString to provide localized text.
     */
    class CategoryAttribute : public System::Attribute {
        std::string category_;

    public:
        /** Constructs the attribute with the default category. */
        CategoryAttribute() : category_("Default") {}

        /** Constructs the attribute with the given category name. */
        explicit CategoryAttribute(std::string category) : category_(std::move(category)) {}

        /** Gets the action category attribute. */
        [[nodiscard]] static const CategoryAttribute& getActionProperty() {
            static const CategoryAttribute value("Action");
            return value;
        }

        /** Gets the appearance category attribute. */
        [[nodiscard]] static const CategoryAttribute& getAppearanceProperty() {
            static const CategoryAttribute value("Appearance");
            return value;
        }

        /** Gets the asynchronous category attribute. */
        [[nodiscard]] static const CategoryAttribute& getAsynchronousProperty() {
            static const CategoryAttribute value("Asynchronous");
            return value;
        }

        /** Gets the behavior category attribute. */
        [[nodiscard]] static const CategoryAttribute& getBehaviorProperty() {
            static const CategoryAttribute value("Behavior");
            return value;
        }

        /** Gets the data category attribute. */
        [[nodiscard]] static const CategoryAttribute& getDataProperty() {
            static const CategoryAttribute value("Data");
            return value;
        }

        /** Gets the default category attribute. */
        [[nodiscard]] static const CategoryAttribute& getDefaultProperty() {
            static const CategoryAttribute value;
            return value;
        }

        /** Gets the design category attribute. */
        [[nodiscard]] static const CategoryAttribute& getDesignProperty() {
            static const CategoryAttribute value("Design");
            return value;
        }

        /** Gets the drag-and-drop category attribute. */
        [[nodiscard]] static const CategoryAttribute& getDragDropProperty() {
            static const CategoryAttribute value("DragDrop");
            return value;
        }

        /** Gets the focus category attribute. */
        [[nodiscard]] static const CategoryAttribute& getFocusProperty() {
            static const CategoryAttribute value("Focus");
            return value;
        }

        /** Gets the format category attribute. */
        [[nodiscard]] static const CategoryAttribute& getFormatProperty() {
            static const CategoryAttribute value("Format");
            return value;
        }

        /** Gets the keyboard category attribute. */
        [[nodiscard]] static const CategoryAttribute& getKeyProperty() {
            static const CategoryAttribute value("Key");
            return value;
        }

        /** Gets the layout category attribute. */
        [[nodiscard]] static const CategoryAttribute& getLayoutProperty() {
            static const CategoryAttribute value("Layout");
            return value;
        }

        /** Gets the mouse category attribute. */
        [[nodiscard]] static const CategoryAttribute& getMouseProperty() {
            static const CategoryAttribute value("Mouse");
            return value;
        }

        /** Gets the window-style category attribute. */
        [[nodiscard]] static const CategoryAttribute& getWindowStyleProperty() {
            static const CategoryAttribute value("WindowStyle");
            return value;
        }

        /** Gets the category name, applying a derived class's localization hook when supplied. */
        [[nodiscard]] std::string getCategoryProperty() const {
            const std::optional<std::string> localized = GetLocalizedString(category_);
            return localized ? *localized : category_;
        }

        /** Compares attributes by their resolved category names. */
        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const CategoryAttribute*>(&other);
            return attribute != nullptr && attribute->getCategoryProperty() == getCategoryProperty();
        }

        /** Returns a hash code based on the resolved category name. */
        [[nodiscard]] int GetHashCode() const override {
            return static_cast<int>(std::hash<std::string>{}(getCategoryProperty()));
        }

        /** Returns true when this attribute has the default category. */
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override {
            return getCategoryProperty() == getDefaultProperty().getCategoryProperty();
        }

    protected:
        /**
         * Resolves a localized form of a category name.
         * The base implementation has no ComponentModel resource catalogue and returns no override.
         */
        [[nodiscard]] virtual std::optional<std::string> GetLocalizedString(
            const std::string&) const {
            return std::nullopt;
        }
    };

    /** Specifies whether a property or event should be displayed in a property grid. */
    class BrowsableAttribute : public System::Attribute {
        bool browsable_;

    public:
        /** @param browsable True to show the member in property browsers. */
        explicit BrowsableAttribute(bool browsable) : browsable_(browsable) {}

        static const BrowsableAttribute Yes; ///< Pre-built "browsable = true" instance.
        static const BrowsableAttribute No;  ///< Pre-built "browsable = false" instance.
        static const BrowsableAttribute Default; ///< Pre-built default (browsable = true) instance.

        /** Gets whether the object is browsable. */
        [[nodiscard]] bool getBrowsableProperty() const noexcept { return browsable_; }

        /** Compares attributes by their browsable value. */
        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const BrowsableAttribute*>(&other);
            return attribute != nullptr && attribute->browsable_ == browsable_;
        }

        /** Returns a hash code for the browsable value. */
        [[nodiscard]] int GetHashCode() const override { return browsable_ ? 1 : 0; }

        /** Returns true when this attribute is equivalent to BrowsableAttribute::Default. */
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override { return Equals(Default); }
    };
    inline const BrowsableAttribute BrowsableAttribute::Yes{true};
    inline const BrowsableAttribute BrowsableAttribute::No{false};
    inline const BrowsableAttribute BrowsableAttribute::Default{true};

    // ------------------------------------------------------------------------------------------
    // #2403 -- THE SIX ATTRIBUTES BELOW USED TO PUBLISH A BARE MUTABLE DATA MEMBER.
    //
    // .NET declares every one of them `public sealed class` with a get-only property
    // (`public bool X { get; }`), the full Yes/No/Default static set, and value-based `Equals`
    // plus `IsDefaultAttribute`. THE PORT WAS INCONSISTENT WITH ITSELF ABOUT IT: `CategoryAttribute`,
    // `BrowsableAttribute`, `DisplayNameAttribute` and `DescriptionAttribute`, in this very header
    // and its neighbour, already had the correct shape.
    //
    // ONE DIVERGENCE IS DELIBERATE AND IS NOT A SLIP. .NET's `GetHashCode` for all six is
    // `base.GetHashCode()` -- IDENTITY -- while its `Equals` is VALUE-based, so two equal .NET
    // instances can hash differently. That is a hash-contract violation in the reference itself,
    // and this port does not reproduce it, for a reason that is written down rather than chosen:
    // `System/Attribute.hpp`'s own doc-comment states the house rule -- "A subclass that needs
    // value equality must override BOTH Equals and GetHashCode ... Overriding only one breaks the
    // equals/hashCode contract" -- and the four already-correct siblings use a value-based hash.
    // Reproducing .NET here would contradict this port's own stated rule and its own neighbours.
    // ------------------------------------------------------------------------------------------

    /** Specifies whether the property it is bound to is read-only. `ReadOnlyAttribute.cs:9-23`. */
    class ReadOnlyAttribute final : public System::Attribute {
        bool isReadOnly_;

    public:
        /** @param isReadOnly True to mark the property as read-only. */
        explicit ReadOnlyAttribute(bool isReadOnly) : isReadOnly_(isReadOnly) {}

        static const ReadOnlyAttribute Yes;     ///< Pre-built "read-only = true" instance.
        static const ReadOnlyAttribute No;      ///< Pre-built "read-only = false" instance.
        static const ReadOnlyAttribute Default; ///< `ReadOnlyAttribute.cs:14` -- Default is **No**.

        /** @return true if the property this attribute is bound to is read-only. */
        [[nodiscard]] bool getIsReadOnlyProperty() const noexcept { return isReadOnly_; }

        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const ReadOnlyAttribute*>(&other);
            return attribute != nullptr && attribute->isReadOnly_ == isReadOnly_;
        }
        [[nodiscard]] int GetHashCode() const override { return isReadOnly_ ? 1 : 0; }
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override { return Equals(Default); }
    };
    inline const ReadOnlyAttribute ReadOnlyAttribute::Yes{true};
    inline const ReadOnlyAttribute ReadOnlyAttribute::No{false};
    inline const ReadOnlyAttribute ReadOnlyAttribute::Default{false};

    /** Specifies the display name for a property or event. */
    class DisplayNameAttribute : public System::Attribute {
        std::string displayName_;
    public:
        /** Pre-built instance holding the default empty display name. */
        static const DisplayNameAttribute Default;

        /** Constructs the attribute with an empty display name. */
        DisplayNameAttribute() = default;

        /** Constructs the attribute with the given display name. */
        explicit DisplayNameAttribute(std::string displayName) : displayName_(std::move(displayName)) {}

        /** Gets the display name string. */
        [[nodiscard]] virtual const std::string& getDisplayNameProperty() const noexcept {
            return displayName_;
        }

        /** Compares attributes by their display names. */
        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const DisplayNameAttribute*>(&other);
            return attribute != nullptr && attribute->getDisplayNameProperty() == getDisplayNameProperty();
        }

        /** Returns a hash code based on the display name. */
        [[nodiscard]] int GetHashCode() const override {
            return static_cast<int>(std::hash<std::string>{}(getDisplayNameProperty()));
        }

        /** Returns true when this attribute has the empty default display name. */
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override { return Equals(Default); }

    protected:
        /** Gets the mutable display-name value for derived attributes. */
        [[nodiscard]] const std::string& getDisplayNameValueProperty() const noexcept {
            return displayName_;
        }

        /** Sets the display-name value for derived attributes. */
        void setDisplayNameValueProperty(std::string value) { displayName_ = std::move(value); }
    };
    inline const DisplayNameAttribute DisplayNameAttribute::Default{};

    /** Specifies that the decorated class is immutable. `ImmutableObjectAttribute.cs:9-30`. */
    class ImmutableObjectAttribute final : public System::Attribute {
        bool immutable_;

    public:
        /** @param immutable True to mark the object as immutable. */
        explicit ImmutableObjectAttribute(bool immutable) : immutable_(immutable) {}

        static const ImmutableObjectAttribute Yes;     ///< Pre-built "immutable = true" instance.
        static const ImmutableObjectAttribute No;      ///< Pre-built "immutable = false" instance.
        static const ImmutableObjectAttribute Default; ///< `:14` -- Default is **No**.

        /** @return true if the decorated object is immutable. */
        [[nodiscard]] bool getImmutableProperty() const noexcept { return immutable_; }

        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const ImmutableObjectAttribute*>(&other);
            return attribute != nullptr && attribute->immutable_ == immutable_;
        }
        [[nodiscard]] int GetHashCode() const override { return immutable_ ? 1 : 0; }
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override { return Equals(Default); }
    };
    inline const ImmutableObjectAttribute ImmutableObjectAttribute::Yes{true};
    inline const ImmutableObjectAttribute ImmutableObjectAttribute::No{false};
    inline const ImmutableObjectAttribute ImmutableObjectAttribute::Default{false};

    /** Specifies whether a property should be localised. `LocalizableAttribute.cs:9-30`. */
    class LocalizableAttribute final : public System::Attribute {
        bool isLocalizable_;

    public:
        /** @param localizable True to indicate the property is localizable. */
        explicit LocalizableAttribute(bool localizable) : isLocalizable_(localizable) {}

        static const LocalizableAttribute Yes;     ///< Pre-built "localizable = true" instance.
        static const LocalizableAttribute No;      ///< Pre-built "localizable = false" instance.
        static const LocalizableAttribute Default; ///< `:20` -- Default is **No**.

        /** @return true if the property value should be localised. */
        [[nodiscard]] bool getIsLocalizableProperty() const noexcept { return isLocalizable_; }

        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const LocalizableAttribute*>(&other);
            return attribute != nullptr && attribute->isLocalizable_ == isLocalizable_;
        }
        [[nodiscard]] int GetHashCode() const override { return isLocalizable_ ? 1 : 0; }
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override { return Equals(Default); }
    };
    inline const LocalizableAttribute LocalizableAttribute::Yes{true};
    inline const LocalizableAttribute LocalizableAttribute::No{false};
    inline const LocalizableAttribute LocalizableAttribute::Default{false};

    /**
     * @brief Specifies whether the list value of this property can be merged into a single object.
     * `MergablePropertyAttribute.cs:9-30`.
     *
     * @note **Its `Default` is `Yes`, not `No`** (`:12`), unlike the other four boolean attributes
     * in this header. That asymmetry is .NET's, and a repair that "harmonised" the five would be
     * wrong here and nowhere else -- which is why it carries a pin of its own.
     */
    class MergablePropertyAttribute final : public System::Attribute {
        bool allowMerge_;

    public:
        /** @param allowMerge True to allow merging. */
        explicit MergablePropertyAttribute(bool allowMerge) : allowMerge_(allowMerge) {}

        static const MergablePropertyAttribute Yes;     ///< Pre-built "allow merge = true".
        static const MergablePropertyAttribute No;      ///< Pre-built "allow merge = false".
        static const MergablePropertyAttribute Default; ///< `:12` -- Default is **Yes**.

        /** @return true if this property's list value can be merged. */
        [[nodiscard]] bool getAllowMergeProperty() const noexcept { return allowMerge_; }

        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const MergablePropertyAttribute*>(&other);
            return attribute != nullptr && attribute->allowMerge_ == allowMerge_;
        }
        [[nodiscard]] int GetHashCode() const override { return allowMerge_ ? 1 : 0; }
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override { return Equals(Default); }
    };
    inline const MergablePropertyAttribute MergablePropertyAttribute::Yes{true};
    inline const MergablePropertyAttribute MergablePropertyAttribute::No{false};
    inline const MergablePropertyAttribute MergablePropertyAttribute::Default{true};

    /**
     * @brief Specifies that changing this property should trigger re-querying the parent property.
     * `NotifyParentPropertyAttribute.cs:9-30`.
     */
    class NotifyParentPropertyAttribute final : public System::Attribute {
        bool notifyParent_;

    public:
        /** @param notifyParent True to enable parent notification. */
        explicit NotifyParentPropertyAttribute(bool notifyParent) : notifyParent_(notifyParent) {}

        static const NotifyParentPropertyAttribute Yes;     ///< Pre-built "notify = true".
        static const NotifyParentPropertyAttribute No;      ///< Pre-built "notify = false".
        static const NotifyParentPropertyAttribute Default; ///< `:14` -- Default is **No**.

        /** @return true if the parent property should be notified of a change. */
        [[nodiscard]] bool getNotifyParentProperty() const noexcept { return notifyParent_; }

        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const NotifyParentPropertyAttribute*>(&other);
            return attribute != nullptr && attribute->notifyParent_ == notifyParent_;
        }
        [[nodiscard]] int GetHashCode() const override { return notifyParent_ ? 1 : 0; }
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override { return Equals(Default); }
    };
    inline const NotifyParentPropertyAttribute NotifyParentPropertyAttribute::Yes{true};
    inline const NotifyParentPropertyAttribute NotifyParentPropertyAttribute::No{false};
    inline const NotifyParentPropertyAttribute NotifyParentPropertyAttribute::Default{false};

    /**
     * @brief Controls how the property grid refreshes. `RefreshProperties.cs:5-11`.
     *
     * @note **Top-level, not nested.** This port used to declare it as
     * `RefreshPropertiesAttribute::Refresh`, which differed from .NET in both the name and the
     * scope. .NET's is a top-level `public enum RefreshProperties` in its own file.
     */
    enum class RefreshProperties {
        None    = 0,
        All     = 1,
        Repaint = 2,
    };

    /**
     * @brief Specifies how the property grid refreshes when the decorated property changes.
     * `RefreshPropertiesAttribute.cs:6-20`.
     */
    class RefreshPropertiesAttribute final : public System::Attribute {
        RefreshProperties refreshProperties_;

    public:
        /** @param refresh The desired refresh mode. */
        explicit RefreshPropertiesAttribute(RefreshProperties refresh)
            : refreshProperties_(refresh) {}

        static const RefreshPropertiesAttribute All;     ///< `:8` -- RefreshProperties::All.
        static const RefreshPropertiesAttribute Repaint; ///< `:9` -- RefreshProperties::Repaint.
        static const RefreshPropertiesAttribute Default; ///< `:10` -- RefreshProperties::None.

        /** @return the refresh mode this attribute carries. */
        [[nodiscard]] RefreshProperties getRefreshPropertiesProperty() const noexcept {
            return refreshProperties_;
        }

        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const RefreshPropertiesAttribute*>(&other);
            return attribute != nullptr && attribute->refreshProperties_ == refreshProperties_;
        }
        [[nodiscard]] int GetHashCode() const override {
            return static_cast<int>(refreshProperties_);
        }
        [[nodiscard]] bool getIsDefaultAttributeProperty() const override { return Equals(Default); }
    };
    inline const RefreshPropertiesAttribute RefreshPropertiesAttribute::All{RefreshProperties::All};
    inline const RefreshPropertiesAttribute RefreshPropertiesAttribute::Repaint{RefreshProperties::Repaint};
    inline const RefreshPropertiesAttribute RefreshPropertiesAttribute::Default{RefreshProperties::None};

    /**
     * @brief Specifies the type converter type associated with an object.
     *
     * This metadata attribute is useful independently of the TypeConverter
     * runtime system.  The System::Type overload stores the RTTI full name;
     * assembly-qualified names are unavailable by the project's permanent
     * no-reflection deviation.
     */
    class TypeConverterAttribute final : public System::Attribute {
        std::string typeName_;
    public:
        /** A shared attribute with no converter type name. */
        static const TypeConverterAttribute Default;

        /** Constructs the attribute with an empty type name. */
        TypeConverterAttribute() = default;

        /** @param typeName Fully qualified name of the converter type. */
        explicit TypeConverterAttribute(std::string typeName) : typeName_(std::move(typeName)) {}

        /** @param type Converter type represented by the project's RTTI Type wrapper. */
        explicit TypeConverterAttribute(const System::Type& type)
            : typeName_(type.getFullNameProperty()) {}

        /** @return The fully qualified converter type name. */
        [[nodiscard]] const std::string& getConverterTypeNameProperty() const noexcept { return typeName_; }

        /** Compares attributes by converter type name. */
        [[nodiscard]] bool Equals(const System::Attribute& other) const override {
            const auto* attribute = dynamic_cast<const TypeConverterAttribute*>(&other);
            return attribute != nullptr && attribute->typeName_ == typeName_;
        }

        /** Returns a hash code based on the converter type name. */
        [[nodiscard]] int GetHashCode() const override {
            return static_cast<int>(std::hash<std::string>{}(typeName_));
        }
    };
    inline const TypeConverterAttribute TypeConverterAttribute::Default{};

    /** Specifies the designer for a class. */
    class DesignerAttribute : public System::Attribute {
        std::string designerTypeName_;
    public:
        /** @param designerTypeName Fully qualified name of the designer type. */
        explicit DesignerAttribute(const std::string& designerTypeName) : designerTypeName_(designerTypeName) {}

        /** @return The fully qualified designer type name. */
        [[nodiscard]] const std::string& getDesignerTypeNameProperty() const { return designerTypeName_; }
    };

    /** Specifies that the decorated class is usable only in design mode. */
    class DesignOnlyAttribute : public System::Attribute {
    public:
        bool IsDesignOnly; ///< True if the component is design-time only.

        /** @param designOnly True to restrict usage to design time. */
        explicit DesignOnlyAttribute(bool designOnly) : IsDesignOnly(designOnly) {}
    };

} // namespace System::ComponentModel
