// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Runtime::Versioning {

    class TargetFrameworkAttribute : public System::Attribute {
        std::string frameworkName_;
        std::string frameworkDisplayName_;
    public:
        explicit TargetFrameworkAttribute(const std::string& frameworkName)
            : frameworkName_(frameworkName) {}

        [[nodiscard]] const std::string& getFrameworkNameProperty()        const { return frameworkName_; }
        [[nodiscard]] const std::string& getFrameworkDisplayNameProperty() const { return frameworkDisplayName_; }
        void setFrameworkDisplayNameProperty(const std::string& v) { frameworkDisplayName_ = v; }
    };

    class SupportedOSPlatformAttribute : public System::Attribute {
        std::string platformName_;
    public:
        explicit SupportedOSPlatformAttribute(const std::string& platformName)
            : platformName_(platformName) {}
        [[nodiscard]] const std::string& getPlatformNameProperty() const { return platformName_; }
    };

    class UnsupportedOSPlatformAttribute : public System::Attribute {
        std::string platformName_;
    public:
        explicit UnsupportedOSPlatformAttribute(const std::string& platformName)
            : platformName_(platformName) {}
        [[nodiscard]] const std::string& getPlatformNameProperty() const { return platformName_; }
    };

    class ObsoletedOSPlatformAttribute : public System::Attribute {
        std::string platformName_;
        std::string message_;
        std::string url_;
    public:
        explicit ObsoletedOSPlatformAttribute(const std::string& platformName,
                                               const std::string& message = {},
                                               const std::string& url = {})
            : platformName_(platformName), message_(message), url_(url) {}
        [[nodiscard]] const std::string& getPlatformNameProperty() const { return platformName_; }
        [[nodiscard]] const std::string& getMessageProperty()      const { return message_; }
        [[nodiscard]] const std::string& getUrlProperty()          const { return url_; }
    };

    class RequiresPreviewFeaturesAttribute : public System::Attribute {
        std::string message_;
        std::string url_;
    public:
        RequiresPreviewFeaturesAttribute() = default;
        explicit RequiresPreviewFeaturesAttribute(const std::string& message, const std::string& url = {})
            : message_(message), url_(url) {}
        [[nodiscard]] const std::string& getMessageProperty() const { return message_; }
        [[nodiscard]] const std::string& getUrlProperty()     const { return url_; }
    };

} // namespace System::Runtime::Versioning
