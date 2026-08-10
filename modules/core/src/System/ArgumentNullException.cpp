// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ArgumentNullException.hpp"

namespace System {

    namespace {
        constexpr const char* DefaultMsg = "Value cannot be null.";
    }

    // Ticket #1776 (REMED-CORE-ARGNULL-MESSAGE / SR-AUD-089, SR-AUD-090): the paramName-only
    // constructors used to precompose "(Parameter 'x')" via a local makeMsg() helper and then
    // pass that already-suffixed text to the ArgumentException(message, paramName) base, whose
    // own appendParamName() appended the identical suffix a second time -- and makeMsg's raw
    // C-string concatenation null-dereferenced for a null paramName before that base constructor
    // could apply its own null guard. Passing the raw DefaultMsg straight through, exactly like
    // .NET's own `ArgumentNullException(string? paramName) : base(SR.ArgumentNull_Generic,
    // paramName)`, appends the suffix exactly once and reuses the base constructor's existing
    // null-safe handling for both message and paramName.
    ArgumentNullException::ArgumentNullException()
        : ArgumentException(DefaultMsg) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004003)); // E_POINTER
    }

    ArgumentNullException::ArgumentNullException(const char* paramName)
        : ArgumentException(DefaultMsg, paramName) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004003)); // E_POINTER
    }

    ArgumentNullException::ArgumentNullException(const char* paramName, const char* message)
        : ArgumentException(message, paramName) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004003)); // E_POINTER
    }

    ArgumentNullException::ArgumentNullException(const std::string& paramName)
        : ArgumentException(std::string(DefaultMsg), paramName) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004003)); // E_POINTER
    }

    ArgumentNullException::ArgumentNullException(const std::string& paramName, const std::string& message)
        : ArgumentException(message, paramName) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004003)); // E_POINTER
    }

    ArgumentNullException::ArgumentNullException(const std::string& message, std::exception_ptr innerException)
        : ArgumentException(message, std::move(innerException)) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004003)); // E_POINTER
    }

} // namespace System
