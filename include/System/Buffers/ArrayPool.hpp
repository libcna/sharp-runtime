// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <vector>

namespace System::Buffers {

    template<typename T>
    class ArrayPool {
    public:
        virtual ~ArrayPool() = default;

        virtual std::vector<T> Rent(int minimumLength) = 0;
        virtual void Return(std::vector<T>& array, bool clearArray = false) {
            if (clearArray) array.assign(array.size(), T{});
        }

        static ArrayPool<T>& Shared();
    };

    // Defined after ArrayPool<T> is complete to avoid incomplete-type warning.
    template<typename T>
    struct SharedArrayPool : ArrayPool<T> {
        std::vector<T> Rent(int minimumLength) override {
            return std::vector<T>(static_cast<size_t>(minimumLength));
        }
    };

    template<typename T>
    ArrayPool<T>& ArrayPool<T>::Shared() {
        static SharedArrayPool<T> instance;
        return instance;
    }

} // namespace System::Buffers
