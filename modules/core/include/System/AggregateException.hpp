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

    /**
     * @brief Rejects any null entry in @p exs and returns its first element, or a null
     *        `std::exception_ptr` when @p exs is empty.
     *
     * This is the value every constructor that stores inner exceptions hands to the
     * `Exception(msg, inner)` base constructor, so that the base
     * `getInnerExceptionProperty()` names the first cause instead of reporting none.
     *
     * `System::Exception` documents `getInnerExceptionProperty()` as "the exception that
     * caused the current exception". Before ticket #2307 every one of this class's six
     * constructors left that field null while `getInnerExceptionsProperty()` held the
     * causes -- an aggregate that stored N causes reported none. The frozen SR-AUD-098
     * text names only the message-taking constructors; measured per constructor, the plain
     * vector and initializer-list constructors had the identical gap, so the repair is
     * applied to all of them.
     *
     * A base-class initializer is sequenced before every member initializer, so calling
     * this from the base initializer also moves the null-element rejection ahead of
     * `innerExceptions_`, without changing which exception type is thrown.
     */
    static std::exception_ptr firstValidatedInnerOf(const std::vector<std::exception_ptr>& exs) {
        requireNoNullElements(exs);
        return exs.empty() ? std::exception_ptr() : exs.front();
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
        : Exception(buildMessage(innerExceptions), firstValidatedInnerOf(innerExceptions)),
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
        : Exception(message, firstValidatedInnerOf(innerExceptions)),
          innerExceptions_(std::move(innerExceptions)) {}

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
        : Exception(message, requireNonNullInner(innerException)),
          innerExceptions_({innerException}) {}

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
     *
     * Leaves come out in breadth-first order: every direct leaf of one aggregate is
     * emitted before any leaf drawn from an aggregate nested inside it. For
     * `{ Aggregate{a, b}, c }` that is `c, a, b`.
     *
     * This replaced a recursive depth-first walk that emitted nested leaves before a
     * later direct leaf -- `a, b, c` for the same input. SR-AUD-098 records both the
     * algorithm ("current .NET's queue algorithm returns direct leaves before queued
     * nested leaves") and that exact expected output, and the pre-repair `a, b, c` was
     * reproduced for the shape before the change (ticket #2308).
     *
     * Shapes on which the two algorithms agree -- an already flat aggregate, and any
     * aggregate whose nested entries all follow its direct leaves -- are unchanged.
     * Leaf identity and leaf count are unchanged for every shape; only the order is.
     * Because this class composes its default message from the leaves in leaf order,
     * a nested aggregate's flattened message lists the same leaves in the new order.
     *
     * Being iterative also removes the one stack frame per nesting level the recursion
     * needed, so flattening depth is bounded by heap rather than by stack.
     */
    [[nodiscard]] AggregateException Flatten() const {
        std::vector<std::exception_ptr> flat;

        // FIFO of the inner lists still to be walked, seeded with this aggregate's own.
        // Walked by index rather than popped, and each list is COPIED out of the queue
        // before use: the push_back below can reallocate `pending`, which would leave a
        // reference into it dangling.
        std::vector<std::vector<std::exception_ptr>> pending;
        pending.push_back(innerExceptions_);

        for (std::size_t head = 0; head < pending.size(); ++head) {
            const std::vector<std::exception_ptr> current = pending[pending.size() - 1 - head];
            for (const auto& ep : current) {
                try {
                    std::rethrow_exception(ep);
                } catch (const AggregateException& ae) {
                    pending.push_back(ae.innerExceptions_);
                } catch (...) {
                    flat.push_back(ep);
                }
            }
        }
        return AggregateException(std::move(flat));
    }

    /**
     * @brief Invokes a handler on each inner exception; rethrows unhandled ones as a new AggregateException.
     *
     * C++ counterpart of .NET AggregateException.Handle(Func&lt;Exception,bool&gt;).
     *
     * The empty-predicate check is unconditional and runs before the loop, exactly like
     * .NET's `ArgumentNullException.ThrowIfNull(predicate)` at the top of
     * `AggregateException.Handle`. Without it an empty std::function used to reach
     * `predicate(ep)` and raise `std::bad_function_call` -- a native exception outside the
     * System::Exception hierarchy -- at the first inner exception, and to be accepted
     * silently when there was no inner exception at all, so the same wrong call was fatal
     * or invisible depending on the aggregate's contents. Ticket #1867 / SR-AUD-099 /
     * CCF-011; see docs/EmptyCallableBoundaryPlan.md.
     *
     * @param predicate Handler returning true if the exception is handled.
     * @throws ArgumentNullException if @p predicate is an empty std::function.
     */
    void Handle(std::function<bool(std::exception_ptr)> predicate) const {
        if (!predicate) throw ArgumentNullException("predicate");
        std::vector<std::exception_ptr> unhandled;
        for (auto& ep : innerExceptions_) {
            if (!predicate(ep)) unhandled.push_back(ep);
        }
        if (!unhandled.empty()) throw AggregateException(std::move(unhandled));
    }
};

} // namespace System
