// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdlib>
#include <cstring>
#include <limits>
#include "System/ArgumentException.hpp"
#include "System/OutOfMemoryException.hpp"

namespace System::Runtime::InteropServices {

    /**
     * @brief Thin wrappers over the C allocator, with alignment support.
     *
     * C++ counterpart of .NET System.Runtime.InteropServices.NativeMemory.
     * All methods are static. `nuint` (pointer-sized unsigned integer) parameters
     * in the .NET API map to `size_t` here, matching this project's other raw-pointer-
     * arithmetic-facing headers (e.g. Span/Base64).
     */
    class NativeMemory {
        static void checkPow2Alignment(size_t alignment) {
            if (alignment == 0 || (alignment & (alignment - 1)) != 0)
                throw System::ArgumentException("Alignment must be a power of two.", "alignment");
        }

        // Rounds byteCount up to the next multiple of alignment (both already validated/adjusted
        // by the caller), matching real .NET's NativeMemory.AlignedAlloc/AlignedRealloc adjustment
        // -- C11 aligned_alloc/posix_memalign require size to be a multiple of alignment.
        static size_t roundUpToAlignment(size_t byteCount, size_t alignment) {
            return (byteCount != 0) ? ((byteCount + (alignment - 1)) & ~(alignment - 1)) : alignment;
        }

    public:
        NativeMemory() = delete;

        /**
         * @brief Allocates a block of memory of the specified size, in bytes.
         * C++ counterpart of .NET NativeMemory.Alloc(nuint). Thin wrapper over C `malloc`.
         * @throws System::OutOfMemoryException if allocation fails.
         */
        static void* Alloc(size_t byteCount) {
            void* result = std::malloc(byteCount != 0 ? byteCount : 1);
            if (result == nullptr) throw System::OutOfMemoryException();
            return result;
        }

        /**
         * @brief Allocates a block of memory of the specified size, in elements.
         * C++ counterpart of .NET NativeMemory.Alloc(nuint, nuint).
         * @throws System::OutOfMemoryException if @p elementCount * @p elementSize overflows or allocation fails.
         */
        static void* Alloc(size_t elementCount, size_t elementSize) {
            if (elementSize != 0 && elementCount > (std::numeric_limits<size_t>::max)() / elementSize)
                throw System::OutOfMemoryException();
            return Alloc(elementCount * elementSize);
        }

        /**
         * @brief Allocates and zeroes a block of memory of the specified size, in bytes.
         * C++ counterpart of .NET NativeMemory.AllocZeroed(nuint). Thin wrapper over C `calloc`.
         * @throws System::OutOfMemoryException if allocation fails.
         */
        static void* AllocZeroed(size_t byteCount) {
            return AllocZeroed(byteCount != 0 ? byteCount : 1, 1);
        }

        /**
         * @brief Allocates and zeroes a block of memory of the specified size, in elements.
         * C++ counterpart of .NET NativeMemory.AllocZeroed(nuint, nuint). Thin wrapper over C `calloc`.
         * @throws System::OutOfMemoryException if allocation fails.
         */
        static void* AllocZeroed(size_t elementCount, size_t elementSize) {
            void* result = (elementCount != 0 && elementSize != 0)
                ? std::calloc(elementCount, elementSize)
                : std::malloc(1);
            if (result == nullptr) throw System::OutOfMemoryException();
            return result;
        }

        /**
         * @brief Reallocates a block of memory to the specified size, in bytes.
         * C++ counterpart of .NET NativeMemory.Realloc(void*, nuint). Acts as Alloc() if @p ptr is null.
         * @throws System::OutOfMemoryException if reallocation fails.
         */
        static void* Realloc(void* ptr, size_t byteCount) {
            void* result = std::realloc(ptr, byteCount != 0 ? byteCount : 1);
            if (result == nullptr) throw System::OutOfMemoryException();
            return result;
        }

        /**
         * @brief Frees a block of memory. No-op if @p ptr is null.
         * C++ counterpart of .NET NativeMemory.Free(void*). Thin wrapper over C `free`.
         */
        static void Free(void* ptr) {
            std::free(ptr);
        }

        /**
         * @brief Allocates an aligned block of memory of the specified size and alignment, in bytes.
         *
         * C++ counterpart of .NET NativeMemory.AlignedAlloc(nuint, nuint). Not compatible with
         * Free()/Realloc() -- use AlignedFree()/AlignedRealloc() instead.
         * @throws System::ArgumentException if @p alignment is not a power of two.
         * @throws System::OutOfMemoryException if allocation fails.
         */
        static void* AlignedAlloc(size_t byteCount, size_t alignment) {
            checkPow2Alignment(alignment);
            size_t adjustedAlignment = alignment > sizeof(void*) ? alignment : sizeof(void*);
            size_t adjustedByteCount = roundUpToAlignment(byteCount, adjustedAlignment);
            if (adjustedByteCount < byteCount) throw System::OutOfMemoryException();

#if defined(_WIN32)
            void* result = _aligned_malloc(adjustedByteCount, adjustedAlignment);
#else
            void* result = std::aligned_alloc(adjustedAlignment, adjustedByteCount);
#endif
            if (result == nullptr) throw System::OutOfMemoryException();
            return result;
        }

        /**
         * @brief Frees an aligned block of memory allocated via AlignedAlloc()/AlignedRealloc(). No-op if @p ptr is null.
         * C++ counterpart of .NET NativeMemory.AlignedFree(void*).
         */
        static void AlignedFree(void* ptr) {
            if (ptr == nullptr) return;
#if defined(_WIN32)
            _aligned_free(ptr);
#else
            std::free(ptr);
#endif
        }

        /**
         * @brief Reallocates an aligned block of memory to the specified size and alignment, in bytes.
         *
         * C++ counterpart of .NET NativeMemory.AlignedRealloc(void*, nuint, nuint). Acts as
         * AlignedAlloc() if @p ptr is null. This port has no portable POSIX aligned-realloc
         * primitive to delegate to (glibc/BSD offer no `aligned_realloc`, unlike Windows'
         * `_aligned_realloc`), so it allocates fresh via AlignedAlloc(), copies the lesser of the
         * old/new sizes, and frees the original -- functionally equivalent, at the cost of always
         * moving even when an in-place growth would have been possible. Since the true prior
         * allocation size isn't tracked, @p byteCount is used as the copy bound when growing (the
         * caller-supplied new size), which is always safe as an upper bound for what could have
         * been written to the smaller old block.
         * @throws System::ArgumentException if @p alignment is not a power of two.
         * @throws System::OutOfMemoryException if reallocation fails.
         */
        static void* AlignedRealloc(void* ptr, size_t byteCount, size_t alignment) {
            if (ptr == nullptr) return AlignedAlloc(byteCount, alignment);
            checkPow2Alignment(alignment);
#if defined(_WIN32)
            size_t adjustedAlignment = alignment > sizeof(void*) ? alignment : sizeof(void*);
            size_t adjustedByteCount = roundUpToAlignment(byteCount, adjustedAlignment);
            if (adjustedByteCount < byteCount) throw System::OutOfMemoryException();
            void* result = _aligned_realloc(ptr, adjustedByteCount, adjustedAlignment);
            if (result == nullptr) throw System::OutOfMemoryException();
            return result;
#else
            void* result = AlignedAlloc(byteCount, alignment);
            std::memcpy(result, ptr, byteCount);
            std::free(ptr);
            return result;
#endif
        }

        /**
         * @brief Clears a block of memory to zero.
         * C++ counterpart of .NET NativeMemory.Clear(void*, nuint).
         */
        static void Clear(void* ptr, size_t byteCount) {
            std::memset(ptr, 0, byteCount);
        }

        /**
         * @brief Copies a block of memory from @p source to @p destination. Regions may overlap.
         * C++ counterpart of .NET NativeMemory.Copy(void*, void*, nuint).
         */
        static void Copy(void* source, void* destination, size_t byteCount) {
            std::memmove(destination, source, byteCount);
        }

        /**
         * @brief Sets the first @p byteCount bytes at @p ptr to @p value.
         * C++ counterpart of .NET NativeMemory.Fill(void*, nuint, byte).
         */
        static void Fill(void* ptr, size_t byteCount, uint8_t value) {
            std::memset(ptr, value, byteCount);
        }
    };

} // namespace System::Runtime::InteropServices
