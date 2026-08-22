// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "System/ArgumentNullException.hpp"
#include "System/Globalization/detail/UnicodeCategoryLookup.hpp"
#include "System/Security/Principal/IPrincipal.hpp"
#include "System/detail/Utf8Scalar.hpp"

namespace System::Security::Principal {

    /**
     * @brief Represents a generic principal, backed by a fixed list of role names.
     *
     * C++ counterpart of .NET System.Security.Principal.GenericPrincipal.
     *
     * @note .NET's real `GenericPrincipal` derives from `System.Security.Claims.ClaimsPrincipal`
     * (adding the identity and roles as claims, so `IsInRole` also consults any claims already on
     * the wrapped identity). That whole claims-based system (`System.Security.Claims`) is a
     * separate, not-yet-reviewed namespace here, so this is a standalone reduced-scope
     * implementation: role membership is checked only against the explicit @p roles list passed
     * to the constructor, not any claim on the identity.
     */
    class GenericPrincipal : public IPrincipal {
        std::shared_ptr<IIdentity> identity_;
        std::vector<std::string> roles_;

        static std::uint32_t nextOrdinalFold(const std::string& text, std::size_t& offset) noexcept {
            std::uint32_t codePoint = 0;
            std::size_t length = 0;
            if (System::detail::TryDecodeUtf8Scalar(text, offset, codePoint, length)) {
                offset += length;
                return System::Globalization::detail::LookupToUpperInvariant(codePoint);
            }

            // System::String cannot contain malformed UTF-8 in this port's supported contract,
            // but std::string can. Keep such bytes deterministic and distinct from every valid
            // Unicode scalar instead of feeding them to locale-dependent ctype functions.
            const auto raw = static_cast<unsigned char>(text[offset++]);
            return 0x110000u + raw;
        }

        static bool equalsOrdinalIgnoreCase(const std::string& a, const std::string& b) noexcept {
            std::size_t aOffset = 0;
            std::size_t bOffset = 0;
            while (aOffset < a.size() && bOffset < b.size()) {
                if (nextOrdinalFold(a, aOffset) != nextOrdinalFold(b, bOffset)) {
                    return false;
                }
            }
            return aOffset == a.size() && bOffset == b.size();
        }

    public:
        /**
         * @brief Constructs from @p identity and the list of role names it belongs to.
         * @throws System::ArgumentNullException if @p identity is null, matching .NET's
         *         `ArgumentNullException.ThrowIfNull(identity)` (GenericPrincipal.cs) --
         *         without this check, a null identity_ would silently propagate into
         *         getIdentityProperty() and crash a later, unrelated caller instead of
         *         failing fast here at construction.
         */
        GenericPrincipal(std::shared_ptr<IIdentity> identity, const std::vector<std::string>& roles)
            : identity_(std::move(identity)), roles_(roles) {
            if (!identity_) throw System::ArgumentNullException("identity");
        }

        [[nodiscard]] std::shared_ptr<IIdentity> getIdentityProperty() const override { return identity_; }

        [[nodiscard]] bool IsInRole(const std::string& role) const override {
            return std::any_of(roles_.begin(), roles_.end(),
                                [&](const std::string& r) { return equalsOrdinalIgnoreCase(r, role); });
        }
    };

} // namespace System::Security::Principal
