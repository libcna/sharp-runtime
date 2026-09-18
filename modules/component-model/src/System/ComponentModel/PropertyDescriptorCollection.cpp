// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ComponentModel/PropertyDescriptorCollection.hpp"

#include <algorithm>
#include <cctype>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/NotSupportedException.hpp"

namespace System::ComponentModel {

namespace {

bool equalsIgnoreCaseAscii(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// TypeDescriptor.SortDescriptorArray's ordering: by Name, with the invariant culture's
// comparison, which for the ASCII names a property has is ordinal.
SharpRuntime::intcs compareByName(const PropertyDescriptor& left, const PropertyDescriptor& right) {
    return static_cast<SharpRuntime::intcs>(left.getNameProperty().compare(right.getNameProperty()));
}

} // namespace

const PropertyDescriptorCollection PropertyDescriptorCollection::Empty{std::vector<Element>{}, true};

PropertyDescriptorCollection::PropertyDescriptorCollection(std::vector<Element> properties, bool readOnly)
    : properties_(std::move(properties)), readOnly_(readOnly) {
    std::erase_if(properties_, [](const Element& element) { return element == nullptr; });
}

PropertyDescriptorCollection::PropertyDescriptorCollection(std::initializer_list<Element> properties)
    : PropertyDescriptorCollection(std::vector<Element>(properties), false) {}

const PropertyDescriptorCollection::Element& PropertyDescriptorCollection::getItem(SharpRuntime::intcs index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= properties_.size()) {
        throw System::IndexOutOfRangeException();
    }
    return properties_[static_cast<std::size_t>(index)];
}

SharpRuntime::intcs PropertyDescriptorCollection::Add(Element value) {
    VerifyWritable();
    if (value == nullptr) return getCountProperty() - 1;
    properties_.push_back(std::move(value));
    return getCountProperty() - 1;
}

void PropertyDescriptorCollection::Clear() {
    VerifyWritable();
    properties_.clear();
}

PropertyDescriptorCollection::Element PropertyDescriptorCollection::Find(const std::string& name, bool ignoreCase) const {
    for (const Element& property : properties_) {
        if (ignoreCase ? equalsIgnoreCaseAscii(property->getNameProperty(), name)
                       : property->getNameProperty() == name) {
            return property;
        }
    }
    return nullptr;
}

SharpRuntime::intcs PropertyDescriptorCollection::IndexOf(const PropertyDescriptor& value) const {
    for (std::size_t i = 0; i < properties_.size(); ++i) {
        if (properties_[i].get() == &value || properties_[i]->Equals(value)) {
            return static_cast<SharpRuntime::intcs>(i);
        }
    }
    return -1;
}

void PropertyDescriptorCollection::Insert(SharpRuntime::intcs index, Element value) {
    VerifyWritable();
    if (index < 0 || static_cast<std::size_t>(index) > properties_.size()) {
        throw System::ArgumentOutOfRangeException("index");
    }
    if (value == nullptr) return;
    properties_.insert(properties_.begin() + index, std::move(value));
}

void PropertyDescriptorCollection::Remove(const PropertyDescriptor& value) {
    VerifyWritable();
    const SharpRuntime::intcs index = IndexOf(value);
    if (index >= 0) properties_.erase(properties_.begin() + index);
}

void PropertyDescriptorCollection::RemoveAt(SharpRuntime::intcs index) {
    VerifyWritable();
    if (index < 0 || static_cast<std::size_t>(index) >= properties_.size()) {
        throw System::ArgumentOutOfRangeException("index");
    }
    properties_.erase(properties_.begin() + index);
}

PropertyDescriptorCollection PropertyDescriptorCollection::Sort() const {
    PropertyDescriptorCollection sorted(properties_, false);
    sorted.InternalSort(Comparer{});
    return sorted;
}

PropertyDescriptorCollection PropertyDescriptorCollection::Sort(const std::vector<std::string>& names) const {
    PropertyDescriptorCollection sorted(properties_, false);
    sorted.InternalSort(names);
    return sorted;
}

PropertyDescriptorCollection PropertyDescriptorCollection::Sort(const Comparer& comparer) const {
    PropertyDescriptorCollection sorted(properties_, false);
    sorted.InternalSort(comparer);
    return sorted;
}

PropertyDescriptorCollection PropertyDescriptorCollection::Sort(const std::vector<std::string>& names,
                                                                const Comparer& comparer) const {
    PropertyDescriptorCollection sorted(properties_, false);
    sorted.InternalSort(comparer);
    sorted.InternalSort(names);
    return sorted;
}

void PropertyDescriptorCollection::InternalSort(const std::vector<std::string>& names) {
    // PropertyDescriptorCollection.InternalSort(string[]): the default sort first, then every
    // named property is moved to the front in the order the names give, and the rest keep
    // their sorted order after them.
    if (properties_.empty()) return;
    InternalSort(Comparer{});
    if (names.empty()) return;
    std::vector<Element> ordered;
    ordered.reserve(properties_.size());
    std::vector<Element> remaining = properties_;
    for (const std::string& name : names) {
        for (Element& candidate : remaining) {
            if (candidate != nullptr && candidate->getNameProperty() == name) {
                ordered.push_back(candidate);
                candidate = nullptr;
                break;
            }
        }
    }
    for (Element& candidate : remaining) {
        if (candidate != nullptr) ordered.push_back(std::move(candidate));
    }
    properties_ = std::move(ordered);
}

void PropertyDescriptorCollection::InternalSort(const Comparer& comparer) {
    const auto less = [&comparer](const Element& left, const Element& right) {
        return (comparer ? comparer(*left, *right) : compareByName(*left, *right)) < 0;
    };
    std::stable_sort(properties_.begin(), properties_.end(), less);
}

void PropertyDescriptorCollection::VerifyWritable() const {
    if (readOnly_) throw System::NotSupportedException();
}

} // namespace System::ComponentModel
