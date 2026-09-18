// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>
#include <utility>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ComponentModel/AttributeCollection.hpp"
#include "System/ComponentModel/BrowsableAttribute.hpp"
#include "System/ComponentModel/CategoryAttribute.hpp"
#include "System/ComponentModel/DescriptionAttribute.hpp"
#include "System/ComponentModel/DisplayNameAttribute.hpp"

namespace System::ComponentModel {

    /**
     * @brief Represents a class member, such as a property or event. This is an abstract base
     *        class.
     *
     * C++ counterpart of .NET System.ComponentModel.MemberDescriptor: a name plus the
     * attributes that describe the member, and the derived properties .NET reads from those
     * attributes (`Category`, `Description`, `DisplayName`, `IsBrowsable`, `DesignTimeOnly`).
     * In .NET the attributes come from reflection over the member; here the class that declares
     * a descriptor states them explicitly, and the default is none.
     */
    class MemberDescriptor {
    public:
        virtual ~MemberDescriptor() = default;

        /**
         * @brief Gets the collection of attributes for this member.
         *
         * C++ counterpart of .NET MemberDescriptor.Attributes.
         * @return The attributes, possibly empty.
         */
        [[nodiscard]] virtual const AttributeCollection& getAttributesProperty() const noexcept { return attributes_; }

        /**
         * @brief Gets the name of the category to which the member belongs, as specified in the
         *        CategoryAttribute.
         *
         * C++ counterpart of .NET MemberDescriptor.Category: the attribute's category, or the
         * default category when the member carries no CategoryAttribute.
         * @return The category name.
         */
        [[nodiscard]] virtual std::string getCategoryProperty() const {
            const auto* category = attributes_.getItem<CategoryAttribute>();
            return category != nullptr ? category->getCategoryProperty()
                                       : CategoryAttribute::getDefaultProperty().getCategoryProperty();
        }

        /**
         * @brief Gets the description of the member, as specified in the DescriptionAttribute.
         *
         * C++ counterpart of .NET MemberDescriptor.Description.
         * @return The description, or an empty string.
         */
        [[nodiscard]] virtual std::string getDescriptionProperty() const {
            const auto* description = attributes_.getItem<DescriptionAttribute>();
            return description != nullptr ? description->getDescriptionProperty() : std::string{};
        }

        /**
         * @brief Gets whether this member should be set only at design time, as specified in
         *        the DesignOnlyAttribute.
         *
         * C++ counterpart of .NET MemberDescriptor.DesignTimeOnly.
         * @return true if the member is design-time only; false otherwise.
         */
        [[nodiscard]] virtual bool getDesignTimeOnlyProperty() const {
            const auto* designOnly = attributes_.getItem<DesignOnlyAttribute>();
            return designOnly != nullptr && designOnly->IsDesignOnly;
        }

        /**
         * @brief Gets the name that can be displayed in a window, such as a Properties window.
         *
         * C++ counterpart of .NET MemberDescriptor.DisplayName: the DisplayNameAttribute's
         * value when the member carries a non-default one, otherwise the member name.
         * @return The display name.
         */
        [[nodiscard]] virtual std::string getDisplayNameProperty() const {
            const auto* displayName = attributes_.getItem<DisplayNameAttribute>();
            if (displayName != nullptr && !displayName->getIsDefaultAttributeProperty()) {
                return displayName->getDisplayNameProperty();
            }
            return name_;
        }

        /**
         * @brief Gets a value indicating whether the member is browsable, as specified in the
         *        BrowsableAttribute.
         *
         * C++ counterpart of .NET MemberDescriptor.IsBrowsable.
         * @return true unless a BrowsableAttribute says otherwise.
         */
        [[nodiscard]] virtual bool getIsBrowsableProperty() const {
            const auto* browsable = attributes_.getItem<BrowsableAttribute>();
            return browsable == nullptr || browsable->getBrowsableProperty();
        }

        /**
         * @brief Gets the name of the member.
         *
         * C++ counterpart of .NET MemberDescriptor.Name.
         * @return The member name.
         */
        [[nodiscard]] virtual const std::string& getNameProperty() const noexcept { return name_; }

        /**
         * @brief Compares this instance to the given object to see if they are equivalent.
         *
         * C++ counterpart of .NET MemberDescriptor.Equals(object): same name, category and
         * description, and the same attributes in the same order.
         * @param other The descriptor to compare with.
         * @return true if the two descriptors are equivalent.
         */
        [[nodiscard]] virtual bool Equals(const MemberDescriptor& other) const {
            if (this == &other) return true;
            if (name_ != other.name_ || getCategoryProperty() != other.getCategoryProperty() ||
                getDescriptionProperty() != other.getDescriptionProperty() ||
                attributes_.getCountProperty() != other.attributes_.getCountProperty()) {
                return false;
            }
            for (SharpRuntime::intcs i = 0; i < attributes_.getCountProperty(); ++i) {
                if (!attributes_.getItem(i).Equals(other.attributes_.getItem(i))) return false;
            }
            return true;
        }

        /**
         * @brief Returns the hash code for this instance.
         *
         * C++ counterpart of .NET MemberDescriptor.GetHashCode(): the name's hash.
         * @return The hash code.
         */
        [[nodiscard]] virtual int GetHashCode() const { return getNameHashCodeProperty(); }

    protected:
        /**
         * @brief Initializes a new instance with the specified name and attributes.
         *
         * C++ counterpart of .NET MemberDescriptor(string, Attribute[]).
         * @param name The member name; must not be empty.
         * @param attributes The attributes that describe the member.
         * @throws System::ArgumentException if @p name is empty.
         */
        explicit MemberDescriptor(std::string name, AttributeCollection attributes = {})
            : name_(std::move(name)), attributes_(std::move(attributes)) {
            if (name_.empty()) throw System::ArgumentException("Invalid member name.");
        }

        /**
         * @brief Initializes a new instance with the name and attributes of another descriptor.
         *
         * C++ counterpart of .NET MemberDescriptor(MemberDescriptor).
         * @param other The descriptor to copy the name and attributes from.
         */
        MemberDescriptor(const MemberDescriptor& other) = default;

        /**
         * @brief Initializes a new instance with another descriptor's name and the given
         *        attributes merged over its own.
         *
         * C++ counterpart of .NET MemberDescriptor(MemberDescriptor, Attribute[]).
         * @param other The descriptor to copy the name and base attributes from.
         * @param newAttributes The attributes that replace or extend the copied ones.
         */
        MemberDescriptor(const MemberDescriptor& other, const std::vector<AttributeCollection::Element>& newAttributes)
            : name_(other.name_), attributes_(AttributeCollection::FromExisting(other.attributes_, newAttributes)) {}

        /** @brief Copies another descriptor's name and attributes. @param other The descriptor to copy. @return This descriptor. */
        MemberDescriptor& operator=(const MemberDescriptor& other) = default;

        /**
         * @brief Gets the hash code for the name of the member.
         *
         * C++ counterpart of .NET MemberDescriptor.NameHashCode.
         * @return The name's hash.
         */
        [[nodiscard]] int getNameHashCodeProperty() const {
            return static_cast<int>(std::hash<std::string>{}(name_));
        }

    private:
        std::string name_;
        AttributeCollection attributes_;
    };

} // namespace System::ComponentModel
