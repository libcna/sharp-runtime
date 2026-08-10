// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <any>
#include <string>
#include <unordered_map>
#include <utility>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/Net/Http/HttpRequestOptionsKey.hpp"

namespace System::Net::Http {

/**
 * @brief Represents a collection of options for an HTTP request.
 *
 * C++ counterpart of .NET System.Net.Http.HttpRequestOptions.  std::any is
 * the C++ equivalent of the underlying object-valued dictionary; typed keys
 * prevent callers from retrieving a value under an unexpected type.
 */
class HttpRequestOptions final {
public:
    /** Initializes an empty HTTP request option collection. */
    HttpRequestOptions() = default;

    /** Gets the number of stored options. */
    [[nodiscard]] SharpRuntime::intcs getCountProperty() const noexcept {
        return static_cast<SharpRuntime::intcs>(options_.size());
    }

    /** Returns whether an option with @p key exists. */
    [[nodiscard]] bool ContainsKey(const std::string& key) const {
        return options_.contains(key);
    }

    /** Removes the option with @p key, returning whether it existed. */
    bool Remove(const std::string& key) { return options_.erase(key) != 0; }

    /** Removes every option. */
    void Clear() noexcept { options_.clear(); }

    /**
     * @brief Adds an untyped option.
     * @throws System::ArgumentException when an option with the same key already exists.
     */
    void Add(std::string key, std::any value) {
        if (!options_.emplace(std::move(key), std::move(value)).second) {
            throw System::ArgumentException("An item with the same key has already been added.", "key");
        }
    }

    /** Stores or replaces an untyped option. */
    void Set(std::string key, std::any value) { options_.insert_or_assign(std::move(key), std::move(value)); }

    /** Retrieves an untyped option. A missing value resets @p value to empty. */
    [[nodiscard]] bool TryGetValue(const std::string& key, std::any& value) const {
        const auto it = options_.find(key);
        if (it == options_.end()) {
            value.reset();
            return false;
        }
        value = it->second;
        return true;
    }

    /** Stores or replaces the value associated with a strongly typed key. */
    template<typename TValue>
    void Set(const HttpRequestOptionsKey<TValue>& key, TValue value) {
        options_.insert_or_assign(key.getKeyProperty(), std::any(std::move(value)));
    }

    /**
     * @brief Gets the value associated with a strongly typed key.
     * @return True only when the key exists and its stored value has type TValue.
     *
     * On failure, @p value receives TValue{} just as the .NET out parameter
     * receives default(TValue).
     */
    template<typename TValue>
    [[nodiscard]] bool TryGetValue(const HttpRequestOptionsKey<TValue>& key, TValue& value) const {
        const auto it = options_.find(key.getKeyProperty());
        if (it != options_.end()) {
            if (const TValue* storedValue = std::any_cast<TValue>(&it->second)) {
                value = *storedValue;
                return true;
            }
        }
        value = TValue{};
        return false;
    }

    /** Gets the underlying option map for range-based read-only enumeration. */
    [[nodiscard]] const std::unordered_map<std::string, std::any>& getItemsProperty() const noexcept {
        return options_;
    }

private:
    std::unordered_map<std::string, std::any> options_;
};

} // namespace System::Net::Http
