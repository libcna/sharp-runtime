// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Collections/detail/ElementReference.hpp"
#include "System/InvalidOperationException.hpp"
#include <gtest/gtest.h>
#include <string>
using System::Collections::detail::ElementReference;
using System::Collections::detail::MutationCounter;
using System::Collections::detail::MutationVersion;

TEST(ElementReferenceGuard, ReadsAndRejectedWritesPreserveValueAndCounter) {
    int value=7;MutationCounter counter;int calls=0;
    const std::function<void()> guard=[&]{++calls;throw System::InvalidOperationException("read only");};
    ElementReference<int> reference(&value,&counter,&guard);
    const MutationVersion before=counter;
    EXPECT_EQ(reference.getValueProperty(),7);
    EXPECT_EQ(static_cast<const int&>(reference),7);
    EXPECT_EQ(calls,0);
    const int replacement=8;
    EXPECT_THROW(reference=replacement,System::InvalidOperationException);
    EXPECT_THROW(reference=9,System::InvalidOperationException);
    int source=42;MutationCounter sourceCounter;ElementReference<int> other(&source,&sourceCounter);
    EXPECT_THROW(reference=other,System::InvalidOperationException);
    EXPECT_EQ(value,7);
    EXPECT_EQ(static_cast<MutationVersion>(counter),before);
    EXPECT_EQ(calls,3);
}
TEST(ElementReferenceGuard, EveryCompoundAndIncrementChecksBeforeMutating) {
    int value=7;MutationCounter counter;int calls=0;
    const std::function<void()> guard=[&]{++calls;throw System::InvalidOperationException("blocked");};
    ElementReference<int> reference(&value,&counter,&guard);const MutationVersion before=counter;
    EXPECT_THROW(reference+=1,System::InvalidOperationException);
    EXPECT_THROW(reference-=1,System::InvalidOperationException);
    EXPECT_THROW(reference*=2,System::InvalidOperationException);
    EXPECT_THROW(reference/=2,System::InvalidOperationException);
    EXPECT_THROW(reference%=2,System::InvalidOperationException);
    EXPECT_THROW(reference&=1,System::InvalidOperationException);
    EXPECT_THROW(reference|=1,System::InvalidOperationException);
    EXPECT_THROW(reference^=1,System::InvalidOperationException);
    EXPECT_THROW(reference<<=1,System::InvalidOperationException);
    EXPECT_THROW(reference>>=1,System::InvalidOperationException);
    EXPECT_THROW(++reference,System::InvalidOperationException);
    EXPECT_THROW(--reference,System::InvalidOperationException);
    EXPECT_THROW(reference++,System::InvalidOperationException);
    EXPECT_THROW(reference--,System::InvalidOperationException);
    EXPECT_EQ(calls,14);
    EXPECT_EQ(value,7);
    EXPECT_EQ(static_cast<MutationVersion>(counter),before);
}
TEST(ElementReferenceGuard, AllowedWritesAdvanceExactlyOnceAndGuardTracksChangingAuthority) {
    int value=7;MutationCounter counter;int calls=0;bool allowed=true;
    const std::function<void()> guard=[&]{++calls;if(!allowed)throw System::InvalidOperationException("lost authority");};
    ElementReference<int> reference(&value,&counter,&guard);
    reference=7;
    EXPECT_EQ(static_cast<MutationVersion>(counter),1U);
    reference+=3;
    EXPECT_EQ(value,10);
    EXPECT_EQ(static_cast<MutationVersion>(counter),2U);
    EXPECT_EQ(reference++,10);
    EXPECT_EQ(value,11);
    EXPECT_EQ(static_cast<MutationVersion>(counter),3U);
    auto alias=reference;allowed=false;
    EXPECT_THROW(alias=99,System::InvalidOperationException);
    EXPECT_EQ(value,11);
    EXPECT_EQ(static_cast<MutationVersion>(counter),3U);
    EXPECT_EQ(calls,4);
}
TEST(ElementReferenceGuard, EmptyAndAbsentChecksRetainTheExistingConstructorBehavior) {
    int value=0;MutationCounter counter;const std::function<void()> empty;
    ElementReference<int> old(&value,&counter);old=1;
    ElementReference<int> checked(&value,&counter,&empty);checked=2;
    ElementReference<int> absent(&value,&counter,nullptr);absent=3;
    EXPECT_EQ(value,3);
    EXPECT_EQ(static_cast<MutationVersion>(counter),3U);
}
TEST(ElementReferenceGuard, ForwardedAssignmentUsesDestinationGuardAndLeavesSourceUnchanged) {
    std::string value="old";MutationCounter counter;bool allowed=false;
    const std::function<void()> guard=[&]{if(!allowed)throw System::InvalidOperationException("blocked");};
    ElementReference<std::string> reference(&value,&counter,&guard);
    EXPECT_THROW(reference="new",System::InvalidOperationException);
    EXPECT_EQ(value,"old");
    EXPECT_EQ(static_cast<MutationVersion>(counter),0U);
    allowed=true;reference="new";
    EXPECT_EQ(value,"new");
    EXPECT_EQ(static_cast<MutationVersion>(counter),1U);
}
TEST(ElementReferenceGuard, LegacyAndNewLayoutArePinnedAndConstructorContractsPreserved) {
    struct LegacyLayout {int* slot;MutationCounter* counter;};
    static_assert(sizeof(LegacyLayout)==2*sizeof(void*));
    static_assert(sizeof(ElementReference<int>)==3*sizeof(void*));
    static_assert(alignof(LegacyLayout)==alignof(ElementReference<int>));
    static_assert(std::is_nothrow_constructible_v<ElementReference<int>,int*,MutationCounter*>);
    static_assert(std::is_nothrow_constructible_v<ElementReference<int>,int*,MutationCounter*,const std::function<void()>*>);
    static_assert(std::is_trivially_destructible_v<ElementReference<int>>);
    EXPECT_EQ(sizeof(ElementReference<int>)-sizeof(LegacyLayout),sizeof(void*));
}
