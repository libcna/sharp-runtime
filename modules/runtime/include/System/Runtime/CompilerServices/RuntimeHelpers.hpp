// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Delegate.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include "System/Range.hpp"
#include "System/ReadOnlySpan.hpp"
#include "System/RuntimeFieldHandle.hpp"
#include "System/RuntimeMethodHandle.hpp"
#include "System/RuntimeTypeHandle.hpp"
#include "System/ModuleHandle.hpp"
#include "System/Type.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Provides low-level helpers normally supplied by the CLR.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.RuntimeHelpers. Operations with a
 * direct C++ meaning (identity, subarrays, cleanup callbacks, and reference-containing type
 * detection) are implemented. APIs that require CLR metadata, IL field data, or the CLR type
 * loader throw PlatformNotSupportedException rather than fabricating a result.
 */
class RuntimeHelpers final {
    template<typename T>
    struct IsSmartPointer : std::false_type {};

    template<typename T>
    struct IsSmartPointer<std::shared_ptr<T>> : std::true_type {};

    template<typename T>
    struct IsSmartPointer<std::weak_ptr<T>> : std::true_type {};

    template<typename T>
    struct IsSmartPointer<std::unique_ptr<T>> : std::true_type {};

public:
    /** Callback invoked by ExecuteCodeWithGuaranteedCleanup. */
    using TryCode = std::function<void(void*)>;

    /** Cleanup callback invoked after TryCode, including when TryCode throws. */
    using CleanupCode = std::function<void(void*, bool)>;

    RuntimeHelpers() = delete;

    /**
     * Gets the deprecated CLR string-data offset.
     * C++ `std::string` has no stable object layout, so this always returns zero.
     */
    [[nodiscard]] static SharpRuntime::intcs getOffsetToStringDataProperty() noexcept { return 0; }

    /**
     * Allocates CLR type-associated memory.
     *
     * Not supported: C++ has neither CLR type-loader lifetime nor unloadable CLR types.
     */
    [[noreturn]] static void* AllocateTypeAssociatedMemory(
        const System::Type&, SharpRuntime::intcs) {
        throw System::PlatformNotSupportedException(
            "RuntimeHelpers.AllocateTypeAssociatedMemory requires CLR type-loader support.");
    }

    /** Allocates aligned CLR type-associated memory; unsupported for the same reason. */
    [[noreturn]] static void* AllocateTypeAssociatedMemory(
        const System::Type&, SharpRuntime::intcs, SharpRuntime::intcs) {
        throw System::PlatformNotSupportedException(
            "RuntimeHelpers.AllocateTypeAssociatedMemory requires CLR type-loader support.");
    }

    /**
     * Boxes a byte using a CLR runtime type handle.
     * Not supported because C++ has no CLR boxing or runtime type metadata.
     */
    [[noreturn]] static std::any Box(SharpRuntime::bytecs&, const System::RuntimeTypeHandle&) {
        throw System::PlatformNotSupportedException(
            "RuntimeHelpers.Box requires CLR boxing support.");
    }

    /**
     * Creates a read-only span over an RVA-backed CLR field.
     * Not supported because RuntimeFieldHandle contains no C++ field-data address.
     */
    template<typename T>
    [[noreturn]] static System::ReadOnlySpan<T> CreateSpan(const System::RuntimeFieldHandle&) {
        throw System::PlatformNotSupportedException(
            "RuntimeHelpers.CreateSpan requires CLR RVA field metadata.");
    }

    /**
     * Ensures sufficient CLR execution stack space.
     * C++ does not expose a portable stack probe, so this is a no-op.
     */
    static void EnsureSufficientExecutionStack() noexcept {}

    /** Determines whether two raw object addresses have the same identity. */
    [[nodiscard]] static bool Equals(const void* first, const void* second) noexcept {
        return first == second;
    }

    /** Determines whether two shared object references have the same identity. */
    template<typename T>
    [[nodiscard]] static bool Equals(const std::shared_ptr<T>& first,
                                     const std::shared_ptr<T>& second) noexcept {
        return first.get() == second.get();
    }

    /**
     * Executes a callback and guarantees invocation of cleanup after it returns or throws.
     * @param code Main callback; must not be empty.
     * @param backoutCode Cleanup callback; must not be empty.
     * @param userData Caller-provided opaque context.
     */
    static void ExecuteCodeWithGuaranteedCleanup(const TryCode& code,
                                                 const CleanupCode& backoutCode,
                                                 void* userData) {
        if (!code) {
            throw System::ArgumentNullException("code");
        }
        if (!backoutCode) {
            throw System::ArgumentNullException("backoutCode");
        }

        bool exceptionThrown = true;
        try {
            code(userData);
            exceptionThrown = false;
        } catch (...) {
            backoutCode(userData, exceptionThrown);
            throw;
        }
        backoutCode(userData, exceptionThrown);
    }

    /** Returns a hash based on an object's raw address, or zero for a null address. */
    [[nodiscard]] static SharpRuntime::intcs GetHashCode(const void* object) noexcept {
        return object == nullptr
            ? 0
            : static_cast<SharpRuntime::intcs>(std::hash<const void*>{}(object));
    }

    /** Returns a hash based on a shared object's identity, or zero for a null reference. */
    template<typename T>
    [[nodiscard]] static SharpRuntime::intcs GetHashCode(const std::shared_ptr<T>& object) noexcept {
        return GetHashCode(object.get());
    }

    /** Returns an object value unchanged, copying value types as in the CLR API. */
    template<typename T>
    [[nodiscard]] static T GetObjectValue(T object) {
        return object;
    }

    /**
     * Returns a copy of the specified half-open array range.
     * @throws System::ArgumentOutOfRangeException when the range falls outside the source array.
     */
    template<typename T>
    [[nodiscard]] static std::vector<T> GetSubArray(const std::vector<T>& array,
                                                     const System::Range& range) {
        const auto offsetAndLength = range.GetOffsetAndLength(
            static_cast<SharpRuntime::intcs>(array.size()));
        const auto begin = array.begin() + offsetAndLength.Offset;
        return std::vector<T>(begin, begin + offsetAndLength.Length);
    }

    /**
     * Returns an uninitialized CLR object.
     * Not supported because constructing an arbitrary C++ type from System::Type requires reflection.
     */
    [[noreturn]] static std::any GetUninitializedObject(const System::Type&) {
        throw System::PlatformNotSupportedException(
            "RuntimeHelpers.GetUninitializedObject requires CLR reflection support.");
    }

    /**
     * Initializes an array from an RVA-backed CLR field.
     * Not supported because C++ RuntimeFieldHandle values carry no field data.
     */
    template<typename T>
    [[noreturn]] static void InitializeArray(std::vector<T>&, const System::RuntimeFieldHandle&) {
        throw System::PlatformNotSupportedException(
            "RuntimeHelpers.InitializeArray requires CLR RVA field metadata.");
    }

    /**
     * Determines whether T is a managed reference type or contains managed references.
     *
     * `std::shared_ptr`, `std::weak_ptr`, and `std::unique_ptr` model reference-containing values.
     * C++ cannot inspect the fields of arbitrary user-defined types, so non-trivially-copyable
     * types are conservatively reported as containing references.
     */
    template<typename T>
    [[nodiscard]] static constexpr bool IsReferenceOrContainsReferences() noexcept {
        return IsSmartPointer<std::remove_cv_t<T>>::value || !std::is_trivially_copyable_v<T>;
    }

    /** C++ has no CER preparation phase, so this method does nothing. */
    static void PrepareConstrainedRegions() noexcept {}

    /** C++ has no CER preparation phase, so this method does nothing. */
    static void PrepareConstrainedRegionsNoOP() noexcept {}

    /** C++ has no CLR delegate preparation phase, so this method does nothing. */
    static void PrepareContractedDelegate(const System::Delegate&) noexcept {}

    /** C++ has no JIT delegate preparation phase, so this method does nothing. */
    static void PrepareDelegate(const System::Delegate&) noexcept {}

    /** C++ has no CLR method-preparation phase, so this method does nothing. */
    static void PrepareMethod(const System::RuntimeMethodHandle&) noexcept {}

    /** C++ has no CLR generic method-instantiation phase, so this method does nothing. */
    static void PrepareMethod(const System::RuntimeMethodHandle&,
                              const std::vector<System::RuntimeTypeHandle>&) noexcept {}

    /** C++ has no portable CLR-equivalent stack probe, so this method does nothing. */
    static void ProbeForSufficientStack() noexcept {}

    /** C++ static initialization runs automatically, so this method does nothing. */
    static void RunClassConstructor(const System::RuntimeTypeHandle&) noexcept {}

    /** C++ static initialization runs automatically, so this method does nothing. */
    static void RunModuleConstructor(const System::ModuleHandle&) noexcept {}

    /** Returns the CLR size of a type handle; unsupported without CLR type metadata. */
    [[noreturn]] static SharpRuntime::intcs SizeOf(const System::RuntimeTypeHandle&) {
        throw System::PlatformNotSupportedException(
            "RuntimeHelpers.SizeOf requires CLR type metadata.");
    }

    /** C++ has no portable CLR-equivalent stack probe, so this conservatively returns true. */
    [[nodiscard]] static bool TryEnsureSufficientExecutionStack() noexcept { return true; }
};

} // namespace System::Runtime::CompilerServices
