// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/DictionaryEntry.hpp"

namespace System::Collections {

    /// Enumerates the elements of a non-generic dictionary.
    class IDictionaryEnumerator : public IEnumerator {
    public:
        /// Destroys the enumerator.
        virtual ~IDictionaryEnumerator() = default;
        /// Gets both the key and the value of the current dictionary entry.
        [[nodiscard]] virtual DictionaryEntry getEntryProperty() const = 0;
        /// Gets the key of the current dictionary entry.
        [[nodiscard]] virtual const void* getKeyProperty() const = 0;
        /// Gets the value of the current dictionary entry.
        [[nodiscard]] virtual const void* getValueProperty() const = 0;
    };

} // namespace System::Collections
