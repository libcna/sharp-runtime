// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System::Security {

    enum class SecurityRuleSet : uint8_t {
        None    = 0,
        Level1  = 1,
        Level2  = 2,
    };

    enum class PartialTrustVisibilityLevel {
        VisibleToAllHosts   = 0,
        NotVisibleByDefault = 1,
    };

    class AllowPartiallyTrustedCallersAttribute : public System::Attribute {
        PartialTrustVisibilityLevel visibilityLevel_ = PartialTrustVisibilityLevel::VisibleToAllHosts;
    public:
        AllowPartiallyTrustedCallersAttribute() = default;
        [[nodiscard]] PartialTrustVisibilityLevel getPartialTrustVisibilityLevelProperty() const { return visibilityLevel_; }
        void setPartialTrustVisibilityLevelProperty(PartialTrustVisibilityLevel v) { visibilityLevel_ = v; }
    };

    class SecurityCriticalAttribute : public System::Attribute {};
    class SecuritySafeCriticalAttribute : public System::Attribute {};
    class SecurityTransparentAttribute : public System::Attribute {};
    class DynamicSecurityMethodAttribute : public System::Attribute {};
    class SuppressUnmanagedCodeSecurityAttribute : public System::Attribute {};
    class UnverifiableCodeAttribute : public System::Attribute {};

    class SecurityRulesAttribute : public System::Attribute {
        SecurityRuleSet ruleSet_;
    public:
        explicit SecurityRulesAttribute(SecurityRuleSet ruleSet) : ruleSet_(ruleSet) {}
        [[nodiscard]] SecurityRuleSet getRuleSetProperty() const { return ruleSet_; }
        bool skipVerificationInFullTrust_ = false;
    };

} // namespace System::Security
