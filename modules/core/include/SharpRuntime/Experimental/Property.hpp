// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#pragma once

/**
 * @brief Experimental property wrapper.
 * @note Status: Complete, but deliberately unused. The getter/setter delegation below is fully
 * implemented (not a stub) -- this type is deprioritized in favor of SharpRuntime::Prop.hpp,
 * which expands to ordinary members and methods without this type's per-instance
 * std::function-based indirection. Kept for reference/experimentation only.
 */
#include <functional>
#include <iostream>
#include <stdexcept>
#include "System/NotSupportedException.hpp"

#define DEF_PROP_AUTO(type, name, init) \
    private: type name##VVVV = init; \
    public: SharpRuntime::Experimental::Property<type> name;

#define IMPL_PROP_AUTO(type, name)\
name( [this]() { return name##VVVV; } , [this](type v) {name##VVVV = v; })

#define IMPL_PROP_AUTO_READONLY(type, name)\
name( [this]() { return name##VVVV; })

#define DEF_PROP_CUSTOM(type, name) \
SharpRuntime::Experimental::Property<type> name;

#define IMPL_PROP_CUSTOM(type, name, customGetter, customSetter)\
name( [this]() customGetter, [this](type v) customSetter)

#define IMPL_PROP_CUSTOM_READONLY(type, name, customGetter)\
name( [this]() customGetter )

namespace SharpRuntime::Experimental {
    // Template for Property
    /// Experimental property wrapper that delegates get/set to std::function objects.
    /// Prefer SharpRuntime::Prop.hpp in production code; this class adds per-instance overhead.
    ///
    /// @tparam T  The property's value type. It must be default-constructible: the
    ///            vestigial @c cachedValue member below is default-initialised by every
    ///            constructor. Nothing else in this class needs the requirement.
    template <typename T>
    class Property {
    public:
        /// @param customGetter  Lambda invoked on every read.
        /// @param customSetter  Lambda invoked on every write (nullptr = read-only).
        Property(std::function<T()> customGetter, std::function<void(const T&)> customSetter = nullptr)
            : getter(customGetter), setter(customSetter) {}

        /// Writes @p value via the custom setter.
        /// @return The value the property reads back afterwards, obtained from the custom
        ///         getter -- so a setter that transforms, clamps or ignores its argument is
        ///         reflected in the assignment expression's own result. Returning by value
        ///         (as @c std::atomic does) rather than by reference is deliberate: this
        ///         wrapper owns no storage a reference could refer to, and the getter is
        ///         invoked exactly once per assignment.
        /// @throws System::NotSupportedException if no setter was provided.
        T operator=(const T& value) {
            if (setter) {
                setter(value);
            } else {
                throw System::NotSupportedException("Setter not implemented.");
            }
            return getter();
        }

        /// Reads the current value via the custom getter.
        operator T() const {
            return getter();
        }
        /// @return The current value via the custom getter.
        T get() const {
            return getter();
        }
        /// Writes @p value via the custom setter. Throws System::NotSupportedException if no setter was provided.
        void set(T value) {
            if (setter) {
                setter(value);
            } else {
                throw System::NotSupportedException("Setter not implemented.");
            }
        }

    private:
        std::function<T()> getter;
        std::function<void(const T&)> setter;
        /// Vestigial. This member is never read and never written: the value a
        /// property holds lives in whatever storage the supplied getter/setter close
        /// over, so a cache here can only ever disagree with it (which is what
        /// SR-AUD-179 measured, ticket #2244). It is retained only because removing
        /// it changes @c sizeof(Property<T>) -- an object-layout change, which needs
        /// explicit approval; ticket #2246 carries that request. While it is here,
        /// @c T must be default-constructible.
        [[maybe_unused]] T cachedValue;
    };

}


//// Example usage
//class MyClass {
//public:
//    MyClass()
//        : ReadWriteValue(
//            // Getter
//            [this]() { return value; },
//            // Setter
//            [this](const int& v) { value = v; }),
//          ReadOnlyValue(
//            // Getter
//            [this]() { return readOnlyValue; }
//          )
//    {}
//
//    // Property: Read-Write
//    Property<int> ReadWriteValue;
//
//    // Property: Read-Only
//    Property<int> ReadOnlyValue;
//
//private:
//    int value = 0;
//    int readOnlyValue = 42; // Example of a read-only value
//};
//
//int main() {
//    MyClass obj;
//
//    // Read-Write Property
//    obj.ReadWriteValue = 100; // Sets the value using the custom setter
//    std::cout << "Read-Write Value: " << obj.ReadWriteValue << "\n";
//
//    // Read-Only Property
//    std::cout << "Read-Only Value: " << obj.ReadOnlyValue << "\n";
//
//    // Uncommenting the following line will throw an exception because it's read-only
//    // obj.ReadOnlyValue = 200;
//
//    return 0;
//}
