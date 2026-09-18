// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <typeinfo>
#include <utility>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Attribute.hpp"

namespace System::ComponentModel {

    /**
     * @brief Represents a collection of attributes.
     *
     * C++ counterpart of .NET System.ComponentModel.AttributeCollection: an immutable, ordered
     * set of attributes that a `MemberDescriptor` or a registered type carries. Attributes are
     * polymorphic, so the collection holds them by `std::shared_ptr<const Attribute>`; copying a
     * collection shares the attributes, which are never mutated.
     *
     * One .NET behaviour is not reproduced: the type indexer, when the attribute is absent,
     * returns the attribute type's static `Default` member found by reflection. Here it returns
     * null, and a caller that wants the default asks the attribute type for it.
     */
    class AttributeCollection {
    public:
        /** @brief An empty collection. C++ counterpart of .NET AttributeCollection.Empty. */
        static const AttributeCollection Empty;

        /** @brief The element type: a shared, immutable attribute. */
        using Element = std::shared_ptr<const System::Attribute>;

        /** @brief Creates an empty collection. */
        AttributeCollection() = default;

        /**
         * @brief Initializes the collection with the given attributes, in order.
         *
         * C++ counterpart of .NET AttributeCollection(Attribute[]). Null elements are dropped,
         * matching .NET's `ArgumentNullException`-free handling of a sparse array.
         * @param attributes The attributes.
         */
        explicit AttributeCollection(std::vector<Element> attributes) : attributes_(std::move(attributes)) {
            std::erase_if(attributes_, [](const Element& element) { return element == nullptr; });
        }

        /**
         * @brief Creates a collection from an existing one plus new attributes; a new attribute
         *        replaces an existing one of the same type.
         *
         * C++ counterpart of .NET AttributeCollection.FromExisting(AttributeCollection, Attribute[]).
         * @param existing The attributes to start from.
         * @param newAttributes The attributes to add or replace.
         * @return The combined collection.
         */
        [[nodiscard]] static AttributeCollection FromExisting(const AttributeCollection& existing,
                                                              const std::vector<Element>& newAttributes) {
            std::vector<Element> combined = existing.attributes_;
            for (const Element& added : newAttributes) {
                if (added == nullptr) continue;
                bool replaced = false;
                for (Element& current : combined) {
                    if (current->getTypeIdProperty() == added->getTypeIdProperty()) {
                        current = added;
                        replaced = true;
                        break;
                    }
                }
                if (!replaced) combined.push_back(added);
            }
            return AttributeCollection(std::move(combined));
        }

        /**
         * @brief Gets the number of attributes.
         *
         * C++ counterpart of .NET AttributeCollection.Count.
         * @return The number of attributes.
         */
        [[nodiscard]] SharpRuntime::intcs getCountProperty() const noexcept {
            return static_cast<SharpRuntime::intcs>(attributes_.size());
        }

        /**
         * @brief Gets the attribute with the specified index number.
         *
         * C++ counterpart of .NET AttributeCollection.this[int].
         * @param index The zero-based index.
         * @return The attribute.
         * @throws System::ArgumentOutOfRangeException if @p index is out of range.
         */
        [[nodiscard]] const System::Attribute& getItem(SharpRuntime::intcs index) const {
            if (index < 0 || static_cast<std::size_t>(index) >= attributes_.size()) {
                throw System::ArgumentOutOfRangeException("index");
            }
            return *attributes_[static_cast<std::size_t>(index)];
        }

        /**
         * @brief Gets the attribute with the specified type.
         *
         * C++ counterpart of .NET AttributeCollection.this[Type]; see the class comment for the
         * one difference (null instead of the type's `Default`).
         * @param attributeType The attribute's dynamic type.
         * @return The first attribute of that exact type, or null.
         */
        [[nodiscard]] const System::Attribute* getItem(const std::type_info& attributeType) const noexcept {
            for (const Element& attribute : attributes_) {
                if (attribute->getTypeIdProperty() == attributeType) return attribute.get();
            }
            return nullptr;
        }

        /**
         * @brief Gets the attribute of type @p TAttribute, typed.
         *
         * The C++ spelling of `attributes[typeof(TAttribute)]`.
         * @tparam TAttribute The attribute class.
         * @return The attribute, or null when absent.
         */
        template<class TAttribute>
        [[nodiscard]] const TAttribute* getItem() const noexcept {
            return dynamic_cast<const TAttribute*>(getItem(typeid(TAttribute)));
        }

        /**
         * @brief Determines whether this collection contains the specified attribute or an
         *        equal one of the same type.
         *
         * C++ counterpart of .NET AttributeCollection.Contains(Attribute).
         * @param attribute The attribute to find.
         * @return true if an attribute of the same type compares equal to @p attribute.
         */
        [[nodiscard]] bool Contains(const System::Attribute& attribute) const {
            const System::Attribute* found = getItem(attribute.getTypeIdProperty());
            return found != nullptr && found->Equals(attribute);
        }

        /**
         * @brief Determines whether this collection contains all the specified attributes.
         *
         * C++ counterpart of .NET AttributeCollection.Contains(Attribute[]).
         * @param attributes The attributes to find.
         * @return true if every attribute is contained.
         */
        [[nodiscard]] bool Contains(const AttributeCollection& attributes) const {
            for (const Element& attribute : attributes.attributes_) {
                if (!Contains(*attribute)) return false;
            }
            return true;
        }

        /**
         * @brief Determines whether a specified attribute is the same as an attribute in the
         *        collection, using `Attribute::Match`.
         *
         * C++ counterpart of .NET AttributeCollection.Matches(Attribute).
         * @param attribute The attribute to match.
         * @return true if any attribute matches.
         */
        [[nodiscard]] bool Matches(const System::Attribute& attribute) const {
            for (const Element& candidate : attributes_) {
                if (candidate->Match(attribute)) return true;
            }
            return false;
        }

        /**
         * @brief Determines whether the attributes in the specified collection are the same as
         *        attributes in this collection.
         *
         * C++ counterpart of .NET AttributeCollection.Matches(Attribute[]).
         * @param attributes The attributes to match.
         * @return true if every attribute matches.
         */
        [[nodiscard]] bool Matches(const AttributeCollection& attributes) const {
            for (const Element& attribute : attributes.attributes_) {
                if (!Matches(*attribute)) return false;
            }
            return true;
        }

        /** @brief Iteration support: the first attribute. */
        [[nodiscard]] std::vector<Element>::const_iterator begin() const noexcept { return attributes_.begin(); }

        /** @brief Iteration support: one past the last attribute. */
        [[nodiscard]] std::vector<Element>::const_iterator end() const noexcept { return attributes_.end(); }

    private:
        std::vector<Element> attributes_;
    };

    inline const AttributeCollection AttributeCollection::Empty{};

} // namespace System::ComponentModel
