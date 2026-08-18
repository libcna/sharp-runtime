// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/5/25.
//

#include "System/Exception.hpp"

namespace System {

    // Ticket #2323 (SR-AUD-092, 2026-08-18). This used to leave the message EMPTY, where .NET's
    // is `_message ?? SR.Format(SR.Exception_WasThrown, GetClassName())` (Exception.cs:61,65) --
    // "Exception of type '{0}' was thrown.", Strings.resx:2333, with {0} the RUNTIME TYPE NAME.
    //
    // The review recorded two blockers. The first, the exact resource text, is simply readable
    // now. The second is real and permanent: {0} is reflection, which this port does not have.
    //
    // WHAT DISSOLVES IT is that .NET computes the fallback LAZILY only so that `_message` can
    // stay null for serialization; the observable is identical if the constructor just stores it,
    // which is what a hundred subclasses in this repository already do. So {0} is resolved
    // STATICALLY, at each site, by the one entity that knows the answer -- the type itself. No
    // reflection, no new virtual, no layout change, no signature change.
    //
    // Hard-coding this string into the base and letting subclasses inherit it was the option the
    // review rejected, and rightly: a message naming the WRONG type is a lie, where an empty one
    // is merely an absence. The two subclasses that reach here are given their own (#2323).
    Exception::Exception()
        : message_("Exception of type 'System.Exception' was thrown.") {
    }

    Exception::Exception(const char* msg)
        : message_(msg ? msg : "") {
    }

    Exception::Exception(const std::string& msg)
        : message_(msg) {
    }

    Exception::Exception(const std::string& msg, std::exception_ptr inner)
        : message_(msg), innerException_(std::move(inner)) {
    }

    const std::string& Exception::getMessageProperty() const {
        return message_;
    }

    std::exception_ptr Exception::getInnerExceptionProperty() const {
        return innerException_;
    }

    const std::string& Exception::getStackTraceProperty() const {
        static const std::string empty;
        return empty;
    }

    std::map<std::string, std::string>& Exception::getDataProperty() {
        return data_;
    }

    const std::map<std::string, std::string>& Exception::getDataProperty() const {
        return data_;
    }

    const std::string& Exception::getHelpLinkProperty() const {
        return helpLink_;
    }

    void Exception::setHelpLinkProperty(const std::string& value) {
        helpLink_ = value;
    }

    const std::string& Exception::getSourceProperty() const {
        return source_;
    }

    void Exception::setSourceProperty(const std::string& value) {
        source_ = value;
    }

    SharpRuntime::intcs Exception::getHResultProperty() const {
        return hResult_;
    }

    void Exception::setHResultProperty(SharpRuntime::intcs value) {
        hResult_ = value;
    }

    const char* Exception::what() const noexcept {
        return message_.c_str();
    }

} // namespace System