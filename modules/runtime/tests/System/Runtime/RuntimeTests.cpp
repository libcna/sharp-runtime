// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <memory>
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

TEST(CompilerFeatureRequiredAttributeTests, StoresFeatureNameAndOptionalFlag) {
    CompilerFeatureRequiredAttribute attr("custom-feature");
    EXPECT_EQ(attr.getFeatureNameProperty(), "custom-feature");
    EXPECT_FALSE(attr.getIsOptionalProperty());
    attr.setIsOptionalProperty(true);
    EXPECT_TRUE(attr.getIsOptionalProperty());
    EXPECT_EQ(CompilerFeatureRequiredAttribute::RefStructs, "RefStructs");
    EXPECT_EQ(CompilerFeatureRequiredAttribute::RequiredMembers, "RequiredMembers");
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
    MarshalAsAttribute attr(UnmanagedType::LPStr);
    EXPECT_EQ(attr.Value, UnmanagedType::LPStr);
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

TEST(TargetFrameworkAttributeTests, DefaultDisplayName_IsEmpty) {
    TargetFrameworkAttribute attr(".NETCoreApp,Version=v8.0");
    EXPECT_EQ(attr.getFrameworkDisplayNameProperty(), "");
}

TEST(TargetFrameworkAttributeTests, SetDisplayName) {
    TargetFrameworkAttribute attr(".NETCoreApp,Version=v8.0");
    attr.setFrameworkDisplayNameProperty(".NET 8.0");
    EXPECT_EQ(attr.getFrameworkDisplayNameProperty(), ".NET 8.0");
}

TEST(SupportedOSPlatformAttributeTests, Constructor_StoresPlatform) {
    SupportedOSPlatformAttribute attr("windows");
    EXPECT_EQ(attr.getPlatformNameProperty(), "windows");
}

TEST(UnsupportedOSPlatformAttributeTests, Constructor_StoresPlatform) {
    UnsupportedOSPlatformAttribute attr("browser");
    EXPECT_EQ(attr.getPlatformNameProperty(), "browser");
    EXPECT_EQ(attr.getMessageProperty(), "");
}

TEST(UnsupportedOSPlatformAttributeTests, Constructor_WithMessage) {
    UnsupportedOSPlatformAttribute attr("browser", "Not supported in the browser sandbox.");
    EXPECT_EQ(attr.getPlatformNameProperty(), "browser");
    EXPECT_EQ(attr.getMessageProperty(), "Not supported in the browser sandbox.");
}

TEST(SupportedOSPlatformGuardAttributeTests, Constructor_StoresPlatform) {
    SupportedOSPlatformGuardAttribute attr("linux");
    EXPECT_EQ(attr.getPlatformNameProperty(), "linux");
}

TEST(UnsupportedOSPlatformGuardAttributeTests, Constructor_StoresPlatform) {
    UnsupportedOSPlatformGuardAttribute attr("windows");
    EXPECT_EQ(attr.getPlatformNameProperty(), "windows");
}

TEST(ObsoletedOSPlatformAttributeTests, Constructor_AllFields) {
    ObsoletedOSPlatformAttribute attr("ios", "Use X instead", "https://example.com");
    EXPECT_EQ(attr.getPlatformNameProperty(), "ios");
    EXPECT_EQ(attr.getMessageProperty(), "Use X instead");
    EXPECT_EQ(attr.getUrlProperty(), "https://example.com");
}

TEST(RequiresPreviewFeaturesAttributeTests, DefaultConstructor_EmptyMessage) {
    RequiresPreviewFeaturesAttribute attr;
    EXPECT_EQ(attr.getMessageProperty(), "");
}

TEST(RequiresPreviewFeaturesAttributeTests, Constructor_WithMessage) {
    RequiresPreviewFeaturesAttribute attr("Preview feature");
    EXPECT_EQ(attr.getMessageProperty(), "Preview feature");
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
