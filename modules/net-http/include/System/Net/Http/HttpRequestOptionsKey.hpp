// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>
#include <utility>

namespace System::Net::Http {

/**
 * @brief Represents a strongly typed key in an HTTP request's options.
 * @tparam TValue Type of the option associated with the key.
 *
 * C++ counterpart of .NET System.Net.Http.HttpRequestOptionsKey&lt;TValue&gt;.
 */
template<typename TValue>
class HttpRequestOptionsKey {
public:
    /**
     * @brief Initializes the key with its name.
     * @param key Name of the HTTP request option.
     */
    explicit HttpRequestOptionsKey(std::string key) : key_(std::move(key)) {}

    /** Gets the name of the option. */
    [[nodiscard]] const std::string& getKeyProperty() const noexcept { return key_; }

private:
    std::string key_;
};

} // namespace System::Net::Http
