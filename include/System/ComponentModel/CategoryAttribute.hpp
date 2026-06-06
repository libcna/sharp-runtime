// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::ComponentModel {

    class CategoryAttribute : public System::Attribute {
        std::string category_;
    public:
        CategoryAttribute() : category_("Misc") {}
        explicit CategoryAttribute(const std::string& category) : category_(category) {}
        [[nodiscard]] const std::string& getCategoryProperty() const { return category_; }
    };

    class BrowsableAttribute : public System::Attribute {
    public:
        bool Browsable;
        explicit BrowsableAttribute(bool browsable) : Browsable(browsable) {}
        static const BrowsableAttribute Yes;
        static const BrowsableAttribute No;
    };
    inline const BrowsableAttribute BrowsableAttribute::Yes{true};
    inline const BrowsableAttribute BrowsableAttribute::No{false};

    class ReadOnlyAttribute : public System::Attribute {
    public:
        bool IsReadOnly;
        explicit ReadOnlyAttribute(bool isReadOnly) : IsReadOnly(isReadOnly) {}
        static const ReadOnlyAttribute Yes;
        static const ReadOnlyAttribute No;
    };
    inline const ReadOnlyAttribute ReadOnlyAttribute::Yes{true};
    inline const ReadOnlyAttribute ReadOnlyAttribute::No{false};

    class DisplayNameAttribute : public System::Attribute {
        std::string displayName_;
    public:
        DisplayNameAttribute() = default;
        explicit DisplayNameAttribute(const std::string& displayName) : displayName_(displayName) {}
        [[nodiscard]] const std::string& getDisplayNameProperty() const { return displayName_; }
    };

    class ImmutableObjectAttribute : public System::Attribute {
    public:
        bool Immutable;
        explicit ImmutableObjectAttribute(bool immutable) : Immutable(immutable) {}
    };

    class LocalizableAttribute : public System::Attribute {
    public:
        bool IsLocalizable;
        explicit LocalizableAttribute(bool localizable) : IsLocalizable(localizable) {}
    };

    class MergablePropertyAttribute : public System::Attribute {
    public:
        bool AllowMerge;
        explicit MergablePropertyAttribute(bool allowMerge) : AllowMerge(allowMerge) {}
    };

    class NotifyParentPropertyAttribute : public System::Attribute {
    public:
        bool NotifyParent;
        explicit NotifyParentPropertyAttribute(bool notifyParent) : NotifyParent(notifyParent) {}
    };

    class RefreshPropertiesAttribute : public System::Attribute {
    public:
        enum class Refresh { None = 0, All = 1, Repaint = 2 };
        Refresh RefreshProperties_;
        explicit RefreshPropertiesAttribute(Refresh r) : RefreshProperties_(r) {}
    };

    class TypeConverterAttribute : public System::Attribute {
        std::string typeName_;
    public:
        TypeConverterAttribute() = default;
        explicit TypeConverterAttribute(const std::string& typeName) : typeName_(typeName) {}
        [[nodiscard]] const std::string& getConverterTypeNameProperty() const { return typeName_; }
    };

    class DesignerAttribute : public System::Attribute {
        std::string designerTypeName_;
    public:
        explicit DesignerAttribute(const std::string& designerTypeName) : designerTypeName_(designerTypeName) {}
        [[nodiscard]] const std::string& getDesignerTypeNameProperty() const { return designerTypeName_; }
    };

    class DesignOnlyAttribute : public System::Attribute {
    public:
        bool IsDesignOnly;
        explicit DesignOnlyAttribute(bool designOnly) : IsDesignOnly(designOnly) {}
    };

} // namespace System::ComponentModel
