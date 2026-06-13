// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>

namespace System {

    /// Represents a composite format string along with its arguments; C++ approximation of System.FormattableString.
    class FormattableString {
    protected:
        std::string format_;
        std::vector<std::string> args_;

    public:
        /// Initializes a new instance with the specified format string and optional argument list.
        explicit FormattableString(std::string format, std::vector<std::string> args = {})
            : format_(std::move(format)), args_(std::move(args)) {}

        /// Virtual destructor for safe polymorphic destruction.
        virtual ~FormattableString() = default;

        /// Returns the composite format string.
        [[nodiscard]] const std::string& getFormatProperty() const { return format_; }
        /// Returns the number of format arguments.
        [[nodiscard]] int getArgumentCountProperty() const { return static_cast<int>(args_.size()); }
        /// Returns the argument at the specified index.
        [[nodiscard]] const std::string& GetArgument(int index) const { return args_.at(index); }

        // Invariant: substitute {0}, {1}... with the stored args.
        /// Returns the formatted string with arguments substituted.
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

        /// Returns the formatted string using the invariant culture (same as ToString() in this port).
        static std::string Invariant(const FormattableString& formattable) {
            return formattable.ToString();
        }
    };

} // namespace System
