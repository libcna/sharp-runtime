// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Attribute.hpp"
#include "System/Index.hpp"
#include "System/Range.hpp"
#include "System/Runtime/CompilerServices/AsyncStateMachineAttribute.hpp"
#include "System/Runtime/CompilerServices/MethodImplOptions.hpp"
#include "System/Runtime/CompilerServices/MethodImplAttribute.hpp"
#include "System/Runtime/CompilerServices/MethodCodeType.hpp"
#include "System/Runtime/CompilerServices/CompilerFeatureRequiredAttribute.hpp"
#include "System/Runtime/CompilerServices/ConditionalWeakTable.hpp"
#include "System/Runtime/CompilerServices/CallerAttributes.hpp"
#include "System/Runtime/CompilerServices/CompilerGeneratedAttribute.hpp"
#include "System/Runtime/CompilerServices/EnumeratorCancellationAttribute.hpp"
#include "System/Runtime/CompilerServices/ExtensionAttribute.hpp"
#include "System/Runtime/CompilerServices/FormattableStringFactory.hpp"
#include "System/Runtime/CompilerServices/InterpolatedStringHandlerAttribute.hpp"
#include "System/Runtime/CompilerServices/IsExternalInit.hpp"
#include "System/Runtime/CompilerServices/IteratorStateMachineAttribute.hpp"
#include "System/Runtime/CompilerServices/ModuleInitializerAttribute.hpp"
#include "System/Runtime/CompilerServices/RequiredMemberAttribute.hpp"
#include "System/Runtime/CompilerServices/RuntimeHelpers.hpp"
#include "System/Runtime/CompilerServices/SkipLocalsInitAttribute.hpp"
#include "System/Runtime/CompilerServices/StateMachineAttribute.hpp"
#include "System/Runtime/GCSettings.hpp"
#include "System/Runtime/AmbiguousImplementationException.hpp"
#include "System/SystemException.hpp"
#include "System/Exception.hpp"
#include <exception>
#include "System/Runtime/InteropServices/InteropAttributes.hpp"
#include "System/Runtime/InteropServices/ExternalException.hpp"
#include "System/Runtime/Versioning/VersioningAttributes.hpp"

using namespace System::Runtime::CompilerServices;
using namespace System::Runtime::InteropServices;
using namespace System::Runtime::Versioning;
using System::Runtime::GCSettings;
using System::Runtime::GCLatencyMode;
using System::Runtime::GCLargeObjectHeapCompactionMode;
using System::Runtime::AmbiguousImplementationException;

// ===========================================================================
// MethodImplOptions
// ===========================================================================

TEST(MethodImplOptionsTests, NoInlining_Value) {
    EXPECT_EQ(static_cast<int>(MethodImplOptions::NoInlining), 0x0008);
}

TEST(MethodImplOptionsTests, AggressiveInlining_Value) {
    EXPECT_EQ(static_cast<int>(MethodImplOptions::AggressiveInlining), 0x0100);
}

TEST(MethodImplOptionsTests, Synchronized_Value) {
    EXPECT_EQ(static_cast<int>(MethodImplOptions::Synchronized), 0x0020);
}

TEST(MethodImplOptionsTests, OrOperator_CombinesFlags) {
    auto combined = MethodImplOptions::NoInlining | MethodImplOptions::NoOptimization;
    EXPECT_EQ(static_cast<int>(combined), 0x0008 | 0x0040);
}

TEST(MethodImplOptionsTests, Async_Value) {
    EXPECT_EQ(static_cast<int>(MethodImplOptions::Async), 0x2000);
}

TEST(MethodCodeTypeTests, Values_MatchDotNet) {
    EXPECT_EQ(static_cast<int>(MethodCodeType::IL), 0);
    EXPECT_EQ(static_cast<int>(MethodCodeType::Native), 1);
    EXPECT_EQ(static_cast<int>(MethodCodeType::OPTIL), 2);
    EXPECT_EQ(static_cast<int>(MethodCodeType::Runtime), 3);
}

// ===========================================================================
// MethodImplAttribute
// ===========================================================================

TEST(MethodImplAttributeTests, Constructor_EnumValue) {
    MethodImplAttribute attr(MethodImplOptions::AggressiveInlining);
    EXPECT_EQ(attr.getValueProperty(), MethodImplOptions::AggressiveInlining);
}

TEST(MethodImplAttributeTests, Constructor_IntValue) {
    MethodImplAttribute attr(static_cast<SharpRuntime::shortcs>(0x0008));
    EXPECT_EQ(attr.getValueProperty(), MethodImplOptions::NoInlining);
}

TEST(MethodImplAttributeTests, DefaultConstructor_UsesDefaultOptionsAndILCodeType) {
    MethodImplAttribute attr;
    EXPECT_EQ(static_cast<int>(attr.getValueProperty()), 0);
    EXPECT_EQ(attr.getMethodCodeTypeProperty(), MethodCodeType::IL);
}

TEST(MethodImplAttributeTests, MethodCodeType_IsMutableMetadata) {
    MethodImplAttribute attr;
    attr.setMethodCodeTypeProperty(MethodCodeType::Native);
    EXPECT_EQ(attr.getMethodCodeTypeProperty(), MethodCodeType::Native);
}

// ===========================================================================
// Compiler metadata attributes
// ===========================================================================

TEST(StateMachineAttributeTests, StateMachineType_IsRetained) {
    StateMachineAttribute attr(System::Type::From<int>());
    EXPECT_EQ(attr.getStateMachineTypeProperty().getNameProperty(),
              System::Type::From<int>().getNameProperty());
}

TEST(StateMachineAttributeTests, AsyncAndIteratorAttributes_InheritStateMachineType) {
    AsyncStateMachineAttribute asyncAttr(System::Type::From<double>());
    IteratorStateMachineAttribute iteratorAttr(System::Type::From<char>());
    EXPECT_EQ(asyncAttr.getStateMachineTypeProperty().getNameProperty(),
              System::Type::From<double>().getNameProperty());
    EXPECT_EQ(iteratorAttr.getStateMachineTypeProperty().getNameProperty(),
              System::Type::From<char>().getNameProperty());
}

namespace {
    /// Dependent on purpose: gcc evaluates a NON-dependent `requires` eagerly and hard-errors on
    /// the missing member instead of yielding false (the #2299 trap).
    template <typename T>
    concept HasSetIsOptional = requires(T a) { a.setIsOptionalProperty(true); };
}

TEST(CompilerFeatureRequiredAttributeTests, StoresFeatureNameAndOptionalFlag) {
    // MIGRATED by #1980 group G-4 / SR-AUD-160. The old body called setIsOptionalProperty, which
    // published a mutability .NET does not have: its IsOptional is `{ get; init; }` -- settable at
    // construction, immutable afterwards. The value is now supplied through the constructor.
    CompilerFeatureRequiredAttribute attr("custom-feature");
    EXPECT_EQ(attr.getFeatureNameProperty(), "custom-feature");
    EXPECT_FALSE(attr.getIsOptionalProperty());

    CompilerFeatureRequiredAttribute optional("custom-feature", true);
    EXPECT_EQ(optional.getFeatureNameProperty(), "custom-feature");
    EXPECT_TRUE(optional.getIsOptionalProperty());

    EXPECT_EQ(CompilerFeatureRequiredAttribute::RefStructs, "RefStructs");
    EXPECT_EQ(CompilerFeatureRequiredAttribute::RequiredMembers, "RequiredMembers");
}

TEST(CompilerFeatureRequiredAttributeTests, Decl1980G4_IsOptionalIsNotMutableAfterConstruction) {
    // `init` is settable-at-construction AND immutable-afterwards. Removing the setter without
    // adding the constructor would have been a narrowing; keeping the setter would have published
    // mutability .NET lacks. This asserts both halves at once.
    static_assert(std::is_constructible_v<CompilerFeatureRequiredAttribute, std::string, bool>,
                  "#1980 G-4: the value must still be settable at construction");
    static_assert(!HasSetIsOptional<CompilerFeatureRequiredAttribute>,
                  "#1980 G-4: .NET's IsOptional is init-only, so there is no setter");
}

TEST(CompilerMetadataMarkerAttributeTests, MarkersInstantiateAndDeriveFromAttribute) {
    ExtensionAttribute extension;
    RequiredMemberAttribute required;
    EnumeratorCancellationAttribute cancellation;
    ModuleInitializerAttribute initializer;
    SkipLocalsInitAttribute skipLocals;
    InterpolatedStringHandlerAttribute handler;
    (void)extension;
    (void)required;
    (void)cancellation;
    (void)initializer;
    (void)skipLocals;
    (void)handler;
    EXPECT_TRUE((std::is_base_of_v<System::Attribute, ExtensionAttribute>));
    EXPECT_TRUE((std::is_base_of_v<System::Attribute, RequiredMemberAttribute>));
    EXPECT_FALSE((std::is_default_constructible_v<IsExternalInit>));
}

// ===========================================================================
// ConditionalWeakTable
// ===========================================================================

namespace {
struct ConditionalWeakTableKey {
    int id = 0;
};

struct ConditionalWeakTableValue {
    int value = 0;
};
} // namespace

using ConditionalWeakTableUnderTest =
    ConditionalWeakTable<ConditionalWeakTableKey, ConditionalWeakTableValue>;

TEST(ConditionalWeakTableTests, AddTryGetAndRemove_UseObjectIdentity) {
    ConditionalWeakTableUnderTest table;
    auto key = std::make_shared<ConditionalWeakTableKey>();
    auto value = std::make_shared<ConditionalWeakTableValue>();
    value->value = 42;

    table.Add(key, value);
    std::shared_ptr<ConditionalWeakTableValue> actual;
    EXPECT_TRUE(table.TryGetValue(key, actual));
    EXPECT_EQ(actual, value);
    EXPECT_THROW(table.Add(key, value), System::ArgumentException);

    std::shared_ptr<ConditionalWeakTableValue> removed;
    EXPECT_TRUE(table.Remove(key, removed));
    EXPECT_EQ(removed, value);
    EXPECT_FALSE(table.TryGetValue(key, actual));
    EXPECT_EQ(actual, nullptr);
}

TEST(ConditionalWeakTableTests, NullKey_ThrowsArgumentNullException) {
    ConditionalWeakTableUnderTest table;
    std::shared_ptr<ConditionalWeakTableKey> key;
    auto value = std::make_shared<ConditionalWeakTableValue>();
    std::shared_ptr<ConditionalWeakTableValue> actual;
    EXPECT_THROW(table.Add(key, value), System::ArgumentNullException);
    EXPECT_THROW(table.TryGetValue(key, actual), System::ArgumentNullException);
    EXPECT_THROW(table.Remove(key), System::ArgumentNullException);
}

TEST(ConditionalWeakTableTests, AddOrUpdateAndTryAdd_MatchConditionalTableSemantics) {
    ConditionalWeakTableUnderTest table;
    auto key = std::make_shared<ConditionalWeakTableKey>();
    auto first = std::make_shared<ConditionalWeakTableValue>();
    auto second = std::make_shared<ConditionalWeakTableValue>();
    first->value = 1;
    second->value = 2;

    EXPECT_TRUE(table.TryAdd(key, first));
    EXPECT_FALSE(table.TryAdd(key, second));
    table.AddOrUpdate(key, second);
    std::shared_ptr<ConditionalWeakTableValue> actual;
    EXPECT_TRUE(table.TryGetValue(key, actual));
    EXPECT_EQ(actual, second);
}

TEST(ConditionalWeakTableTests, GetOrAddFactory_RunsOutsideLockAndPreservesExistingValue) {
    ConditionalWeakTableUnderTest table;
    auto key = std::make_shared<ConditionalWeakTableKey>();
    auto relatedKey = std::make_shared<ConditionalWeakTableKey>();
    auto relatedValue = std::make_shared<ConditionalWeakTableValue>();
    relatedValue->value = 7;
    table.Add(relatedKey, relatedValue);

    int factoryCalls = 0;
    auto actual = table.GetOrAdd(key, [&](const auto&) {
        ++factoryCalls;
        return table.GetOrAdd(relatedKey, std::make_shared<ConditionalWeakTableValue>());
    });
    EXPECT_EQ(factoryCalls, 1);
    EXPECT_EQ(actual, relatedValue);

    auto replacement = std::make_shared<ConditionalWeakTableValue>();
    EXPECT_EQ(table.GetOrAdd(key, replacement), relatedValue);
    EXPECT_EQ(factoryCalls, 1);
}

TEST(ConditionalWeakTableTests, ExpiredKeys_AreNotEnumerated) {
    ConditionalWeakTableUnderTest table;
    auto value = std::make_shared<ConditionalWeakTableValue>();
    {
        auto key = std::make_shared<ConditionalWeakTableKey>();
        table.Add(key, value);
    }

    std::unique_ptr<System::Collections::Generic::IEnumerator<ConditionalWeakTableUnderTest::Pair>>
        enumerator(table.GetEnumerator());
    EXPECT_FALSE(enumerator->MoveNext());
}

TEST(ConditionalWeakTableTests, Enumerator_DoesNotIncludeEntriesAddedAfterCreation) {
    ConditionalWeakTableUnderTest table;
    auto firstKey = std::make_shared<ConditionalWeakTableKey>();
    auto secondKey = std::make_shared<ConditionalWeakTableKey>();
    auto firstValue = std::make_shared<ConditionalWeakTableValue>();
    auto secondValue = std::make_shared<ConditionalWeakTableValue>();
    table.Add(firstKey, firstValue);

    std::unique_ptr<System::Collections::Generic::IEnumerator<ConditionalWeakTableUnderTest::Pair>>
        enumerator(table.GetEnumerator());
    table.Add(secondKey, secondValue);
    ASSERT_TRUE(enumerator->MoveNext());
    EXPECT_EQ(enumerator->Current().Key, firstKey);
    EXPECT_FALSE(enumerator->MoveNext());
}

TEST(ConditionalWeakTableTests, GetOrAdd_ConcurrentCallsReturnTheStoredValue) {
    ConditionalWeakTableUnderTest table;
    auto key = std::make_shared<ConditionalWeakTableKey>();
    std::vector<std::shared_ptr<ConditionalWeakTableValue>> results(8);
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < results.size(); ++index) {
        threads.emplace_back([&table, &key, &results, index]() {
            results[index] = table.GetOrAdd(key, [index](const auto&) {
                auto value = std::make_shared<ConditionalWeakTableValue>();
                value->value = static_cast<int>(index);
                return value;
            });
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    for (const auto& result : results) {
        EXPECT_EQ(result, results.front());
    }
}

// ===========================================================================
// RuntimeHelpers
// ===========================================================================

TEST(RuntimeHelpersTests, IdentityHashAndObjectValue_FollowCppObjectIdentity) {
    auto object = std::make_shared<ConditionalWeakTableValue>();
    auto sameObject = object;
    auto otherObject = std::make_shared<ConditionalWeakTableValue>();
    EXPECT_TRUE(RuntimeHelpers::Equals(object, sameObject));
    EXPECT_FALSE(RuntimeHelpers::Equals(object, otherObject));
    // The removed EXPECT_NE(..., 0) was not a contract: zero is a legal hash code
    // (docs/HashAssertionContractRule.md R6), and the only zero RuntimeHelpers::GetHashCode
    // documents is the null one, asserted below. What an identity hash does owe is that the same
    // reference hashes the same, which is what replaces it.
    EXPECT_EQ(RuntimeHelpers::GetHashCode(object), RuntimeHelpers::GetHashCode(sameObject));
    EXPECT_EQ(RuntimeHelpers::GetHashCode(std::shared_ptr<ConditionalWeakTableValue>{}), 0);
    EXPECT_EQ(RuntimeHelpers::GetObjectValue(object), object);
    EXPECT_EQ(RuntimeHelpers::GetObjectValue(5), 5);
}

TEST(RuntimeHelpersTests, GetSubArray_UsesRangeValidationAndHalfOpenRange) {
    const std::vector<int> source{1, 2, 3, 4};
    const auto result = RuntimeHelpers::GetSubArray(source, System::Range(System::Index(1), System::Index(3)));
    EXPECT_EQ(result, (std::vector<int>{2, 3}));
    EXPECT_THROW(RuntimeHelpers::GetSubArray(source, System::Range(System::Index(3), System::Index(1))),
                 System::ArgumentOutOfRangeException);
}

TEST(RuntimeHelpersTests, GuaranteedCleanup_ReceivesExceptionState) {
    bool normalCleanupSawException = true;
    RuntimeHelpers::ExecuteCodeWithGuaranteedCleanup(
        [](void*) {},
        [&normalCleanupSawException](void*, bool exceptionThrown) {
            normalCleanupSawException = exceptionThrown;
        },
        nullptr);
    EXPECT_FALSE(normalCleanupSawException);

    bool throwingCleanupSawException = false;
    EXPECT_THROW(RuntimeHelpers::ExecuteCodeWithGuaranteedCleanup(
                     [](void*) { throw std::runtime_error("expected"); },
                     [&throwingCleanupSawException](void*, bool exceptionThrown) {
                         throwingCleanupSawException = exceptionThrown;
                     },
                     nullptr),
                 std::runtime_error);
    EXPECT_TRUE(throwingCleanupSawException);
}

TEST(RuntimeHelpersTests, ReferenceContainingDetection_IsConservative) {
    EXPECT_FALSE((RuntimeHelpers::IsReferenceOrContainsReferences<int>()));
    EXPECT_TRUE((RuntimeHelpers::IsReferenceOrContainsReferences<std::shared_ptr<int>>()));
    EXPECT_TRUE((RuntimeHelpers::IsReferenceOrContainsReferences<std::string>()));
}

TEST(RuntimeHelpersTests, ClrMetadataOperations_ThrowPlatformNotSupported) {
    EXPECT_THROW(RuntimeHelpers::CreateSpan<int>(System::RuntimeFieldHandle{}),
                 System::PlatformNotSupportedException);
    EXPECT_THROW(RuntimeHelpers::SizeOf(System::RuntimeTypeHandle{}),
                 System::PlatformNotSupportedException);
}

// ===========================================================================
// Caller Attributes
// ===========================================================================

TEST(CallerAttributesTests, CallerMemberName_Instantiates) {
    CallerMemberNameAttribute attr;
    (void)attr;
    SUCCEED();
}

TEST(CallerAttributesTests, CallerFilePath_Instantiates) {
    CallerFilePathAttribute attr;
    (void)attr;
    SUCCEED();
}

TEST(CallerAttributesTests, CallerLineNumber_Instantiates) {
    CallerLineNumberAttribute attr;
    (void)attr;
    SUCCEED();
}

TEST(CallerAttributesTests, CallerArgumentExpression_StoresParameterName) {
    CallerArgumentExpressionAttribute attr("value");
    EXPECT_EQ(attr.getParameterNameProperty(), "value");
}

TEST(CallerAttributesTests, CallerArgumentExpression_EmptyName) {
    CallerArgumentExpressionAttribute attr("");
    EXPECT_EQ(attr.getParameterNameProperty(), "");
}

// ===========================================================================
// GCSettings
// ===========================================================================

TEST(GCSettingsTests, DefaultLatencyMode_IsInteractive) {
    // Reset to known state first (prior tests may have modified it)
    GCSettings::setLatencyModeProperty(GCLatencyMode::Interactive);
    EXPECT_EQ(GCSettings::getLatencyModeProperty(), GCLatencyMode::Interactive);
}

TEST(GCSettingsTests, SetLatencyMode_Batch) {
    GCSettings::setLatencyModeProperty(GCLatencyMode::Batch);
    EXPECT_EQ(GCSettings::getLatencyModeProperty(), GCLatencyMode::Batch);
    GCSettings::setLatencyModeProperty(GCLatencyMode::Interactive); // restore
}

TEST(GCSettingsTests, SetLatencyMode_LowLatency) {
    GCSettings::setLatencyModeProperty(GCLatencyMode::LowLatency);
    EXPECT_EQ(GCSettings::getLatencyModeProperty(), GCLatencyMode::LowLatency);
    GCSettings::setLatencyModeProperty(GCLatencyMode::Interactive); // restore
}

TEST(GCSettingsTests, DefaultCompactionMode_IsDefault) {
    GCSettings::setLargeObjectHeapCompactionModeProperty(GCLargeObjectHeapCompactionMode::Default);
    EXPECT_EQ(GCSettings::getLargeObjectHeapCompactionModeProperty(),
              GCLargeObjectHeapCompactionMode::Default);
}

TEST(GCSettingsTests, SetCompactionMode_CompactOnce) {
    GCSettings::setLargeObjectHeapCompactionModeProperty(GCLargeObjectHeapCompactionMode::CompactOnce);
    EXPECT_EQ(GCSettings::getLargeObjectHeapCompactionModeProperty(),
              GCLargeObjectHeapCompactionMode::CompactOnce);
    GCSettings::setLargeObjectHeapCompactionModeProperty(GCLargeObjectHeapCompactionMode::Default);
}

TEST(GCSettingsTests, IsServerGC_DefaultFalse) {
    EXPECT_FALSE(GCSettings::getIsServerGCProperty());
}

TEST(GCLatencyModeTests, Batch_IsZero) {
    EXPECT_EQ(static_cast<int>(GCLatencyMode::Batch), 0);
}

TEST(GCLatencyModeTests, Interactive_IsOne) {
    EXPECT_EQ(static_cast<int>(GCLatencyMode::Interactive), 1);
}

TEST(GCLatencyModeTests, NoGCRegion_IsFour) {
    EXPECT_EQ(static_cast<int>(GCLatencyMode::NoGCRegion), 4);
}

// ===========================================================================
// AmbiguousImplementationException
// ===========================================================================

TEST(AmbiguousImplementationExceptionTests, DefaultMessage) {
    AmbiguousImplementationException ex;
    EXPECT_STREQ(ex.what(), "Ambiguous implementation found.");
}

TEST(AmbiguousImplementationExceptionTests, CustomMessage) {
    AmbiguousImplementationException ex("Custom ambiguous message");
    EXPECT_STREQ(ex.what(), "Custom ambiguous message");
}

TEST(AmbiguousImplementationExceptionTests, IsThrowable) {
    EXPECT_THROW(throw AmbiguousImplementationException(), AmbiguousImplementationException);
}

// ===========================================================================
// InteropAttributes — enums
// ===========================================================================

TEST(LayoutKindTests, Sequential_IsZero) {
    EXPECT_EQ(static_cast<int>(LayoutKind::Sequential), 0);
}

TEST(LayoutKindTests, Explicit_IsTwo) {
    EXPECT_EQ(static_cast<int>(LayoutKind::Explicit), 2);
}

TEST(LayoutKindTests, Auto_IsThree) {
    EXPECT_EQ(static_cast<int>(LayoutKind::Auto), 3);
}

TEST(CharSetTests, Ansi_IsTwo) {
    EXPECT_EQ(static_cast<int>(CharSet::Ansi), 2);
}

TEST(CharSetTests, Unicode_IsThree) {
    EXPECT_EQ(static_cast<int>(CharSet::Unicode), 3);
}

TEST(CallingConventionTests, Cdecl_IsTwo) {
    EXPECT_EQ(static_cast<int>(CallingConvention::Cdecl), 2);
}

TEST(CallingConventionTests, StdCall_IsThree) {
    EXPECT_EQ(static_cast<int>(CallingConvention::StdCall), 3);
}

TEST(UnmanagedTypeTests, Bool_IsTwo) {
    EXPECT_EQ(static_cast<int>(UnmanagedType::Bool), 2);
}

TEST(UnmanagedTypeTests, LPStr_Is20) {
    EXPECT_EQ(static_cast<int>(UnmanagedType::LPStr), 20);
}

// ===========================================================================
// InteropAttributes — attribute classes
// ===========================================================================

TEST(StructLayoutAttributeTests, Constructor_StoresLayout) {
    StructLayoutAttribute attr(LayoutKind::Sequential);
    EXPECT_EQ(attr.Value, LayoutKind::Sequential);
}

TEST(StructLayoutAttributeTests, Fix1980G2_DefaultPackIsZeroNotEight) {
    // INVERTED by #1980 group G-2. The plan predicted this exact pin would have to be rewritten,
    // and it is the only pre-existing test the group touches. .NET declares `public int Pack;`
    // with no initializer (StructLayoutAttribute.cs:21), so the default is 0 -- which in the
    // metadata means "use the runtime's default packing", a different statement from "pack to 8".
    StructLayoutAttribute attr(LayoutKind::Explicit);
    EXPECT_EQ(attr.Pack, 0);
}

TEST(StructLayoutAttributeTests, DefaultSize_IsZero) {
    StructLayoutAttribute attr(LayoutKind::Auto);
    EXPECT_EQ(attr.Size, 0);
}

TEST(FieldOffsetAttributeTests, Constructor_StoresOffset) {
    FieldOffsetAttribute attr(16);
    EXPECT_EQ(attr.Value, 16);
}

TEST(MarshalAsAttributeTests, Constructor_StoresType) {
    // MIGRATED by #1980 group G-5 / SR-AUD-167: `Value` was a public MUTABLE data member; .NET's
    // is `public UnmanagedType Value { get; }` -- get-only, set once by the constructor.
    MarshalAsAttribute attr(UnmanagedType::LPStr);
    EXPECT_EQ(attr.getValueProperty(), UnmanagedType::LPStr);
}

TEST(MarshalAsAttributeTests, DefaultSizeConst_IsZero) {
    MarshalAsAttribute attr(UnmanagedType::ByValArray);
    EXPECT_EQ(attr.SizeConst, 0);
}

TEST(DllImportAttributeTests, Constructor_StoresDllName) {
    DllImportAttribute attr("kernel32.dll");
    EXPECT_EQ(attr.Value, "kernel32.dll");
}

TEST(DllImportAttributeTests, DefaultSetLastError_False) {
    DllImportAttribute attr("mylib.dll");
    EXPECT_FALSE(attr.SetLastError);
}

TEST(ComVisibleAttributeTests, Constructor_True) {
    ComVisibleAttribute attr(true);
    EXPECT_TRUE(attr.Value);
}

TEST(ComVisibleAttributeTests, Constructor_False) {
    ComVisibleAttribute attr(false);
    EXPECT_FALSE(attr.Value);
}

TEST(GuidAttributeTests, Constructor_StoresGuid) {
    GuidAttribute attr("12345678-1234-1234-1234-123456789012");
    EXPECT_EQ(attr.Value, "12345678-1234-1234-1234-123456789012");
}

TEST(MarkerAttributeTests, InAttribute_Instantiates) {
    InAttribute attr;
    (void)attr;
    SUCCEED();
}

TEST(MarkerAttributeTests, OutAttribute_Instantiates) {
    OutAttribute attr;
    (void)attr;
    SUCCEED();
}

TEST(MarkerAttributeTests, OptionalAttribute_Instantiates) {
    OptionalAttribute attr;
    (void)attr;
    SUCCEED();
}

// ===========================================================================
// ExternalException
// ===========================================================================

namespace detail2387 {
    /// Detection idiom for `ToString()`. The parameter is DEPENDENT deliberately: gcc evaluates a
    /// non-dependent `requires` eagerly and hard-errors on a missing name instead of yielding
    /// false -- the trap #2299 recorded. An absent member must be expressible as absent.
    template <class T>
    concept HasToString = requires(T& t) { t.ToString(); };
}

TEST(ExternalExceptionTests, Decl2387_ToStringIsDeliberatelyAbsent) {
    // DECIDED 2026-08-19. .NET's ExternalException.ToString() is
    // $"{GetType()} (0x{HResult:X8})" plus the message and any inner exception, and GetType() is
    // the MOST DERIVED type -- reflection this port permanently lacks.
    //
    // #1980 IMPLEMENTED IT AND THEN REMOVED IT, on the downstream measurement SA-2 condition 5
    // requires: cna derives from this class in THREE types -- NoAudioHardwareException,
    // InstancePlayLimitException and StorageDeviceNotConnectedException -- and a statically
    // resolved name would have misnamed every one. That is #2323's own rule: a message naming the
    // WRONG type is a lie, where an absence is merely an absence.
    //
    // Both remaining routes were offered on 2026-08-19 and DECLINED: a virtual ToString() (a
    // vtable slot, the #2374 shape -- but there the slot bought an overridable policy nothing else
    // could provide, while here it would only relocate the naming problem into each of the three
    // downstream types) and a stored type name (a data member on a class three types derive from).
    //
    // This pin is the DECLARATION. If it ever fails, the decision was reversed and the header note
    // must be rewritten.
    static_assert(!detail2387::HasToString<System::Runtime::InteropServices::ExternalException>,
                  "#2387: ToString() is deliberately absent -- see the header for why, and for "
                  "the two routes that were declined");

    // The members that ARE present still work, so the absence is confined rather than a gap in
    // the type. ErrorCode is an ALIAS for HResult (.NET: `public virtual int ErrorCode => HResult`),
    // not a second field -- asserted here because a stored-name repair would have been tempted to
    // add one beside it.
    System::Runtime::InteropServices::ExternalException ex("boom", 0x80004005);
    EXPECT_EQ(ex.getErrorCodeProperty(), ex.getHResultProperty());
    EXPECT_EQ(ex.getErrorCodeProperty(), static_cast<SharpRuntime::intcs>(0x80004005));
    EXPECT_STREQ(ex.what(), "boom");
}

TEST(ExternalExceptionTests, DefaultMessage) {
    ExternalException ex;
    EXPECT_STREQ(ex.what(), "External component has thrown an exception.");
}

TEST(ExternalExceptionTests, CustomMessage) {
    ExternalException ex("Native call failed");
    EXPECT_STREQ(ex.what(), "Native call failed");
}

TEST(ExternalExceptionTests, MessageWithInner) {
    auto inner = std::make_exception_ptr(std::runtime_error("access denied"));
    ExternalException ex("P/Invoke error", inner);
    std::string msg = ex.what();
    EXPECT_NE(msg.find("P/Invoke error"), std::string::npos);
}

TEST(ExternalExceptionTests, IsThrowable) {
    EXPECT_THROW(throw ExternalException(), ExternalException);
}

// ===========================================================================
// VersioningAttributes
// ===========================================================================

TEST(TargetFrameworkAttributeTests, Constructor_StoresFrameworkName) {
    TargetFrameworkAttribute attr(".NETCoreApp,Version=v8.0");
    EXPECT_EQ(attr.getFrameworkNameProperty(), ".NETCoreApp,Version=v8.0");
}

TEST(TargetFrameworkAttributeTests, DefaultDisplayName_IsNull) {
    TargetFrameworkAttribute attr(".NETCoreApp,Version=v8.0");
    EXPECT_EQ(attr.getFrameworkDisplayNameProperty(), std::nullopt);
}

TEST(TargetFrameworkAttributeTests, SetDisplayName) {
    TargetFrameworkAttribute attr(".NETCoreApp,Version=v8.0");
    attr.setFrameworkDisplayNameProperty(".NET 8.0");
    EXPECT_EQ(attr.getFrameworkDisplayNameProperty(), std::optional<std::string>(".NET 8.0"));
}

TEST(SupportedOSPlatformAttributeTests, Constructor_StoresPlatform) {
    SupportedOSPlatformAttribute attr("windows");
    EXPECT_EQ(attr.getPlatformNameProperty(), "windows");
}

TEST(UnsupportedOSPlatformAttributeTests, Constructor_StoresPlatform) {
    UnsupportedOSPlatformAttribute attr("browser");
    EXPECT_EQ(attr.getPlatformNameProperty(), "browser");
    EXPECT_EQ(attr.getMessageProperty(), std::nullopt);
}

TEST(UnsupportedOSPlatformAttributeTests, Constructor_WithMessage) {
    UnsupportedOSPlatformAttribute attr("browser", "Not supported in the browser sandbox.");
    EXPECT_EQ(attr.getPlatformNameProperty(), "browser");
    EXPECT_EQ(attr.getMessageProperty(),
              std::optional<std::string>("Not supported in the browser sandbox."));
}

TEST(SupportedOSPlatformGuardAttributeTests, Constructor_StoresPlatform) {
    SupportedOSPlatformGuardAttribute attr("linux");
    EXPECT_EQ(attr.getPlatformNameProperty(), "linux");
}

TEST(UnsupportedOSPlatformGuardAttributeTests, Constructor_StoresPlatform) {
    UnsupportedOSPlatformGuardAttribute attr("windows");
    EXPECT_EQ(attr.getPlatformNameProperty(), "windows");
}

TEST(ObsoletedOSPlatformAttributeTests, Fix1980G4_UrlIsASettablePropertyNotAConstructorArgument) {
    // MIGRATED by #1980 group G-4 / SR-AUD-164, and the port had BOTH halves wrong, in OPPOSITE
    // directions: it took a third constructor parameter .NET does not have, and fed it into a
    // read-only accessor where .NET's is `public string? Url { get; set; }`. .NET declares
    // exactly `(platformName)` and `(platformName, message)` (PlatformAttributes.cs).
    ObsoletedOSPlatformAttribute attr("ios", "Use X instead");
    EXPECT_EQ(attr.getPlatformNameProperty(), "ios");
    EXPECT_EQ(attr.getMessageProperty(), std::optional<std::string>("Use X instead"));
    EXPECT_EQ(attr.getUrlProperty(), std::nullopt);

    attr.setUrlProperty("https://example.com");
    EXPECT_EQ(attr.getUrlProperty(), std::optional<std::string>("https://example.com"));

    static_assert(!std::is_constructible_v<ObsoletedOSPlatformAttribute,
                                            std::string, std::string, std::string>,
                  "#1980 G-4: .NET has no three-argument constructor");
}

TEST(ObsoletedOSPlatformAttributeTests, Fix1980G4_TheOneArgumentConstructorIsDotNets) {
    ObsoletedOSPlatformAttribute attr("android");
    EXPECT_EQ(attr.getPlatformNameProperty(), "android");
    EXPECT_EQ(attr.getMessageProperty(), std::nullopt);
    EXPECT_EQ(attr.getUrlProperty(), std::nullopt);
}

TEST(RequiresPreviewFeaturesAttributeTests, DefaultConstructor_NullMessage) {
    RequiresPreviewFeaturesAttribute attr;
    EXPECT_EQ(attr.getMessageProperty(), std::nullopt);
}

TEST(RequiresPreviewFeaturesAttributeTests, Constructor_WithMessage) {
    RequiresPreviewFeaturesAttribute attr("Preview feature");
    EXPECT_EQ(attr.getMessageProperty(), std::optional<std::string>("Preview feature"));
}

TEST(RequiresPreviewFeaturesAttributeTests, Fix1980G4_UrlIsASettablePropertyNotAConstructorArgument) {
    // The same inversion as ObsoletedOSPlatformAttribute, in the second of the two types G-4
    // names. .NET: `public RequiresPreviewFeaturesAttribute(string? message)` and
    // `public string? Url { get; set; }` (RequiresPreviewFeaturesAttribute.cs:34,47).
    RequiresPreviewFeaturesAttribute attr("Preview feature");
    EXPECT_EQ(attr.getUrlProperty(), std::nullopt);
    attr.setUrlProperty("https://aka.ms/preview");
    EXPECT_EQ(attr.getUrlProperty(), std::optional<std::string>("https://aka.ms/preview"));

    static_assert(!std::is_constructible_v<RequiresPreviewFeaturesAttribute,
                                            std::string, std::string>,
                  "#1980 G-4: .NET's constructor takes the message alone");
}

TEST(VersioningNullableMetadataTests, AbsentAndExplicitlyEmptyRemainDifferentForAllSixProperties) {
    // SR-AUD-164's remaining half: every field below is `string?` in .NET. Plain std::string
    // collapsed an omitted value and an explicitly supplied empty string into the same state.
    TargetFrameworkAttribute target(".NETCoreApp,Version=v8.0");
    EXPECT_EQ(target.getFrameworkDisplayNameProperty(), std::nullopt);
    target.setFrameworkDisplayNameProperty(std::string{});
    EXPECT_EQ(target.getFrameworkDisplayNameProperty(), std::optional<std::string>(""));
    target.setFrameworkDisplayNameProperty(std::nullopt);
    EXPECT_EQ(target.getFrameworkDisplayNameProperty(), std::nullopt);

    const UnsupportedOSPlatformAttribute unsupportedAbsent("browser");
    const UnsupportedOSPlatformAttribute unsupportedEmpty("browser", std::string{});
    EXPECT_EQ(unsupportedAbsent.getMessageProperty(), std::nullopt);
    EXPECT_EQ(unsupportedEmpty.getMessageProperty(), std::optional<std::string>(""));
    EXPECT_NE(unsupportedAbsent.getMessageProperty(), unsupportedEmpty.getMessageProperty());

    ObsoletedOSPlatformAttribute obsoletedAbsent("ios");
    const ObsoletedOSPlatformAttribute obsoletedEmpty("ios", std::string{});
    EXPECT_EQ(obsoletedAbsent.getMessageProperty(), std::nullopt);
    EXPECT_EQ(obsoletedEmpty.getMessageProperty(), std::optional<std::string>(""));
    EXPECT_EQ(obsoletedAbsent.getUrlProperty(), std::nullopt);
    obsoletedAbsent.setUrlProperty(std::string{});
    EXPECT_EQ(obsoletedAbsent.getUrlProperty(), std::optional<std::string>(""));
    obsoletedAbsent.setUrlProperty(std::nullopt);
    EXPECT_EQ(obsoletedAbsent.getUrlProperty(), std::nullopt);

    RequiresPreviewFeaturesAttribute previewAbsent;
    const RequiresPreviewFeaturesAttribute previewEmpty(std::string{});
    EXPECT_EQ(previewAbsent.getMessageProperty(), std::nullopt);
    EXPECT_EQ(previewEmpty.getMessageProperty(), std::optional<std::string>(""));
    EXPECT_EQ(previewAbsent.getUrlProperty(), std::nullopt);
    previewAbsent.setUrlProperty(std::string{});
    EXPECT_EQ(previewAbsent.getUrlProperty(), std::optional<std::string>(""));
    previewAbsent.setUrlProperty(std::nullopt);
    EXPECT_EQ(previewAbsent.getUrlProperty(), std::nullopt);
}

// ===========================================================================
// CompilerGeneratedAttribute
// ===========================================================================

TEST(CompilerGeneratedAttributeTests, IsAttribute) {
    CompilerGeneratedAttribute attr;
    const System::Attribute& base = attr;
    (void)base;
    SUCCEED();
}

// ===========================================================================
// GCSettings enum-domain validation
//
// Ticket #1976 / SR-AUD-156 (cause R-F of docs/SystemRuntimeNamespaceReviewPlan.md).
// Both setters were bare assignments, so an arbitrary enum cast became persistent,
// observable global configuration. Measured before the repair
// (build-probe/1972_probe1_before.log): latency(99) retained=99, latency(-1) retained=-1,
// loh(0) retained=0, loh(3) retained=3, every one of them no-throw.
//
// Each test restores the shared static, because these are process-global settings and the
// suite's other GCSettings tests read them.
// ===========================================================================

TEST(GCSettingsTests, SetLatencyMode_OutOfDomainValue_Throws) {
    GCSettings::setLatencyModeProperty(GCLatencyMode::Interactive);
    EXPECT_THROW(GCSettings::setLatencyModeProperty(static_cast<GCLatencyMode>(99)),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(GCSettings::setLatencyModeProperty(static_cast<GCLatencyMode>(-1)),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(GCSettings::setLatencyModeProperty(static_cast<GCLatencyMode>(5)),
                 System::ArgumentOutOfRangeException);
}

TEST(GCSettingsTests, SetLatencyMode_RejectedValue_LeavesThePreviousValueIntact) {
    // Rejecting is only half the contract: the invalid value must not be stored either.
    GCSettings::setLatencyModeProperty(GCLatencyMode::LowLatency);
    EXPECT_THROW(GCSettings::setLatencyModeProperty(static_cast<GCLatencyMode>(99)),
                 System::ArgumentOutOfRangeException);
    EXPECT_EQ(GCSettings::getLatencyModeProperty(), GCLatencyMode::LowLatency);
    GCSettings::setLatencyModeProperty(GCLatencyMode::Interactive);
}

TEST(GCSettingsTests, SetLatencyMode_NoGCRegion_IsRejectedAsRuntimeOwnedState) {
    // .NET does not let a caller declare a no-GC region through this setter; the value
    // reports runtime state. It stays a legal value to READ -- see the next test.
    GCSettings::setLatencyModeProperty(GCLatencyMode::Interactive);
    EXPECT_THROW(GCSettings::setLatencyModeProperty(GCLatencyMode::NoGCRegion),
                 System::ArgumentOutOfRangeException);
    EXPECT_EQ(GCSettings::getLatencyModeProperty(), GCLatencyMode::Interactive);
}

TEST(GCSettingsTests, GetLatencyMode_DomainIsWiderThanTheSetterDomain) {
    // Pins the deliberate asymmetry so a future "simplification" that validates the getter
    // against the setter's domain fails here rather than in a consumer.
    EXPECT_EQ(static_cast<int>(GCLatencyMode::NoGCRegion), 4);
    EXPECT_GT(static_cast<int>(GCLatencyMode::NoGCRegion),
              static_cast<int>(GCLatencyMode::SustainedLowLatency));
}

TEST(GCSettingsTests, SetLatencyMode_EveryInDomainValueStillSucceeds) {
    for (int v = static_cast<int>(GCLatencyMode::Batch);
         v <= static_cast<int>(GCLatencyMode::SustainedLowLatency); ++v) {
        GCSettings::setLatencyModeProperty(static_cast<GCLatencyMode>(v));
        EXPECT_EQ(static_cast<int>(GCSettings::getLatencyModeProperty()), v);
    }
    GCSettings::setLatencyModeProperty(GCLatencyMode::Interactive);
}

TEST(GCSettingsTests, SetCompactionMode_BothAdjacentInvalidValues_Throw) {
    // 0 and 3 bracket the enum: 0 is the value-initialised default and is NOT a member,
    // because this enum starts at Default = 1.
    GCSettings::setLargeObjectHeapCompactionModeProperty(GCLargeObjectHeapCompactionMode::Default);
    EXPECT_THROW(GCSettings::setLargeObjectHeapCompactionModeProperty(
                     static_cast<GCLargeObjectHeapCompactionMode>(0)),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(GCSettings::setLargeObjectHeapCompactionModeProperty(
                     static_cast<GCLargeObjectHeapCompactionMode>(3)),
                 System::ArgumentOutOfRangeException);
    EXPECT_EQ(GCSettings::getLargeObjectHeapCompactionModeProperty(),
              GCLargeObjectHeapCompactionMode::Default);
}

TEST(GCSettingsTests, SetCompactionMode_EveryInDomainValueStillSucceeds) {
    for (int v = static_cast<int>(GCLargeObjectHeapCompactionMode::Default);
         v <= static_cast<int>(GCLargeObjectHeapCompactionMode::CompactOnce); ++v) {
        GCSettings::setLargeObjectHeapCompactionModeProperty(
            static_cast<GCLargeObjectHeapCompactionMode>(v));
        EXPECT_EQ(static_cast<int>(GCSettings::getLargeObjectHeapCompactionModeProperty()), v);
    }
    GCSettings::setLargeObjectHeapCompactionModeProperty(GCLargeObjectHeapCompactionMode::Default);
}

TEST(GCSettingsTests, RejectedWrites_AreCatchableAsSystemException) {
    GCSettings::setLatencyModeProperty(GCLatencyMode::Interactive);
    bool caught = false;
    try {
        GCSettings::setLatencyModeProperty(static_cast<GCLatencyMode>(42));
    } catch (const System::Exception&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

// ===========================================================================
// FormattableStringFactory — SR-AUD-059's behaviour, pinned
//
// Ticket #1978 / cause R-J. The doc-comment used to promise std::invalid_argument for an
// empty format; the body has always forwarded it, and .NET rejects only null, which
// std::string cannot represent. The BEHAVIOUR is correct and the CLAIM was the defect, so
// this pins the behaviour rather than adding a rejection.
// ===========================================================================

TEST(FormattableStringFactoryTests, EmptyFormat_IsAcceptedAndYieldsAnEmptyResult) {
    auto fs = System::Runtime::CompilerServices::FormattableStringFactory::Create("");
    EXPECT_EQ(fs.getArgumentCountProperty(), 0);
    EXPECT_EQ(fs.ToString(), "");
}

TEST(FormattableStringFactoryTests, EmptyFormatWithArguments_IsAlsoAccepted) {
    auto fs = System::Runtime::CompilerServices::FormattableStringFactory::Create(
        "", {"unused"});
    EXPECT_EQ(fs.getArgumentCountProperty(), 1);
    EXPECT_EQ(fs.ToString(), "");
}

// ===========================================================================
// ConditionalWeakTable — the deliberate generic-domain widening, pinned
//
// Ticket #1982 / SR-AUD-162 (cause R-I of docs/SystemRuntimeNamespaceReviewPlan.md).
// .NET's `where T : class` exists because the CLR cannot make a weak GC handle to a value
// type; this port makes no GC handles and keys on std::weak_ptr<TKey>, which is well
// defined for scalar TKey. The disposition is to DOCUMENT the widening, not to narrow the
// domain, so these tests exist to make a later "let's match CS0452" change fail here.
// ===========================================================================

TEST(ConditionalWeakTableTests, ScalarTypeParameters_AreASupportedInstantiation) {
    ConditionalWeakTable<int, int> table;
    auto key = std::make_shared<int>(1);
    auto value = std::make_shared<int>(42);

    table.Add(key, value);
    std::shared_ptr<int> actual;
    ASSERT_TRUE(table.TryGetValue(key, actual));
    EXPECT_EQ(actual, value);
    EXPECT_EQ(*actual, 42);
}

TEST(ConditionalWeakTableTests, ScalarKeys_UseControlBlockIdentityNotValueEquality) {
    // The consequence a caller has to know, and the reason scalar TKey is not a special
    // case: two shared_ptr<int> holding equal integers are two different keys, exactly as
    // two distinct reference-type instances are in the managed API.
    ConditionalWeakTable<int, int> table;
    auto first = std::make_shared<int>(7);
    auto second = std::make_shared<int>(7);

    table.Add(first, std::make_shared<int>(1));
    std::shared_ptr<int> actual;
    EXPECT_TRUE(table.TryGetValue(first, actual));
    EXPECT_FALSE(table.TryGetValue(second, actual));
}

TEST(ConditionalWeakTableTests, ScalarKeys_StillExpireWhenTheLastOwnerDrops) {
    // weak_ptr<int> has the expiry semantics the table depends on, which is the substantive
    // claim behind refusing to adopt the managed constraint.
    ConditionalWeakTable<int, int> table;
    {
        auto key = std::make_shared<int>(5);
        table.Add(key, std::make_shared<int>(99));
        std::shared_ptr<int> actual;
        ASSERT_TRUE(table.TryGetValue(key, actual));
    }
    int enumerated = 0;
    auto e = table.GetEnumerator();
    while (e->MoveNext()) ++enumerated;
    EXPECT_EQ(enumerated, 0) << "an expired scalar key was still enumerated";
}

// ===========================================================================
// #1980 group G-1 -- SR-AUD-159. ExternalException's (message, errorCode)
// constructor, ErrorCode and ToString.
// ===========================================================================

TEST(ExternalExceptionTests, Fix1980G1_ErrorCodeIsTheHResultAndNeedsNoField) {
    // .NET's is `public virtual int ErrorCode => HResult;` -- an ALIAS, not separate state. The
    // finding's implication that it is a second field does not survive the reference, and this
    // row is what says so: setting one moves the other.
    ExternalException withCode("boom", static_cast<SharpRuntime::intcs>(0x80070005));
    EXPECT_EQ(withCode.getErrorCodeProperty(), static_cast<SharpRuntime::intcs>(0x80070005));
    EXPECT_EQ(withCode.getHResultProperty(), withCode.getErrorCodeProperty());

    // The other constructors keep E_FAIL, which is the whole point of the new overload existing.
    EXPECT_EQ(ExternalException().getErrorCodeProperty(),
              static_cast<SharpRuntime::intcs>(0x80004005));
    EXPECT_EQ(ExternalException("m").getErrorCodeProperty(),
              static_cast<SharpRuntime::intcs>(0x80004005));
}

// #1980 G-1 deliberately did NOT land ToString(), and this pins the absence so it cannot be
// added without the decision. It was implemented and then removed on the downstream measurement:
// cna derives from ExternalException in THREE types, and a statically resolved type name would
// have misnamed every one -- #2323's own rule, that a message naming the wrong type is a lie
// where an empty one is merely an absence. Ticket #2387.
//
// The parameter is DEPENDENT because gcc evaluates a non-dependent `requires` eagerly and
// hard-errors instead of yielding false (#2299, recorded in CLAUDE.md).
template <typename T> concept HasToString = requires(const T& e) { e.ToString(); };

TEST(ExternalExceptionTests, Decl1980G1_ToStringIsAbsentUntilItsTypeNameQuestionIsAnswered) {
    static_assert(!HasToString<ExternalException>,
                  "#2387: ToString() landed without resolving the derived-type name -- three cna "
                  "types derive from this class and a static name misnames all of them.");
    // The two members that DID land are unaffected and stay reachable.
    ExternalException ex("boom", static_cast<SharpRuntime::intcs>(0x80070005));
    EXPECT_EQ(ex.getErrorCodeProperty(), static_cast<SharpRuntime::intcs>(0x80070005));
    SUCCEED();
}

// =============================================================================================
// Ticket #1981 (cause R-H) — the ConditionalWeakTable enumerator retains only Current, and
// Reset() does nothing.
//
// The enumerator's snapshot used to be a vector of Entry, whose `value` is a STRONG shared_ptr,
// so an enumerator kept every snapshotted value alive for its own lifetime -- including values
// the table had already released. Measured before the repair
// (build-probe/1981_probe1_layout.cpp): after table.Remove(key) the value was still alive, and
// became collectable only when the enumerator was deleted.
//
// .NET's enumerator holds NO snapshot at all: it keeps the table plus an index range and reads
// the live container under the table's lock (ConditionalWeakTable.cs:441-478), so it retains
// only `_current`. That exact design is deliberately NOT reproduced here -- it requires the
// enumerator to hold a borrowed pointer to the table, and GetEnumerator() hands the caller a raw
// IEnumerator* whose lifetime the table does not control, which is the CCF-019 defect class this
// programme has spent the session removing. A snapshot of WEAK references reaches the same
// observable contract without introducing that hazard.
// =============================================================================================

namespace {
    using WeakTable = System::Runtime::CompilerServices::ConditionalWeakTable<int, int>;
    using WeakPair  = System::Collections::Generic::KeyValuePair<std::shared_ptr<int>,
                                                                 std::shared_ptr<int>>;
    using WeakEnum  = System::Collections::Generic::IEnumerator<WeakPair>;
}

TEST(ConditionalWeakTableEnumeratorTests, Fix1981_TheEnumeratorDoesNotRetainAReleasedValue) {
    auto key = std::make_shared<int>(1);
    std::weak_ptr<int> watch;
    WeakTable table;
    {
        auto value = std::make_shared<int>(42);
        watch = value;
        table.Add(key, value);
    }

    std::unique_ptr<WeakEnum> e(table.GetEnumerator());
    ASSERT_FALSE(watch.expired()) << "the table itself still holds the value here";

    ASSERT_TRUE(table.Remove(key));
    EXPECT_TRUE(watch.expired())
        << "the enumerator must not keep a value alive after the table released it";
}

TEST(ConditionalWeakTableEnumeratorTests, Fix1981_OnlyCurrentIsRetained) {
    auto keyA = std::make_shared<int>(1);
    auto keyB = std::make_shared<int>(2);
    std::weak_ptr<int> watchA, watchB;
    WeakTable table;
    {
        auto a = std::make_shared<int>(10);
        auto b = std::make_shared<int>(20);
        watchA = a; watchB = b;
        table.Add(keyA, a);
        table.Add(keyB, b);
    }

    std::unique_ptr<WeakEnum> e(table.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    const int firstKey = *e->Current().Key;

    // Release BOTH from the table. Whichever one MoveNext has landed on is retained by Current;
    // the other must be gone. That asymmetry is the whole point -- a snapshot holding strong
    // values would keep both alive.
    ASSERT_TRUE(table.Remove(keyA));
    ASSERT_TRUE(table.Remove(keyB));

    if (firstKey == 1) {
        EXPECT_FALSE(watchA.expired()) << "Current must stay alive";
        EXPECT_TRUE(watchB.expired())  << "a value that is not Current must not be retained";
    } else {
        EXPECT_FALSE(watchB.expired()) << "Current must stay alive";
        EXPECT_TRUE(watchA.expired())  << "a value that is not Current must not be retained";
    }
}

TEST(ConditionalWeakTableEnumeratorTests, Fix1981_ResetDoesNothingAndCannotReEnumerate) {
    // .NET: `public void Reset() { }` (ConditionalWeakTable.cs:492). This port used to rewind
    // the index, so Reset() re-enumerated. That capability is removed deliberately.
    auto key = std::make_shared<int>(1);
    auto value = std::make_shared<int>(42);
    WeakTable table;
    table.Add(key, value);

    std::unique_ptr<WeakEnum> e(table.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(*e->Current().Value, 42);
    ASSERT_FALSE(e->MoveNext()) << "one entry, so the second MoveNext ends the enumeration";

    e->Reset();
    EXPECT_FALSE(e->MoveNext()) << "Reset() is a no-op, so it cannot restart the enumeration";
}

TEST(ConditionalWeakTableEnumeratorTests, Fix1981_ResetLeavesCurrentUntouched) {
    // The half that is easy to get wrong in the other direction: because .NET's Reset() body is
    // empty, it does NOT clear Current either. A "no-op" that still cleared hasCurrent_ would
    // make Current() throw here.
    auto key = std::make_shared<int>(1);
    auto value = std::make_shared<int>(42);
    WeakTable table;
    table.Add(key, value);

    std::unique_ptr<WeakEnum> e(table.GetEnumerator());
    ASSERT_TRUE(e->MoveNext());
    e->Reset();
    EXPECT_NO_THROW((void)e->Current());
    EXPECT_EQ(*e->Current().Value, 42);
}

TEST(ConditionalWeakTableEnumeratorTests, Fix1981_AnEntryReleasedAfterTheSnapshotIsSkipped) {
    // Added after mutation M3 -- "test only the key, not the value" -- went UNCAUGHT: no case
    // above enumerates AFTER a Remove, which is the only situation where the key is alive and the
    // value is not. The caller still holds the key, so key.lock() succeeds; the table has dropped
    // the value, so value.lock() fails. Testing the key alone would yield a pair with a NULL
    // value and count it as a live entry.
    //
    // .NET skips it: its MoveNext loops while TryGetEntry returns false for an index whose entry
    // has been removed or collected (ConditionalWeakTable.cs:459-467).
    auto keyA = std::make_shared<int>(1);
    auto keyB = std::make_shared<int>(2);
    WeakTable table;
    {
        table.Add(keyA, std::make_shared<int>(10));
        table.Add(keyB, std::make_shared<int>(20));
    }

    std::unique_ptr<WeakEnum> e(table.GetEnumerator());
    ASSERT_TRUE(table.Remove(keyA));   // after the snapshot was taken; keyA is still alive here

    int seen = 0;
    while (e->MoveNext()) {
        ++seen;
        ASSERT_NE(e->Current().Value, nullptr) << "a skipped entry must never be yielded";
        EXPECT_EQ(*e->Current().Key, 2);
        EXPECT_EQ(*e->Current().Value, 20);
    }
    EXPECT_EQ(seen, 1) << "the released entry must be skipped, not yielded with a null value";
}

TEST(ConditionalWeakTableEnumeratorTests, Fix1981_OrdinaryEnumerationIsUnchanged) {
    auto keyA = std::make_shared<int>(1);
    auto keyB = std::make_shared<int>(2);
    auto a = std::make_shared<int>(10);
    auto b = std::make_shared<int>(20);
    WeakTable table;
    table.Add(keyA, a);
    table.Add(keyB, b);

    std::unique_ptr<WeakEnum> e(table.GetEnumerator());
    int seen = 0, sum = 0;
    while (e->MoveNext()) { ++seen; sum += *e->Current().Value; }
    EXPECT_EQ(seen, 2);
    EXPECT_EQ(sum, 30);
}

TEST(ConditionalWeakTableEnumeratorTests, Decl1981_TheTablesOwnLayoutIsUnchanged) {
    // SA-3's pinned measurement. The Enumerator is a PRIVATE nested class that GetEnumerator()
    // heap-allocates and hands back as an IEnumerator<Pair>*, so no consumer can name it, size it
    // or hold one by value: its layout change is invisible through every public spelling. What a
    // consumer CAN size is the table, and that did not move -- 72 before the repair and 72 after
    // (build-probe/1981_probe1_layout.cpp).
    static_assert(sizeof(WeakTable) == 72, "#1981 must not change the table's own layout");
    static_assert(alignof(WeakTable) == 8);
    EXPECT_EQ(sizeof(WeakTable), 72u);
}

// =============================================================================================
// Ticket #1980 group G-2 (SR-AUD-165/166) — the interop metadata VALUES.
//
// These types exist to preserve managed metadata, not to produce an effect (P/Invoke and interop
// are a declared permanent deviation), so getting the numbers right is the whole of their
// contract. Landed under SA-5: every value below is transcribed from the reference.
// =============================================================================================

TEST(InteropMetadataValueTests, Fix1980G2_LPStructNoLongerCollidesWithLPUTF8Str) {
    // THE FINDING WAS UNDERSTATED. LPStruct was 48 -- which is LPUTF8Str's value, so the two
    // enumerators were INDISTINGUISHABLE: the comparison below was true, and a switch over
    // UnmanagedType could not carry both arms. .NET: LPStruct = 0x2b (UnmanagedType.cs:55).
    EXPECT_EQ(static_cast<int>(UnmanagedType::LPStruct), 43);
    EXPECT_EQ(static_cast<int>(UnmanagedType::LPUTF8Str), 48);
    EXPECT_NE(UnmanagedType::LPStruct, UnmanagedType::LPUTF8Str);
}

TEST(InteropMetadataValueTests, Fix1980G2_CurrencyAndIDispatchExist) {
    // Both were absent. .NET: Currency = 0xf, IDispatch = 0x1a (UnmanagedType.cs:22,30).
    EXPECT_EQ(static_cast<int>(UnmanagedType::Currency), 15);
    EXPECT_EQ(static_cast<int>(UnmanagedType::IDispatch), 26);
}

TEST(InteropMetadataValueTests, Decl1980G2_EveryOtherUnmanagedTypeValueWasAlreadyRight) {
    // Asserted so the two repairs above are not mistaken for a wholesale renumbering: the rest of
    // the enum already matched .NET exactly, which is why only these three rows moved.
    EXPECT_EQ(static_cast<int>(UnmanagedType::Bool), 2);
    EXPECT_EQ(static_cast<int>(UnmanagedType::U8), 10);
    EXPECT_EQ(static_cast<int>(UnmanagedType::BStr), 19);
    EXPECT_EQ(static_cast<int>(UnmanagedType::IUnknown), 25);
    EXPECT_EQ(static_cast<int>(UnmanagedType::Struct), 27);
    EXPECT_EQ(static_cast<int>(UnmanagedType::LPArray), 42);
    EXPECT_EQ(static_cast<int>(UnmanagedType::CustomMarshaler), 44);
    EXPECT_EQ(static_cast<int>(UnmanagedType::HString), 47);
}

TEST(InteropMetadataValueTests, Fix1980G2_DllImportBoolDefaultsAreAllFalse) {
    // PreserveSig and BestFitMapping defaulted to TRUE. .NET declares every one of these as a
    // plain `public bool` with no initializer (DllImportAttribute.cs:18-23), so all five read
    // false -- and the port had already got the other three right, which is what made the two
    // outliers a divergence rather than a policy.
    DllImportAttribute attr("lib");
    EXPECT_FALSE(attr.PreserveSig);
    EXPECT_FALSE(attr.BestFitMapping);
    EXPECT_FALSE(attr.SetLastError);
    EXPECT_FALSE(attr.ExactSpelling);
    EXPECT_FALSE(attr.ThrowOnUnmappableChar);
}

TEST(InteropMetadataValueTests, Decl1980G2_BothCharSetDefaultsAreUnsetNotNamed) {
    // A divergence the plan's G-2 list did NOT name, found by measuring the reference alongside
    // the four it did. .NET declares `public CharSet CharSet;` on both attributes with no
    // initializer, so the default is 0 -- and CharSet has NO enumerator with that value (None is
    // 1). Reproducing it is deliberate: the header's stated purpose is to preserve the managed
    // metadata values, and .NET's metadata really does carry an unset CharSet here.
    EXPECT_EQ(static_cast<int>(CharSet::None), 1) << "None is 1, so 0 is genuinely unnamed";
    EXPECT_EQ(static_cast<int>(StructLayoutAttribute(LayoutKind::Sequential).CharSet), 0);
    EXPECT_EQ(static_cast<int>(DllImportAttribute("lib").CharSet), 0);
}

// =============================================================================================
// Ticket #1980 group G-5 (SR-AUD-167) — MarshalAsAttribute's field TYPES, and the two absent
// COM enums.
//
// Like G-2, this is about metadata fidelity: these types exist to preserve the managed values,
// so a field typed as a loose integer where .NET types it as an enum is not a stylistic choice --
// it lets any number be stored where only a marshalling kind is meaningful, which is the whole
// reason the enum exists.
// =============================================================================================

namespace {
    template <typename T>
    concept HasPublicValueField = requires(T a) { a.Value; };
}

TEST(MarshalAsFieldTypeTests, Fix1980G5_ArraySubTypeIsAnUnmanagedTypeNotAnInt) {
    // .NET: `public UnmanagedType ArraySubType;` (MarshalAsAttribute.cs:29).
    static_assert(std::is_same_v<decltype(MarshalAsAttribute(UnmanagedType::LPArray).ArraySubType),
                                  UnmanagedType>,
                  "#1980 G-5: ArraySubType is an UnmanagedType, not an integer");
    MarshalAsAttribute attr(UnmanagedType::LPArray);
    attr.ArraySubType = UnmanagedType::I4;
    EXPECT_EQ(attr.ArraySubType, UnmanagedType::I4);
}

TEST(MarshalAsFieldTypeTests, Fix1980G5_SizeParamIndexIsAShortNotAnInt) {
    // .NET: `public short SizeParamIndex;` (MarshalAsAttribute.cs:30) -- a short, because the
    // value is a parameter position and the metadata encoding is 16-bit.
    static_assert(std::is_same_v<decltype(MarshalAsAttribute(UnmanagedType::LPArray).SizeParamIndex),
                                  SharpRuntime::shortcs>,
                  "#1980 G-5: SizeParamIndex is a short");
}

TEST(MarshalAsFieldTypeTests, Fix1980G5_ValueIsGetOnly) {
    // SA-8's first bullet: a public mutable data member where .NET's is get-only.
    static_assert(!HasPublicValueField<MarshalAsAttribute>,
                  "#1980 G-5: Value is get-only -- read it with getValueProperty()");
}

TEST(MarshalAsFieldTypeTests, Fix1980G5_TheTwoAbsentFieldsExist) {
    // SafeArraySubType and IidParameterIndex were absent entirely.
    MarshalAsAttribute attr(UnmanagedType::SafeArray);
    EXPECT_EQ(static_cast<int>(attr.SafeArraySubType), 0) << "unset, as .NET's uninitialised field";
    EXPECT_EQ(attr.IidParameterIndex, 0);
    attr.SafeArraySubType = VarEnum::VT_BSTR;
    attr.IidParameterIndex = 3;
    EXPECT_EQ(attr.SafeArraySubType, VarEnum::VT_BSTR);
    EXPECT_EQ(attr.IidParameterIndex, 3);
}

TEST(MarshalAsFieldTypeTests, Decl1980G5_ArraySubTypeDefaultsToAnUnnamedValue) {
    // Same reasoning G-2 recorded for the two CharSet defaults: .NET's field has no initializer,
    // so the default is 0 -- and UnmanagedType has no enumerator with that value (Bool is 2).
    EXPECT_EQ(static_cast<int>(MarshalAsAttribute(UnmanagedType::LPArray).ArraySubType), 0);
    EXPECT_EQ(static_cast<int>(UnmanagedType::Bool), 2) << "so 0 is genuinely unnamed";
}

TEST(MarshalAsFieldTypeTests, Fix1980G5_TheTwoComEnumsExist) {
    EXPECT_EQ(static_cast<int>(ComInterfaceType::InterfaceIsDual), 0);
    EXPECT_EQ(static_cast<int>(ComInterfaceType::InterfaceIsIUnknown), 1);
    EXPECT_EQ(static_cast<int>(ComInterfaceType::InterfaceIsIDispatch), 2);
    EXPECT_EQ(static_cast<int>(ComInterfaceType::InterfaceIsIInspectable), 3);
    EXPECT_EQ(static_cast<int>(ClassInterfaceType::None), 0);
    EXPECT_EQ(static_cast<int>(ClassInterfaceType::AutoDispatch), 1);
    EXPECT_EQ(static_cast<int>(ClassInterfaceType::AutoDual), 2);
}

TEST(ComInterfaceAttributeShapeTests, InterfaceTypeValueIsTypedGetOnlyAndTheClassIsFinal) {
    static_assert(std::is_final_v<InterfaceTypeAttribute>);
    static_assert(!HasPublicValueField<InterfaceTypeAttribute>,
                  "SR-AUD-167: InterfaceTypeAttribute.Value must be get-only");
    static_assert(std::is_constructible_v<InterfaceTypeAttribute, ComInterfaceType>);
    static_assert(std::is_constructible_v<InterfaceTypeAttribute, SharpRuntime::shortcs>);
    static_assert(std::is_same_v<
                  decltype(InterfaceTypeAttribute(ComInterfaceType::InterfaceIsDual)
                               .getValueProperty()),
                  ComInterfaceType>);
    static_assert(sizeof(ComInterfaceType) == sizeof(SharpRuntime::intcs));
    constexpr std::size_t interfaceDeclared =
        sizeof(System::Attribute) + sizeof(ComInterfaceType);
    constexpr std::size_t interfaceAlignment = alignof(InterfaceTypeAttribute);
    constexpr std::size_t interfaceRounded =
        ((interfaceDeclared + interfaceAlignment - 1) / interfaceAlignment) * interfaceAlignment;
    EXPECT_EQ(sizeof(InterfaceTypeAttribute), interfaceRounded)
        << "the typed private field replaces, rather than supplements, the old public int";

    const InterfaceTypeAttribute typed(ComInterfaceType::InterfaceIsIDispatch);
    EXPECT_EQ(typed.getValueProperty(), ComInterfaceType::InterfaceIsIDispatch);

    // .NET retains a raw Int16 compatibility constructor. It stores the value verbatim even when
    // that value is not a named enumerator; the strongly typed constructor is the normal route.
    const InterfaceTypeAttribute raw(static_cast<SharpRuntime::shortcs>(42));
    EXPECT_EQ(static_cast<int>(raw.getValueProperty()), 42);
}

TEST(ComInterfaceAttributeShapeTests, ClassInterfaceValueIsTypedGetOnlyAndTheClassIsFinal) {
    static_assert(std::is_final_v<ClassInterfaceAttribute>);
    static_assert(!HasPublicValueField<ClassInterfaceAttribute>,
                  "SR-AUD-167: ClassInterfaceAttribute.Value must be get-only");
    static_assert(std::is_constructible_v<ClassInterfaceAttribute, ClassInterfaceType>);
    static_assert(std::is_constructible_v<ClassInterfaceAttribute, SharpRuntime::shortcs>);
    static_assert(std::is_same_v<
                  decltype(ClassInterfaceAttribute(ClassInterfaceType::None).getValueProperty()),
                  ClassInterfaceType>);
    static_assert(sizeof(ClassInterfaceType) == sizeof(SharpRuntime::intcs));
    constexpr std::size_t classDeclared =
        sizeof(System::Attribute) + sizeof(ClassInterfaceType);
    constexpr std::size_t classAlignment = alignof(ClassInterfaceAttribute);
    constexpr std::size_t classRounded =
        ((classDeclared + classAlignment - 1) / classAlignment) * classAlignment;
    EXPECT_EQ(sizeof(ClassInterfaceAttribute), classRounded)
        << "the typed private field replaces, rather than supplements, the old public int";

    const ClassInterfaceAttribute typed(ClassInterfaceType::AutoDual);
    EXPECT_EQ(typed.getValueProperty(), ClassInterfaceType::AutoDual);

    const ClassInterfaceAttribute raw(static_cast<SharpRuntime::shortcs>(42));
    EXPECT_EQ(static_cast<int>(raw.getValueProperty()), 42);
}

namespace {
    /// Exhaustive over VarEnum, with NO `default:` label.
    ///
    /// This is the only way C++ offers to pin an enum's MEMBERSHIP rather than its values: the
    /// build runs with -Wall -Wextra -Werror, so gcc's -Wswitch turns any enumerator that is not
    /// handled here into a compile ERROR. Asserting that VT_DECIMAL is 14 and VT_I1 is 16 catches
    /// a RENUMBERING but not an INSERTION -- a mutation adding `VT_UNUSED15 = 15` went uncaught
    /// until this function existed, because C++ has no way to enumerate an enum's members.
    ///
    /// Every arm is .NET's; adding one here without adding it to VarEnum.cs would be inventing
    /// surface, and removing one from VarEnum breaks this function's own reference to it.
    int VarEnumCensus(VarEnum v) {
        switch (v) {
            case VarEnum::VT_EMPTY: case VarEnum::VT_NULL: case VarEnum::VT_I2:
            case VarEnum::VT_I4: case VarEnum::VT_R4: case VarEnum::VT_R8:
            case VarEnum::VT_CY: case VarEnum::VT_DATE: case VarEnum::VT_BSTR:
            case VarEnum::VT_DISPATCH: case VarEnum::VT_ERROR: case VarEnum::VT_BOOL:
            case VarEnum::VT_VARIANT: case VarEnum::VT_UNKNOWN: case VarEnum::VT_DECIMAL:
            case VarEnum::VT_I1: case VarEnum::VT_UI1: case VarEnum::VT_UI2:
            case VarEnum::VT_UI4: case VarEnum::VT_I8: case VarEnum::VT_UI8:
            case VarEnum::VT_INT: case VarEnum::VT_UINT: case VarEnum::VT_VOID:
            case VarEnum::VT_HRESULT: case VarEnum::VT_PTR: case VarEnum::VT_SAFEARRAY:
            case VarEnum::VT_CARRAY: case VarEnum::VT_USERDEFINED: case VarEnum::VT_LPSTR:
            case VarEnum::VT_LPWSTR: case VarEnum::VT_RECORD: case VarEnum::VT_FILETIME:
            case VarEnum::VT_BLOB: case VarEnum::VT_STREAM: case VarEnum::VT_STORAGE:
            case VarEnum::VT_STREAMED_OBJECT: case VarEnum::VT_STORED_OBJECT:
            case VarEnum::VT_BLOB_OBJECT: case VarEnum::VT_CF: case VarEnum::VT_CLSID:
            case VarEnum::VT_VECTOR: case VarEnum::VT_ARRAY: case VarEnum::VT_BYREF:
                return static_cast<int>(v);
        }
        return -1;
    }
}

TEST(MarshalAsFieldTypeTests, Decl1980G5_VarEnumMembershipIsPinnedByAnExhaustiveSwitch) {
    // The census above is the pin; this keeps it visible in the test listing and exercises it.
    EXPECT_EQ(VarEnumCensus(VarEnum::VT_EMPTY), 0);
    EXPECT_EQ(VarEnumCensus(VarEnum::VT_BYREF), 0x4000);
}

TEST(MarshalAsFieldTypeTests, Fix1980G5_VarEnumCarriesItsHoleAndItsFlags) {
    // The two rows a transcription is most likely to get wrong: 15 is a HOLE (.NET's enum jumps
    // from VT_DECIMAL = 14 to VT_I1 = 16), and the three flag values sit far above the rest.
    EXPECT_EQ(static_cast<int>(VarEnum::VT_DECIMAL), 14);
    EXPECT_EQ(static_cast<int>(VarEnum::VT_I1), 16);
    EXPECT_EQ(static_cast<int>(VarEnum::VT_CLSID), 72);
    EXPECT_EQ(static_cast<int>(VarEnum::VT_VECTOR), 0x1000);
    EXPECT_EQ(static_cast<int>(VarEnum::VT_ARRAY), 0x2000);
    EXPECT_EQ(static_cast<int>(VarEnum::VT_BYREF), 0x4000);
}

TEST(MarshalAsFieldTypeTests, Decl1980G5_TheTwoTypeValuedMembersStayOutOfScope) {
    // .NET types MarshalTypeRef and SafeArrayUserDefinedSubType as `Type?`, and System::Type is
    // reflection -- a declared permanent deviation. MarshalTypeRef survives as a string holding
    // the name; SafeArrayUserDefinedSubType is ABSENT rather than invented as a second string,
    // because a member that cannot carry what .NET's carries is worse than no member.
    static_assert(std::is_same_v<decltype(MarshalAsAttribute(UnmanagedType::LPStr).MarshalTypeRef),
                                  std::string>);
    static_assert(std::is_final_v<MarshalAsAttribute>, ".NET's MarshalAsAttribute is sealed");
}

// ===========================================================================================
// #1980 G-3 (SA-15.3) -- the hierarchy change SA-3 used to exclude
//
// SA-15.3 lifted SA-3's exclusion of vtable and base-class changes under five conditions. The
// fourth exists for THIS ticket: a reparenting silently changes which handlers fire, and no
// layout assertion would reveal it.
// ===========================================================================================

TEST(RuntimeG3Tests, AmbiguousImplementationExceptionHasDotNetsShape) {
    using System::Runtime::AmbiguousImplementationException;
    // AmbiguousImplementationException.cs -- `public sealed class ... : Exception`.
    static_assert(std::is_final_v<AmbiguousImplementationException>);
    static_assert(std::is_base_of_v<System::Exception, AmbiguousImplementationException>);
    static_assert(!std::is_base_of_v<System::SystemException, AmbiguousImplementationException>,
                  "#1980 G-3: .NET derives this from Exception, not SystemException");

    // The constructor SR-AUD-158 recorded as missing.
    const AmbiguousImplementationException withInner("outer", std::exception_ptr{});
    EXPECT_EQ(std::string(withInner.getMessageProperty()), "outer");
    // ...and every constructor still carries COR_E_AMBIGUOUSIMPLEMENTATION, which the reparenting
    // must not have dropped along with the old base.
    EXPECT_EQ(AmbiguousImplementationException().getHResultProperty(),
              static_cast<SharpRuntime::intcs>(0x8013106Au));
    EXPECT_EQ(AmbiguousImplementationException("m").getHResultProperty(),
              static_cast<SharpRuntime::intcs>(0x8013106Au));
    EXPECT_EQ(withInner.getHResultProperty(), static_cast<SharpRuntime::intcs>(0x8013106Au));
}

// SA-15.3's FIRST CONDITION: the layout AND the vtable change, measured and pinned. Every figure
// here was measured after the change rather than predicted before it -- #1958's lesson, where a
// predicted 104 was asserted and the build rejected it.
//
// G-3 ITSELF GREW NOTHING. AmbiguousImplementationException stays 168 because SystemException
// adds no members of its own over Exception, so reparenting moved the type sideways rather than
// shrinking it. The five attributes stayed where they were because platformName_ moved INTO the
// new base rather than being duplicated beside it. SR-AUD-164's later nullable-state follow-up
// legitimately grows the four types named below; those new relationships are pinned separately.
// A consumer must rebuild for both changes: G-3 moved the vtable, and SR-AUD-164 moved layouts.
TEST(RuntimeG3Tests, G3LayoutsAndLaterNullableGrowthArePinned) {
    using namespace System::Runtime::Versioning;
    EXPECT_EQ(sizeof(System::Exception), 168u);
    EXPECT_EQ(sizeof(System::SystemException), 168u)
        << "SystemException adding no members is WHY the reparenting costs no bytes";
    EXPECT_EQ(sizeof(System::Runtime::AmbiguousImplementationException), 168u);

    EXPECT_EQ(sizeof(System::Attribute), 8u);
    EXPECT_EQ(sizeof(OSPlatformAttribute), 40u);
    // The base is Attribute plus one std::string, and the derived ones add only their own extras.
    EXPECT_EQ(sizeof(SupportedOSPlatformAttribute), sizeof(OSPlatformAttribute));
    EXPECT_EQ(sizeof(SupportedOSPlatformGuardAttribute), sizeof(OSPlatformAttribute));
    EXPECT_EQ(sizeof(UnsupportedOSPlatformGuardAttribute), sizeof(OSPlatformAttribute));
    // SR-AUD-164 makes each nullable string a real optional state. These relationships pin every
    // affected data member and ensure a later edit cannot silently collapse null back into empty.
    EXPECT_GT(sizeof(std::optional<std::string>), sizeof(std::string));
    EXPECT_EQ(sizeof(TargetFrameworkAttribute),
              sizeof(System::Attribute) + sizeof(std::string) +
                  sizeof(std::optional<std::string>));
    EXPECT_EQ(sizeof(UnsupportedOSPlatformAttribute),
              sizeof(OSPlatformAttribute) + sizeof(std::optional<std::string>));
    EXPECT_EQ(sizeof(ObsoletedOSPlatformAttribute),
              sizeof(OSPlatformAttribute) + 2 * sizeof(std::optional<std::string>));
    EXPECT_EQ(sizeof(RequiresPreviewFeaturesAttribute),
              sizeof(System::Attribute) + 2 * sizeof(std::optional<std::string>));

    // The vtable half: every one of these is polymorphic, which is what forces the rebuild.
    static_assert(std::is_polymorphic_v<System::Runtime::AmbiguousImplementationException>);
    static_assert(std::is_polymorphic_v<OSPlatformAttribute>);
}

// SA-15.3's FOURTH CONDITION, as an assertion rather than a claim: the clause whose meaning moved
// is `catch (const SystemException&)`, and the ones that did not move are asserted beside it so a
// future reparenting cannot quietly take them too.
TEST(RuntimeG3Tests, WhichCatchClausesChangedMeaning) {
    using System::Runtime::AmbiguousImplementationException;

    bool caughtAsSystemException = false;
    try { throw AmbiguousImplementationException(); }
    catch (const System::SystemException&) { caughtAsSystemException = true; }
    catch (const System::Exception&) {}
    EXPECT_FALSE(caughtAsSystemException)
        << "this is the clause that changed: it used to catch this type and must no longer";

    bool caughtAsException = false;
    try { throw AmbiguousImplementationException(); }
    catch (const System::Exception&) { caughtAsException = true; }
    EXPECT_TRUE(caughtAsException) << "catch (const Exception&) must be unaffected";

    bool caughtExactly = false;
    try { throw AmbiguousImplementationException(); }
    catch (const AmbiguousImplementationException&) { caughtExactly = true; }
    EXPECT_TRUE(caughtExactly) << "catching the type itself must be unaffected";
}

// SR-AUD-163: before G-3 the five platform attributes each derived from System::Attribute and
// EACH CARRIED ITS OWN COPY of platformName_ and getPlatformNameProperty() -- five duplicates of
// one fact, and no type through which a caller could handle "any platform attribute".
TEST(RuntimeG3Tests, TheFivePlatformAttributesShareOneBase) {
    using namespace System::Runtime::Versioning;
    static_assert(std::is_base_of_v<OSPlatformAttribute, SupportedOSPlatformAttribute>);
    static_assert(std::is_base_of_v<OSPlatformAttribute, UnsupportedOSPlatformAttribute>);
    static_assert(std::is_base_of_v<OSPlatformAttribute, SupportedOSPlatformGuardAttribute>);
    static_assert(std::is_base_of_v<OSPlatformAttribute, UnsupportedOSPlatformGuardAttribute>);
    static_assert(std::is_base_of_v<OSPlatformAttribute, ObsoletedOSPlatformAttribute>);
    // .NET seals every derived one (PlatformAttributes.cs:35,69,100,135,185,211).
    static_assert(std::is_final_v<SupportedOSPlatformAttribute>);
    static_assert(std::is_final_v<UnsupportedOSPlatformAttribute>);
    static_assert(std::is_final_v<ObsoletedOSPlatformAttribute>);
    // ...and the base is still an Attribute, which the reparenting must not have cost.
    static_assert(std::is_base_of_v<System::Attribute, OSPlatformAttribute>);

    // THE BASE IS WHAT THE CHANGE BUYS: one handle for any platform attribute. A `static_assert`
    // alone would pass against a base nothing can be used through.
    const SupportedOSPlatformAttribute supported("linux");
    const UnsupportedOSPlatformAttribute unsupported("browser");
    const OSPlatformAttribute& asBase = supported;
    EXPECT_EQ(asBase.getPlatformNameProperty(), "linux");
    const OSPlatformAttribute* names[] = {&supported, &unsupported};
    EXPECT_EQ(names[0]->getPlatformNameProperty(), "linux");
    EXPECT_EQ(names[1]->getPlatformNameProperty(), "browser");
}

// PlatformAttributes.cs:12 -- the constructor is `private protected`, so the base cannot be
// created on its own. C++ has no `private protected`; `protected` carries the half that matters
// and the header says which half is not expressible.
TEST(RuntimeG3Tests, TheBaseCannotBeConstructedOnItsOwn) {
    using System::Runtime::Versioning::OSPlatformAttribute;
    static_assert(!std::is_constructible_v<OSPlatformAttribute, std::string>,
                  "#1980 G-3: .NET's constructor is private protected");
    static_assert(!std::is_default_constructible_v<OSPlatformAttribute>);
    SUCCEED();
}
