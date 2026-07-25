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
    EXPECT_NE(RuntimeHelpers::GetHashCode(object), 0);
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

TEST(StructLayoutAttributeTests, DefaultPack_IsEight) {
    StructLayoutAttribute attr(LayoutKind::Explicit);
    EXPECT_EQ(attr.Pack, 8);
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
