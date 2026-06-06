#pragma once

#include "System/Collections/IEnumerable.hpp"
#include "System/Collections/Generic/IEnumerator.hpp"

namespace System::Collections::Generic
{
    /**
     * @brief Exposes an enumerator for a generic collection.
     *
     * C++ counterpart of the .NET System.Collections.Generic.IEnumerable<T> interface.
     *
     * @tparam T The type of elements to enumerate.
     */
    template<typename T>
    class IEnumerable : public System::Collections::IEnumerable
    {
    public:
        ~IEnumerable() override = default;

        /// Returns a typed enumerator that iterates through the collection.
        [[nodiscard]] virtual IEnumerator<T>* GetEnumerator() = 0;

        System::Collections::IEnumerator* GetEnumerator() override
        {
            return GetEnumerator();
        }
    };
}
