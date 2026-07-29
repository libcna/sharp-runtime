// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Exception.hpp"

namespace System {

/**
 * @brief Represents one or more errors that occur during application execution.
 *
 * C++ counterpart of .NET System.AggregateException.
 * Stores a collection of inner exceptions and provides Flatten() and Handle() helpers.
 */
class AggregateException : public Exception {
    std::vector<std::exception_ptr> innerExceptions_;

    /**
     * @brief Rejects a null entry in a collection of inner exceptions.
     *
     * Verified against AggregateException.cs, whose private
     * `AggregateException(string?, Exception[], bool)` core constructor loops over the
     * array and throws `ArgumentException(SR.AggregateException_ctor_InnerExceptionNull)`
     * -- "An element of innerExceptions was null." -- for any null element. Every public
     * collection-taking constructor funnels through it, so the check is unconditional in
     * .NET and is unconditional here.
     *
     * This port previously accepted a null `std::exception_ptr` and stored it.
     * `std::rethrow_exception` has undefined behaviour for a null argument, and three
     * members call it, so a single accepted null armed three separate crashes --
     * `buildMessage` from the collection constructors, `collectLeaves` from `Flatten()`,
     * and `GetBaseException()` -- each an AddressSanitizer SEGV inside
     * `std::rethrow_exception` itself, on the trap address 0xffffffffffffff80 rather than
     * a plain null, which is what a null `exception_ptr` decodes to. Two further members
     * did not crash and were arguably worse: `Handle()` passed the null straight to the
     * caller's predicate, and `Unwrap()` returned it, so the crash surfaced in consumer
     * code with no trace of where the null entered. All five are recorded per case in
     * build-probe/1807_prefix_defects.log (ticket #1807 / SR-AUD-097).
     */
    static void requireNoNullElements(const std::vector<std::exception_ptr>& exs) {
        for (const auto& ep : exs) {
            if (ep == nullptr)
                throw System::ArgumentException("An element of innerExceptions was null.");
        }
    }

    /** @brief Returns @p exs after rejecting any null entry; see requireNoNullElements(). */
    static std::vector<std::exception_ptr> validatedInner(std::vector<std::exception_ptr> exs) {
        requireNoNullElements(exs);
        return exs;
    }

    /**
     * @brief Returns @p ep after rejecting a null single inner exception.
     *
     * Verified against AggregateException.cs line 59, `AggregateException(string? message,
     * Exception innerException)`, which opens with
     * `ArgumentNullException.ThrowIfNull(innerException)`. A missing single argument is an
     * `ArgumentNullException`, whereas a null *inside* a collection is an
     * `ArgumentException` naming the collection; this port reproduces that split rather
     * than collapsing both onto one type.
     */
    static std::exception_ptr requireNonNullInner(std::exception_ptr ep) {
        if (ep == nullptr) throw System::ArgumentNullException("innerException");
        return ep;
    }

    static std::string buildMessage(const std::vector<std::exception_ptr>& exs) {
        // Ahead of the loop below, because that loop is the first of the three
        // std::rethrow_exception call sites a null entry would reach. A base-class
        // initializer is sequenced before every member initializer, so validating here
        // also protects innerExceptions_ in the constructors that build their message
        // from the same vector.
        requireNoNullElements(exs);
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
    /** @brief Initializes a new instance with the default aggregate error message. */
    AggregateException() : Exception("One or more errors occurred.") {}

    /** @brief Initializes a new instance with the specified error message. */
    explicit AggregateException(const std::string& message) : Exception(message) {}

    /**
     * @brief Initializes a new instance with a collection of inner exceptions.
     * @throws System::ArgumentException if any entry is a null `std::exception_ptr`.
     */
    explicit AggregateException(std::vector<std::exception_ptr> innerExceptions)
        : Exception(buildMessage(innerExceptions)),
          innerExceptions_(std::move(innerExceptions)) {}

    /**
     * @brief Initializes a new instance with an initializer list of inner exceptions.
     * @throws System::ArgumentException if any entry is a null `std::exception_ptr`.
     */
    AggregateException(std::initializer_list<std::exception_ptr> innerExceptions)
        : AggregateException(std::vector<std::exception_ptr>(innerExceptions)) {}

    /**
     * @brief Initializes a new instance with a message and a collection of inner exceptions.
     * @throws System::ArgumentException if any entry is a null `std::exception_ptr`.
     */
    AggregateException(const std::string& message, std::vector<std::exception_ptr> innerExceptions)
        : Exception(message), innerExceptions_(validatedInner(std::move(innerExceptions))) {}

    /**
     * @brief Initializes a new instance with a message and a single inner exception.
     *
     * Matches .NET's `AggregateException(string?, Exception)`, which opens with
     * `ArgumentNullException.ThrowIfNull(innerException)` -- a single missing inner
     * exception is a null *argument*, where a null inside a collection is a malformed
     * collection, so the two report different exception types exactly as .NET does.
     *
     * @throws System::ArgumentNullException if @p innerException is null.
     */
    AggregateException(const std::string& message, std::exception_ptr innerException)
        : Exception(message),
          innerExceptions_({requireNonNullInner(std::move(innerException))}) {}

    /**
     * @brief Gets the read-only collection of inner exceptions that caused this aggregate exception.
     *
     * C++ counterpart of .NET AggregateException.InnerExceptions.
     */
    [[nodiscard]] const std::vector<std::exception_ptr>& getInnerExceptionsProperty() const {
        return innerExceptions_;
    }

    /**
     * @brief Returns the number of inner exceptions contained in this aggregate exception.
     *
     * C++ extension — convenience wrapper over InnerExceptions.size().
     */
    [[nodiscard]] std::size_t getInnerExceptionCountProperty() const {
        return innerExceptions_.size();
    }

    /**
     * @brief Returns the first inner exception, or this exception if there are none or more than one.
     *
     * C++ counterpart of .NET AggregateException.GetBaseException().
     */
    [[nodiscard]] std::exception_ptr GetBaseException() const {
        if (innerExceptions_.size() == 1) {
            try {
                std::rethrow_exception(innerExceptions_[0]);
            } catch (const AggregateException& ae) {
                return ae.GetBaseException();
            } catch (...) {}
            return innerExceptions_[0];
        }
        return std::make_exception_ptr(*this);
    }

    /**
     * @brief Returns the single inner exception if exactly one is present, otherwise returns this exception.
     *
     * C++ extension — convenience wrapper around GetBaseException() for the common single-exception case.
     */
    [[nodiscard]] std::exception_ptr Unwrap() const {
        if (innerExceptions_.size() == 1) return innerExceptions_[0];
        return std::make_exception_ptr(*this);
    }

    /**
     * @brief Flattens nested AggregateExceptions into a single flat AggregateException.
     *
     * C++ counterpart of .NET AggregateException.Flatten().
     */
    [[nodiscard]] AggregateException Flatten() const {
        std::vector<std::exception_ptr> flat;
        collectLeaves(innerExceptions_, flat);
        return AggregateException(std::move(flat));
    }

    /**
     * @brief Invokes a handler on each inner exception; rethrows unhandled ones as a new AggregateException.
     *
     * C++ counterpart of .NET AggregateException.Handle(Func&lt;Exception,bool&gt;).
     * @param predicate Handler returning true if the exception is handled.
     */
    void Handle(std::function<bool(std::exception_ptr)> predicate) const {
        std::vector<std::exception_ptr> unhandled;
        for (auto& ep : innerExceptions_) {
            if (!predicate(ep)) unhandled.push_back(ep);
        }
        if (!unhandled.empty()) throw AggregateException(std::move(unhandled));
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
