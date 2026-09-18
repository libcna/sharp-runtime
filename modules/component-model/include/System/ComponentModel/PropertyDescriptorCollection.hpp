// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ComponentModel/PropertyDescriptor.hpp"

namespace System::ComponentModel {

    /**
     * @brief Represents a collection of PropertyDescriptor objects.
     *
     * C++ counterpart of .NET System.ComponentModel.PropertyDescriptorCollection: an ordered,
     * name-addressable list of descriptors with the sorts a designer asks for. Descriptors are
     * polymorphic and typically shared between the converter that built them and every caller
     * that enumerates them, so the elements are `std::shared_ptr<PropertyDescriptor>`; copying a
     * collection copies the list and shares the descriptors, which is the ownership .NET's
     * reference semantics give.
     *
     * A read-only collection refuses every mutating member with `NotSupportedException`, as
     * .NET's does; `Sort` never mutates and always returns a new, writable collection.
     */
    class PropertyDescriptorCollection {
    public:
        /** @brief The element type: a shared descriptor. */
        using Element = std::shared_ptr<PropertyDescriptor>;

        /** @brief Orders two descriptors: negative, zero or positive like `IComparer.Compare`. */
        using Comparer = std::function<SharpRuntime::intcs(const PropertyDescriptor&, const PropertyDescriptor&)>;

        /** @brief An empty, read-only collection. C++ counterpart of .NET PropertyDescriptorCollection.Empty. */
        static const PropertyDescriptorCollection Empty;

        /** @brief Creates an empty, writable collection. */
        PropertyDescriptorCollection() = default;

        /**
         * @brief Initializes the collection with the given descriptors, in order.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection(PropertyDescriptor[], bool).
         * @param properties The descriptors; null entries are dropped.
         * @param readOnly Whether the collection refuses mutation.
         */
        explicit PropertyDescriptorCollection(std::vector<Element> properties, bool readOnly = false);

        /**
         * @brief Initializes the collection from a brace list of descriptors.
         * @param properties The descriptors; null entries are dropped.
         */
        PropertyDescriptorCollection(std::initializer_list<Element> properties);

        /**
         * @brief Gets the number of property descriptors in the collection.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Count.
         * @return The number of descriptors.
         */
        [[nodiscard]] SharpRuntime::intcs getCountProperty() const noexcept {
            return static_cast<SharpRuntime::intcs>(properties_.size());
        }

        /**
         * @brief Gets whether the collection is read-only.
         *
         * C++ counterpart of .NET's explicit `IList.IsReadOnly`.
         * @return true if mutation throws.
         */
        [[nodiscard]] bool getIsReadOnlyProperty() const noexcept { return readOnly_; }

        /**
         * @brief Gets the property with the specified index number.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.this[int].
         * @param index The zero-based index.
         * @return The descriptor.
         * @throws System::IndexOutOfRangeException if @p index is out of range.
         */
        [[nodiscard]] const Element& getItem(SharpRuntime::intcs index) const;

        /**
         * @brief Gets the property with the specified name.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.this[string]: a case-sensitive
         * `Find`.
         * @param name The property name.
         * @return The descriptor, or null when no property has that name.
         */
        [[nodiscard]] Element getItem(const std::string& name) const { return Find(name, false); }

        /**
         * @brief Adds the specified PropertyDescriptor to the collection.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Add(PropertyDescriptor).
         * @param value The descriptor to add.
         * @return The index at which it was added.
         * @throws System::NotSupportedException if the collection is read-only.
         */
        SharpRuntime::intcs Add(Element value);

        /**
         * @brief Removes all PropertyDescriptor objects from the collection.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Clear().
         * @throws System::NotSupportedException if the collection is read-only.
         */
        void Clear();

        /**
         * @brief Returns whether the collection contains the given PropertyDescriptor.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Contains(PropertyDescriptor):
         * membership by `PropertyDescriptor::Equals`.
         * @param value The descriptor to find.
         * @return true if an equal descriptor is present.
         */
        [[nodiscard]] bool Contains(const PropertyDescriptor& value) const { return IndexOf(value) >= 0; }

        /**
         * @brief Returns the PropertyDescriptor with the specified name, using a Boolean to
         *        indicate whether to ignore case.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Find(string, bool).
         * @param name The property name.
         * @param ignoreCase Whether the comparison is ASCII case-insensitive.
         * @return The descriptor, or null when none matches.
         */
        [[nodiscard]] Element Find(const std::string& name, bool ignoreCase) const;

        /**
         * @brief Returns the index of the given PropertyDescriptor.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.IndexOf(PropertyDescriptor).
         * @param value The descriptor to find.
         * @return The index, or -1.
         */
        [[nodiscard]] SharpRuntime::intcs IndexOf(const PropertyDescriptor& value) const;

        /**
         * @brief Adds the PropertyDescriptor to the collection at the specified index number.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Insert(int, PropertyDescriptor).
         * @param index The zero-based index to insert at.
         * @param value The descriptor to insert.
         * @throws System::NotSupportedException if the collection is read-only.
         * @throws System::ArgumentOutOfRangeException if @p index is out of range.
         */
        void Insert(SharpRuntime::intcs index, Element value);

        /**
         * @brief Removes the specified PropertyDescriptor from the collection.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Remove(PropertyDescriptor).
         * @param value The descriptor to remove; absence is not an error.
         * @throws System::NotSupportedException if the collection is read-only.
         */
        void Remove(const PropertyDescriptor& value);

        /**
         * @brief Removes the PropertyDescriptor at the specified index from the collection.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.RemoveAt(int).
         * @param index The zero-based index.
         * @throws System::NotSupportedException if the collection is read-only.
         * @throws System::ArgumentOutOfRangeException if @p index is out of range.
         */
        void RemoveAt(SharpRuntime::intcs index);

        /**
         * @brief Sorts the members of this collection, using the default sort for this
         *        collection, which is usually alphabetical.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Sort().
         * @return A new collection with the sorted descriptors.
         */
        [[nodiscard]] PropertyDescriptorCollection Sort() const;

        /**
         * @brief Sorts the members of this collection. The specified order is applied first,
         *        followed by the default sort for this collection, which is usually alphabetical.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Sort(string[]): the named
         * properties come first in the given order, the rest follow sorted by name.
         * @param names The property names, in the order wanted.
         * @return A new collection with the sorted descriptors.
         */
        [[nodiscard]] PropertyDescriptorCollection Sort(const std::vector<std::string>& names) const;

        /**
         * @brief Sorts the members of this collection, using the specified IComparer.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Sort(IComparer).
         * @param comparer The ordering; an empty function means the default (by name).
         * @return A new collection with the sorted descriptors.
         */
        [[nodiscard]] PropertyDescriptorCollection Sort(const Comparer& comparer) const;

        /**
         * @brief Sorts the members of this collection. The specified order is applied first,
         *        followed by the sort using the specified IComparer.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.Sort(string[], IComparer).
         * @param names The property names, in the order wanted.
         * @param comparer The ordering for the rest; an empty function means by name.
         * @return A new collection with the sorted descriptors.
         */
        [[nodiscard]] PropertyDescriptorCollection Sort(const std::vector<std::string>& names, const Comparer& comparer) const;

        /** @brief Iteration support: the first descriptor. */
        [[nodiscard]] std::vector<Element>::const_iterator begin() const noexcept { return properties_.begin(); }

        /** @brief Iteration support: one past the last descriptor. */
        [[nodiscard]] std::vector<Element>::const_iterator end() const noexcept { return properties_.end(); }

    protected:
        /**
         * @brief Sorts the members of this collection. The specified order is applied first,
         *        followed by the default sort for this collection.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.InternalSort(string[]).
         * @param names The property names, in the order wanted.
         */
        void InternalSort(const std::vector<std::string>& names);

        /**
         * @brief Sorts the members of this collection using the specified IComparer.
         *
         * C++ counterpart of .NET PropertyDescriptorCollection.InternalSort(IComparer).
         * @param comparer The ordering; an empty function means by name.
         */
        void InternalSort(const Comparer& comparer);

    private:
        std::vector<Element> properties_;
        bool readOnly_ = false;

        void VerifyWritable() const;
    };

} // namespace System::ComponentModel
