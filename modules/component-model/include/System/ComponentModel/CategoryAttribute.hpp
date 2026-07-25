// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include "System/Attribute.hpp"

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

    /** Specifies whether the property it is bound to is read-only. */
    class ReadOnlyAttribute : public System::Attribute {
    public:
        bool IsReadOnly; ///< True if the property is read-only.

        /** @param isReadOnly True to mark the property as read-only. */
        explicit ReadOnlyAttribute(bool isReadOnly) : IsReadOnly(isReadOnly) {}

        static const ReadOnlyAttribute Yes; ///< Pre-built "read-only = true" instance.
        static const ReadOnlyAttribute No;  ///< Pre-built "read-only = false" instance.
    };
    inline const ReadOnlyAttribute ReadOnlyAttribute::Yes{true};
    inline const ReadOnlyAttribute ReadOnlyAttribute::No{false};

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

    /** Specifies that the decorated class is immutable. */
    class ImmutableObjectAttribute : public System::Attribute {
    public:
        bool Immutable; ///< True if the object is immutable.

        /** @param immutable True to mark the object as immutable. */
        explicit ImmutableObjectAttribute(bool immutable) : Immutable(immutable) {}
    };

    /** Specifies whether a property should be localised. */
    class LocalizableAttribute : public System::Attribute {
    public:
        bool IsLocalizable; ///< True if the property value should be localised.

        /** @param localizable True to indicate the property is localizable. */
        explicit LocalizableAttribute(bool localizable) : IsLocalizable(localizable) {}
    };

    /** Specifies whether the list value of this property can be merged into a single object. */
    class MergablePropertyAttribute : public System::Attribute {
    public:
        bool AllowMerge; ///< True if property values can be merged.

        /** @param allowMerge True to allow merging. */
        explicit MergablePropertyAttribute(bool allowMerge) : AllowMerge(allowMerge) {}
    };

    /** Specifies that changing this property should trigger re-querying the parent property. */
    class NotifyParentPropertyAttribute : public System::Attribute {
    public:
        bool NotifyParent; ///< True if the parent property should be notified.

        /** @param notifyParent True to enable parent notification. */
        explicit NotifyParentPropertyAttribute(bool notifyParent) : NotifyParent(notifyParent) {}
    };

    /** Specifies how the property grid refreshes when the decorated property changes. */
    class RefreshPropertiesAttribute : public System::Attribute {
    public:
        /** Controls how the property grid refreshes. */
        enum class Refresh { None = 0, All = 1, Repaint = 2 };

        Refresh RefreshProperties_; ///< The refresh mode.

        /** @param r The desired refresh mode. */
        explicit RefreshPropertiesAttribute(Refresh r) : RefreshProperties_(r) {}
    };

    /** Specifies the type converter to use for a property. */
    class TypeConverterAttribute : public System::Attribute {
        std::string typeName_;
    public:
        /** Constructs the attribute with an empty type name. */
        TypeConverterAttribute() = default;

        /** @param typeName Fully qualified name of the converter type. */
        explicit TypeConverterAttribute(const std::string& typeName) : typeName_(typeName) {}

        /** @return The fully qualified converter type name. */
        [[nodiscard]] const std::string& getConverterTypeNameProperty() const { return typeName_; }
    };

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
