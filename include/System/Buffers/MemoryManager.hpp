// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <vector>
#include "System/Span.hpp"
#include "System/Buffers/IMemoryOwner.hpp"
#include "System/Buffers/IPinnable.hpp"

namespace System::Buffers {

/**
 * @brief Abstract base for custom Memory&lt;T&gt; managers.
 *
 * C++ counterpart of .NET System.Buffers.MemoryManager&lt;T&gt;.
 * Implements both IMemoryOwner&lt;T&gt; and IPinnable. Concrete subclasses must
 * provide GetSpan(), Pin(), and Unpin(). The IMemoryOwner&lt;T&gt;::getMemoryProperty()
 * default implementation throws; subclasses that need it should override.
 *
 * @tparam T The element type.
 */
template<typename T>
class MemoryManager : public IMemoryOwner<T>, public IPinnable {
public:
    virtual ~MemoryManager() = default;

    /**
     * @brief Returns a Span&lt;T&gt; wrapping the underlying memory region.
     *
     * C++ counterpart of .NET MemoryManager&lt;T&gt;.GetSpan().
     */
    virtual System::Span<T> GetSpan() = 0;

    /**
     * @brief Pins the memory at the given element offset and returns a handle.
     *
     * C++ counterpart of .NET MemoryManager&lt;T&gt;.Pin(int).
     */
    MemoryHandle Pin(int elementIndex) override = 0;

    /**
     * @brief Unpins the memory, allowing the GC to move it.
     *
     * C++ counterpart of .NET MemoryManager&lt;T&gt;.Unpin().
     */
    void Unpin() override = 0;

    /**
     * @brief Implements IDisposable. Default no-op; override to release resources.
     *
     * C++ counterpart of .NET MemoryManager&lt;T&gt;.Dispose(bool).
     */
    void Dispose() override {}

    /**
     * @brief Returns the memory block owned by this manager as a vector reference.
     *
     * C++ counterpart of the IMemoryOwner&lt;T&gt;.Memory property.
     * Default implementation throws; subclasses should override if needed.
     */
    std::vector<T>& getMemoryProperty() override {
        throw std::runtime_error(
            "MemoryManager::getMemoryProperty is not supported. "
            "Use GetSpan() to access the underlying memory.");
    }
};

} // namespace System::Buffers
