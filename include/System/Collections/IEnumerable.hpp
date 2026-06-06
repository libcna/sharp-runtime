#pragma once

#include "System/Collections/IEnumerator.hpp"

namespace System::Collections
{
    /**
     * @brief Exposes an enumerator that provides simple iteration over a non-generic collection.
     *
     * C++ counterpart of the .NET System.Collections.IEnumerable interface.
     */
    class IEnumerable
    {
    public:
        virtual ~IEnumerable() = default;

        /// Returns an enumerator that iterates through the collection.
        [[nodiscard]] virtual IEnumerator* GetEnumerator() = 0;
    };
}
