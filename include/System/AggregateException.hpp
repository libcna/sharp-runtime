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

    /// The exception that represents one or more errors that occur during application execution.
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
        /// Initializes a new instance with the default aggregate error message.
        AggregateException() : Exception("One or more errors occurred.") {}
        /// Initializes a new instance with the specified error message.
        explicit AggregateException(const std::string& message) : Exception(message) {}

        /// Initializes a new instance with a collection of inner exceptions.
        explicit AggregateException(std::vector<std::exception_ptr> innerExceptions)
            : Exception(buildMessage(innerExceptions)),
              innerExceptions_(std::move(innerExceptions)) {}

        /// Initializes a new instance with a message and collection of inner exceptions.
        AggregateException(const std::string& message, std::vector<std::exception_ptr> innerExceptions)
            : Exception(message), innerExceptions_(std::move(innerExceptions)) {}

        /// Returns the collection of inner exceptions that caused this aggregate exception.
        [[nodiscard]] const std::vector<std::exception_ptr>& getInnerExceptionsProperty() const {
            return innerExceptions_;
        }

        /// Returns the number of inner exceptions contained in this aggregate exception.
        [[nodiscard]] std::size_t getInnerExceptionCountProperty() const {
            return innerExceptions_.size();
        }

        /// Returns the single inner exception if only one is present, otherwise returns this exception.
        [[nodiscard]] std::exception_ptr Unwrap() const {
            if (innerExceptions_.size() == 1) return innerExceptions_[0];
            return std::make_exception_ptr(*this);
        }

        /// Invokes a handler on each inner exception; rethrows unhandled ones as a new AggregateException.
        void Handle(std::function<bool(std::exception_ptr)> handler) const {
            std::vector<std::exception_ptr> unhandled;
            for (auto& ep : innerExceptions_) {
                if (!handler(ep)) unhandled.push_back(ep);
            }
            if (!unhandled.empty()) throw AggregateException(std::move(unhandled));
        }

        /// Flattens nested AggregateExceptions into a single flat AggregateException.
        [[nodiscard]] AggregateException Flatten() const {
            std::vector<std::exception_ptr> flat;
            collectLeaves(innerExceptions_, flat);
            return AggregateException(std::move(flat));
        }

    private:
        static void collectLeaves(const std::vector<std::exception_ptr>& exs,
                                  std::vector<std::exception_ptr>& result) {
            for (auto& ep : exs) {
                try {
                    std::rethrow_exception(ep);
                } catch (const AggregateException& ae) {
                    collectLeaves(ae.innerExceptions_, result);
                } catch (...) {
                    result.push_back(ep);
                }
            }
        }
    };

} // namespace System
