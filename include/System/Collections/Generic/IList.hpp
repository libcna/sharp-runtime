#pragma once

#include "System/Collections/Generic/ICollection.hpp"

namespace System::Collections::Generic
{
    /**
     * @brief Represents a generic collection of elements accessible by index.
     *
     * C++ counterpart of the .NET System.Collections.Generic.IList<T> interface.
     *
     * @tparam T The type of elements in the list.
     */
    template<typename T>
    class IList : public ICollection<T>
    {
    public:
        ~IList() override = default;

        /// Gets the element at the specified index.
        [[nodiscard]] virtual const T& operator[](int index) const = 0;

        /// Gets or sets the element at the specified index (non-const).
        virtual T& operator[](int index) = 0;

        /// Returns the index of the first occurrence of the specified item, or -1.
        [[nodiscard]] virtual int IndexOf(const T& item) const = 0;

        /// Inserts an item at the specified index.
        virtual void Insert(int index, const T& item) = 0;

        /// Removes the element at the specified index.
        virtual void RemoveAt(int index) = 0;
    };
}
