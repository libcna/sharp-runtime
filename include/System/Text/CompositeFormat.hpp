// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Text {

    /** Represents a parsed composite format string (e.g. "Hello, {0}!") that avoids re-parsing on each use. */
    class CompositeFormat {
        std::string format_;
        SharpRuntime::intcs minArgCount_ = 0;

        static SharpRuntime::intcs countPlaceholders(const std::string& fmt) {
            SharpRuntime::intcs maxIdx = -1;
            for (std::size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '{' && i + 1 < fmt.size() && fmt[i+1] != '{') {
                    std::size_t j = i + 1;
                    while (j < fmt.size() && fmt[j] != '}' && fmt[j] != ':') ++j;
                    if (j < fmt.size()) {
                        try {
                            SharpRuntime::intcs idx = std::stoi(fmt.substr(i + 1, j - i - 1));
                            if (idx > maxIdx) maxIdx = idx;
                        } catch (...) {}
                    }
                }
            }
            return maxIdx + 1;
        }

    public:
        /** Parses a composite format string and returns a CompositeFormat instance. */
        static CompositeFormat Parse(const std::string& format) {
            CompositeFormat cf;
            cf.format_ = format;
            cf.minArgCount_ = countPlaceholders(format);
            return cf;
        }

        /** Gets the original format string. */
        [[nodiscard]] const std::string& getFormatProperty() const { return format_; }
        /** Gets the minimum number of arguments required to satisfy the format string. */
        [[nodiscard]] SharpRuntime::intcs getMinimumArgumentCountProperty() const { return minArgCount_; }
    };

} // namespace System::Text
