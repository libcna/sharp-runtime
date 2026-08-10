// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::ComponentModel {

/**
 * Specifies that an object supports a simple transacted notification for batch initialization.
 *
 * C++ counterpart of .NET System.ComponentModel.ISupportInitialize.
 */
class ISupportInitialize {
public:
    /** Virtual destructor for safe destruction through the interface. */
    virtual ~ISupportInitialize() = default;

    /** Signals that initialization is starting. */
    virtual void BeginInit() = 0;

    /** Signals that initialization is complete. */
    virtual void EndInit() = 0;
};

} // namespace System::ComponentModel
