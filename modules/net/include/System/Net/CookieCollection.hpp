// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/Cookie.hpp"

namespace System::Net {

    using SharpRuntime::intcs;

    /**
     * @brief Provides a collection container for instances of the Cookie class.
     *
     * C++ counterpart of .NET System.Net.CookieCollection. A thin ordered container --
     * real .NET's version-negotiation and dedup-by-identity semantics are simplified to a
     * flat, insertion-ordered `std::vector<Cookie>`.
     */
    class CookieCollection {
    public:
        CookieCollection() = default;

        /** @brief Adds a cookie to the collection. */
        void Add(const Cookie& cookie) { cookies_.push_back(cookie); }

        /** @brief Adds every cookie from another collection. */
        void Add(const CookieCollection& other) {
            cookies_.insert(cookies_.end(), other.cookies_.begin(), other.cookies_.end());
        }

        /** @brief Gets the number of cookies contained in the collection. */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(cookies_.size()); }

        /** @brief Gets whether the collection is empty. */
        [[nodiscard]] bool getIsEmptyProperty() const { return cookies_.empty(); }

        /**
         * @brief Indexer: gets the cookie at @p index.
         * @throws System::ArgumentOutOfRangeException if @p index is negative or not less than Count.
         */
        [[nodiscard]] Cookie&       operator[](intcs index)       { return cookies_[validatedIndex(index)]; }
        /**
         * @brief Indexer: gets the cookie at @p index.
         * @throws System::ArgumentOutOfRangeException if @p index is negative or not less than Count.
         */
        [[nodiscard]] const Cookie& operator[](intcs index) const { return cookies_[validatedIndex(index)]; }

        [[nodiscard]] std::vector<Cookie>::iterator       begin()       { return cookies_.begin(); }
        [[nodiscard]] std::vector<Cookie>::iterator       end()         { return cookies_.end(); }
        [[nodiscard]] std::vector<Cookie>::const_iterator begin() const { return cookies_.begin(); }
        [[nodiscard]] std::vector<Cookie>::const_iterator end()   const { return cookies_.end(); }

    private:
        /**
         * @brief Converts a public signed index into a container subscript, rejecting every
         * value outside `[0, Count)`.
         *
         * Both indexers previously wrote `cookies_[static_cast<size_t>(index)]`, which is
         * undefined behaviour for any index the caller got wrong: a negative index becomes a
         * huge unsigned subscript and `index >= Count` walks off the end of the allocation.
         * Measured before the guard existed (`build-probe/2041_probe1_before.log`), the two
         * halves did not even fail the same way -- without a sanitizer `collection[-1]`
         * *returned normally* with a garbage Cookie whose `Name` printed as `(null)`, while
         * `collection[Count]` died with SIGSEGV, and under ASan nine of eleven probed indexes
         * reported a `heap-buffer-overflow` or a `SEGV`. Silently returning wrong data is the
         * worse of the two, which is why the guard is a domain check rather than an assertion.
         *
         * The negative half is tested on the signed value and the upper half only afterwards,
         * so no `size_t` conversion is ever performed on a negative index -- and, unlike a
         * single `uintcs` comparison, the bound is not silently truncated if `size()` ever
         * exceeds `UINTCS_MAX`.
         */
        [[nodiscard]] size_t validatedIndex(intcs index) const {
            if (index < 0 || static_cast<size_t>(index) >= cookies_.size()) {
                throw System::ArgumentOutOfRangeException(
                    "index",
                    "Index was out of range. Must be non-negative and less than the size of the collection.");
            }
            return static_cast<size_t>(index);
        }

        std::vector<Cookie> cookies_;
    };

} // namespace System::Net
