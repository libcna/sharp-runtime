#pragma once

#include <cstdint>
#include <cstddef>

namespace System
{
    /**
     * @brief A platform-specific type used to represent a pointer or a handle.
     *
     * C++ counterpart of the .NET System.IntPtr struct.
     * Holds a signed integer large enough to hold a pointer on the target platform.
     */
    struct IntPtr
    {
        intptr_t value = 0;

        IntPtr() = default;

        IntPtr(intptr_t v) : value(v) {}           // NOLINT(google-explicit-constructor)
        IntPtr(void* p) : value(reinterpret_cast<intptr_t>(p)) {}  // NOLINT(google-explicit-constructor)

        static const IntPtr Zero;

        [[nodiscard]] bool operator==(const IntPtr& other) const { return value == other.value; }
        [[nodiscard]] bool operator!=(const IntPtr& other) const { return value != other.value; }

        [[nodiscard]] explicit operator intptr_t() const { return value; }
        [[nodiscard]] explicit operator void*() const { return reinterpret_cast<void*>(value); }

        [[nodiscard]] bool IsZero() const { return value == 0; }
    };

    inline const IntPtr IntPtr::Zero{0};
}
