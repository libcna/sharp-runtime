// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/DictionaryEntry.hpp"

namespace System::Collections {

    class IDictionaryEnumerator : public IEnumerator {
    public:
        virtual ~IDictionaryEnumerator() = default;
        [[nodiscard]] virtual DictionaryEntry getEntryProperty() const = 0;
        [[nodiscard]] virtual const void* getKeyProperty() const = 0;
        [[nodiscard]] virtual const void* getValueProperty() const = 0;
    };

} // namespace System::Collections
