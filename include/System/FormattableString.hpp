// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>

namespace System {

    // Abstract base class representing a composite format string and its arguments.
    // In C++ we store the pre-formatted result since we have no runtime string interpolation.
    class FormattableString {
    protected:
        std::string format_;
        std::vector<std::string> args_;

    public:
        explicit FormattableString(std::string format, std::vector<std::string> args = {})
            : format_(std::move(format)), args_(std::move(args)) {}

        virtual ~FormattableString() = default;

        [[nodiscard]] const std::string& getFormatProperty() const { return format_; }
        [[nodiscard]] int getArgumentCountProperty() const { return static_cast<int>(args_.size()); }
        [[nodiscard]] const std::string& GetArgument(int index) const { return args_.at(index); }

        // Invariant: substitute {0}, {1}... with the stored args.
        [[nodiscard]] virtual std::string ToString() const {
            std::string result = format_;
            for (int i = 0; i < static_cast<int>(args_.size()); ++i) {
                std::string placeholder = "{" + std::to_string(i) + "}";
                std::size_t pos = 0;
                while ((pos = result.find(placeholder, pos)) != std::string::npos) {
                    result.replace(pos, placeholder.size(), args_[i]);
                    pos += args_[i].size();
                }
            }
            return result;
        }

        static std::string Invariant(const FormattableString& formattable) {
            return formattable.ToString();
        }
    };

} // namespace System
