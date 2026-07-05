// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cassert>
#include <iostream>
#include <string>

namespace System::Diagnostics {

/**
 * @brief Provides the default implementation for the Write and Fail methods of Debug.
 *
 * C++ counterpart of .NET System.Diagnostics.DebugProvider.
 * Override Write/WriteLine/Fail/OnIndentLevelChanged/OnIndentSizeChanged and install the
 * override via Debug::SetProvider() to redirect where Debug output goes (e.g. to an
 * on-screen console instead of stdout), matching .NET's Debug.SetProvider extensibility hook.
 */
class DebugProvider {
public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~DebugProvider() = default;

    /**
     * @brief Writes @p message to stdout, with no trailing newline.
     *
     * C++ counterpart of .NET DebugProvider.Write(string).
     * @param message The text to write.
     */
    virtual void Write(const std::string& message) { std::cout << message; }

    /**
     * @brief Writes @p message followed by a newline.
     *
     * C++ counterpart of .NET DebugProvider.WriteLine(string).
     * @param message The text to write.
     */
    virtual void WriteLine(const std::string& message) { Write(message + "\n"); }

    /**
     * @brief Reports an assertion failure: emits @p message/@p detailMessage and aborts.
     *
     * C++ counterpart of .NET DebugProvider.Fail(string, string).
     * @param message       The primary failure message.
     * @param detailMessage Additional detail about the failure.
     */
    virtual void Fail(const std::string& message, const std::string& detailMessage) {
        std::cerr << "Assertion failed: " << message;
        if (!detailMessage.empty()) std::cerr << "\n" << detailMessage;
        std::cerr << std::endl;
        assert(false);
    }

    /**
     * @brief Invoked when Debug::IndentLevel changes; override to react to indent changes.
     *
     * C++ counterpart of .NET DebugProvider.OnIndentLevelChanged(int).
     * @param indentLevel The new indent level.
     */
    virtual void OnIndentLevelChanged(int indentLevel) { (void)indentLevel; }

    /**
     * @brief Invoked when Debug::IndentSize changes; override to react to indent-size changes.
     *
     * C++ counterpart of .NET DebugProvider.OnIndentSizeChanged(int).
     * @param indentSize The new indent size.
     */
    virtual void OnIndentSizeChanged(int indentSize) { (void)indentSize; }
};

} // namespace System::Diagnostics
