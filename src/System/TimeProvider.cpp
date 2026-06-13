// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/TimeProvider.hpp"

namespace {
    class SystemTimeProvider : public System::TimeProvider {
    public:
        SystemTimeProvider() = default;
    };
} // anonymous namespace

namespace System {
    TimeProvider& TimeProvider::getSystemProperty() {
        static SystemTimeProvider instance;
        return instance;
    }
} // namespace System
