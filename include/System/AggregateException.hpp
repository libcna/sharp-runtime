// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/Exception.hpp"

namespace System {

    class AggregateException : public Exception {
        std::vector<std::exception_ptr> innerExceptions_;

        static std::string buildMessage(const std::vector<std::exception_ptr>& exs) {
            if (exs.empty()) return "One or more errors occurred.";
            std::string m = "One or more errors occurred. (";
            bool first = true;
            for (auto& ep : exs) {
                try { std::rethrow_exception(ep); }
                catch (const std::exception& e) {
                    if (!first) m += ") (";
                    m += e.what();
                    first = false;
                } catch (...) {
                    if (!first) m += ") (";
                    m += "unknown error";
                    first = false;
                }
            }
            m += ")";
            return m;
        }

    public:
        AggregateException() : Exception("One or more errors occurred.") {}
        explicit AggregateException(const std::string& message) : Exception(message) {}

        explicit AggregateException(std::vector<std::exception_ptr> innerExceptions)
            : Exception(buildMessage(innerExceptions)),
              innerExceptions_(std::move(innerExceptions)) {}

        AggregateException(const std::string& message, std::vector<std::exception_ptr> innerExceptions)
            : Exception(message), innerExceptions_(std::move(innerExceptions)) {}

        [[nodiscard]] const std::vector<std::exception_ptr>& getInnerExceptionsProperty() const {
            return innerExceptions_;
        }

        [[nodiscard]] std::size_t getInnerExceptionCountProperty() const {
            return innerExceptions_.size();
        }

        // Unwrap: if there is a single inner exception, return it; else return this.
        [[nodiscard]] std::exception_ptr Unwrap() const {
            if (innerExceptions_.size() == 1) return innerExceptions_[0];
            return std::make_exception_ptr(*this);
        }

        // Call handler for each inner exception. If handler returns true, exception is handled.
        void Handle(std::function<bool(std::exception_ptr)> handler) const {
            std::vector<std::exception_ptr> unhandled;
            for (auto& ep : innerExceptions_) {
                if (!handler(ep)) unhandled.push_back(ep);
            }
            if (!unhandled.empty()) throw AggregateException(std::move(unhandled));
        }
    };

} // namespace System
