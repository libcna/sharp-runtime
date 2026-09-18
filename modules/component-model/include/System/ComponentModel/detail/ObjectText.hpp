// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <optional>
#include <string>
#include <string_view>
#include "System/Type.hpp"

namespace System::Globalization {
class CultureInfo;
}

namespace System::ComponentModel::detail {

/**
 * @brief What `object.ToString()` and `value is string` become over `std::any`.
 *
 * The converters take and return `std::any`, this runtime's `object`. Two questions come up in
 * every one of them: "is this a string?" and "what is this value's text?". .NET answers both
 * through the object itself; `std::any` cannot, so the answers live here, over the closed set of
 * types the runtime boxes as primitives.
 */
struct ObjectText {
    /**
     * @brief Reports whether a runtime type is one of the C++ representations accepted as a
     *        boxed .NET string.
     * @param type The runtime type.
     * @return true for std::string, string_view, and mutable or const character pointers.
     */
    [[nodiscard]] static bool IsStringType(const System::Type& type);

    /**
     * @brief Reads a string out of a boxed value.
     *
     * A `std::string`, `std::string_view`, `const char*` or `char*` all count as .NET's
     * `string`, so a literal passed straight to `ConvertFrom` behaves as the string it is.
     * @param value The boxed value.
     * @return The string, or `std::nullopt` when the value is not a string.
     */
    [[nodiscard]] static std::optional<std::string> AsString(const std::any& value);

    /**
     * @brief The full name of a boxed value's type, or `"(null)"` for an empty value.
     *
     * The spelling .NET's converter exceptions use (`SR.ToStringNull`).
     */
    [[nodiscard]] static std::string TypeName(const std::any& value);

    /**
     * @brief `object.ToString()` for a boxed value, with a culture for the formattable ones.
     *
     * A primitive is formatted the way its `System::X::ToString(value, provider)` formats it,
     * a string is itself, a bool is `"True"`/`"False"`, and any other value -- for which this
     * runtime has no virtual `ToString` to call -- yields its type's full name, which is what
     * `object.ToString()` prints for a type that does not override it.
     * @param value The boxed value.
     * @param culture The culture to format with, or null for the current culture.
     * @return The text.
     */
    [[nodiscard]] static std::string ToString(const std::any& value,
                                              const System::Globalization::CultureInfo* culture);
};

} // namespace System::ComponentModel::detail
