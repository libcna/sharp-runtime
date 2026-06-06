// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Version.hpp"

namespace System {

    class ApplicationId {
        std::string name_;
        Version version_;
        std::string processorArchitecture_;
        std::string culture_;
        std::string publicKeyToken_;

    public:
        ApplicationId(const std::string& publicKeyToken,
                      const std::string& name,
                      const Version& version,
                      const std::string& processorArchitecture,
                      const std::string& culture)
            : name_(name), version_(version),
              processorArchitecture_(processorArchitecture),
              culture_(culture), publicKeyToken_(publicKeyToken) {}

        [[nodiscard]] const std::string& getNameProperty()                    const { return name_; }
        [[nodiscard]] const Version&     getVersionProperty()                 const { return version_; }
        [[nodiscard]] const std::string& getProcessorArchitectureProperty()   const { return processorArchitecture_; }
        [[nodiscard]] const std::string& getCultureProperty()                 const { return culture_; }
        [[nodiscard]] const std::string& getPublicKeyTokenProperty()          const { return publicKeyToken_; }

        [[nodiscard]] std::string ToString() const {
            return name_ + ", Version=" + version_.ToString()
                + ", Culture=" + culture_
                + ", ProcessorArchitecture=" + processorArchitecture_;
        }
    };

} // namespace System
