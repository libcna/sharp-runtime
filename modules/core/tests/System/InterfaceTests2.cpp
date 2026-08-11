// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
// Tests for: IFormatProvider, IFormattable, IObservable, IObserver,
//            IParsable, IProgress, IServiceProvider, ISpanFormattable
#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <typeinfo>
#include <vector>
#include "System/ArgumentNullException.hpp"
#include "System/IDisposable.hpp"
#include "System/IFormatProvider.hpp"
#include "System/IFormattable.hpp"
#include "System/IObservable.hpp"
#include "System/IObserver.hpp"
#include "System/IParsable.hpp"
#include "System/IProgress.hpp"
#include "System/IServiceProvider.hpp"
#include "System/ISpanFormattable.hpp"

// ---------------------------------------------------------------------------
// IFormatProvider
// ---------------------------------------------------------------------------
struct NullFormatProvider : public System::IFormatProvider {
    void* GetFormat(const std::type_info&) const override { return nullptr; }
};

TEST(IFormatProviderTests2, GetFormat_ReturnsNull) {
    NullFormatProvider p;
    EXPECT_EQ(p.GetFormat(typeid(int)), nullptr);
}

// ---------------------------------------------------------------------------
// IFormattable
// ---------------------------------------------------------------------------
struct FormattableVal : public System::IFormattable {
    int value;
    explicit FormattableVal(int v) : value(v) {}
    std::string ToString(const std::string& /*fmt*/) const override {
        return std::to_string(value);
    }
};

TEST(IFormattableTests2, ToString_ReturnsString) {
    FormattableVal fv(42);
    EXPECT_EQ(fv.ToString("D"), "42");
}

TEST(IFormattableTests2, ToString_WithProvider_DefaultsToOneArgOverload) {
    FormattableVal fv(42);
    const System::IFormattable& base = fv;
    EXPECT_EQ(base.ToString("D", nullptr), "42");
}

// ---------------------------------------------------------------------------
// IObservable / IObserver
// ---------------------------------------------------------------------------
struct IntObserver2 : public System::IObserver<int> {
    std::vector<int> received;
    bool completed = false;
    bool errored = false;
    void OnNext(const int& v) override { received.push_back(v); }
    void OnCompleted() override { completed = true; }
    void OnError(const std::exception&) override { errored = true; }
};

// SR-AUD-056 (#2302). This fixture is used as behavioural evidence, so it must
// honour the two contracts its own interfaces document, not merely compile:
// IObservable<T>::Subscribe returns an IDisposable "that allows observers to
// stop receiving notifications before the provider has finished sending them",
// and IObserver<T> states that after a terminal OnError or OnCompleted the
// provider makes no further OnNext or OnCompleted call. The previous version
// returned nullptr from Subscribe and left Emit fully functional after
// Complete(), so a future implementation could have copied both defects without
// a failing regression.
//
// The observer list lives in a State that the provider and every subscription
// co-own, which is how this repository already isolates a view's storage from
// its owner's lifetime (SortedSet<T>, ticket #1786). A subscription therefore
// stays safe to dispose whatever order things are destroyed in, and there is no
// ownership cycle: the subscription owns State, State owns the observers, and an
// observer owns nothing.
struct IntObservable2 : public System::IObservable<int> {
    struct State {
        std::vector<std::pair<int, std::shared_ptr<System::IObserver<int>>>> observers;
        bool terminated = false;
    };

    // A subscription is the fixture's IDisposable: disposing it unsubscribes
    // exactly its own observer, and disposing twice is a no-op, as IDisposable
    // requires of every implementation.
    class Subscription final : public System::IDisposable {
        std::shared_ptr<State> state_;
        int token_;
    public:
        Subscription(std::shared_ptr<State> state, int token)
            : state_(std::move(state)), token_(token) {}
        void Dispose() override {
            if (!state_) { return; }
            std::erase_if(state_->observers,
                          [this](const auto& entry) { return entry.first == token_; });
            state_.reset();
        }
        // Deliberately NOT disposing here. Unsubscription is tied to Dispose(),
        // as IObservable<T> documents, not to the handle's lifetime: dropping
        // the returned shared_ptr must leave the observer subscribed, which is
        // what .NET does and what this file's first two cases rely on.
        ~Subscription() override = default;
    };

    std::shared_ptr<State> state = std::make_shared<State>();
    int nextToken = 0;

    std::shared_ptr<System::IDisposable> Subscribe(
            std::shared_ptr<System::IObserver<int>> obs) override {
        if (!obs) {
            throw System::ArgumentNullException("observer");
        }
        const int token = nextToken++;
        state->observers.emplace_back(token, std::move(obs));
        return std::make_shared<Subscription>(state, token);
    }

    /** Delivers a value, unless a terminal notification has already been sent. */
    void Emit(int v) {
        if (state->terminated) { return; }
        for (auto& [token, o] : state->observers) { (void)token; o->OnNext(v); }
    }
    /** Terminal path one. Subsequent Emit/Complete/Fail deliver nothing. */
    void Complete() {
        if (state->terminated) { return; }
        state->terminated = true;
        for (auto& [token, o] : state->observers) { (void)token; o->OnCompleted(); }
    }
    /** Terminal path two, the one the report notes was never covered. */
    void Fail(const std::exception& error) {
        if (state->terminated) { return; }
        state->terminated = true;
        for (auto& [token, o] : state->observers) { (void)token; o->OnError(error); }
    }
    [[nodiscard]] std::size_t subscriberCount() const { return state->observers.size(); }
};

TEST(IObservableTests2, Subscribe_AndReceiveValues) {
    IntObservable2 src;
    auto obs = std::make_shared<IntObserver2>();
    auto subscription = src.Subscribe(obs);
    ASSERT_NE(subscription, nullptr);
    src.Emit(10); src.Emit(20);
    ASSERT_EQ(obs->received.size(), 2u);
    EXPECT_EQ(obs->received[0], 10);
    EXPECT_EQ(obs->received[1], 20);
}

TEST(IObserverTests2, OnCompleted_SetsFlag) {
    IntObservable2 src;
    auto obs = std::make_shared<IntObserver2>();
    auto subscription = src.Subscribe(obs);
    src.Complete();
    EXPECT_TRUE(obs->completed);
}

// The subscription handle is what IObservable<T> documents Subscribe to return.
TEST(IObservableTests2, Subscribe_ReturnsADisposableSubscription) {
    IntObservable2 src;
    auto obs = std::make_shared<IntObserver2>();
    std::shared_ptr<System::IDisposable> subscription = src.Subscribe(obs);
    ASSERT_NE(subscription, nullptr);
    EXPECT_EQ(src.subscriberCount(), 1u);
}

TEST(IObservableTests2, DisposedSubscription_StopsReceivingValues) {
    IntObservable2 src;
    auto obs = std::make_shared<IntObserver2>();
    auto subscription = src.Subscribe(obs);
    src.Emit(1);
    subscription->Dispose();
    EXPECT_EQ(src.subscriberCount(), 0u);
    src.Emit(2);
    ASSERT_EQ(obs->received.size(), 1u);
    EXPECT_EQ(obs->received[0], 1);
}

TEST(IObservableTests2, DisposingASubscriptionTwiceIsANoOp) {
    IntObservable2 src;
    auto first = std::make_shared<IntObserver2>();
    auto second = std::make_shared<IntObserver2>();
    auto firstSubscription = src.Subscribe(first);
    src.Subscribe(second);
    ASSERT_EQ(src.subscriberCount(), 2u);

    firstSubscription->Dispose();
    firstSubscription->Dispose();
    EXPECT_EQ(src.subscriberCount(), 1u);

    src.Emit(7);
    EXPECT_TRUE(first->received.empty());
    ASSERT_EQ(second->received.size(), 1u);
    EXPECT_EQ(second->received[0], 7);
}

TEST(IObservableTests2, SubscribeRejectsANullObserver) {
    IntObservable2 src;
    EXPECT_THROW(src.Subscribe(nullptr), System::ArgumentNullException);
    EXPECT_EQ(src.subscriberCount(), 0u);
}

// IObserver<T> guarantees no OnNext or OnCompleted follows a terminal call.
TEST(IObserverTests2, NoValueIsDeliveredAfterCompletion) {
    IntObservable2 src;
    auto obs = std::make_shared<IntObserver2>();
    src.Subscribe(obs);
    src.Emit(1);
    src.Complete();
    src.Emit(2);
    ASSERT_EQ(obs->received.size(), 1u);
    EXPECT_EQ(obs->received[0], 1);
    EXPECT_TRUE(obs->completed);
    EXPECT_FALSE(obs->errored);
}

TEST(IObserverTests2, CompletionIsNotDeliveredTwice) {
    IntObservable2 src;
    struct CountingObserver final : public System::IObserver<int> {
        int completions = 0;
        void OnNext(const int&) override {}
        void OnCompleted() override { ++completions; }
        void OnError(const std::exception&) override {}
    };
    auto obs = std::make_shared<CountingObserver>();
    src.Subscribe(obs);
    src.Complete();
    src.Complete();
    EXPECT_EQ(obs->completions, 1);
}

// OnError is the other terminal path, and the report notes it was never covered.
TEST(IObserverTests2, OnError_IsTerminalToo) {
    IntObservable2 src;
    auto obs = std::make_shared<IntObserver2>();
    src.Subscribe(obs);
    src.Fail(std::runtime_error("provider failed"));
    EXPECT_TRUE(obs->errored);
    EXPECT_FALSE(obs->completed);

    src.Emit(5);
    src.Complete();
    EXPECT_TRUE(obs->received.empty());
    EXPECT_FALSE(obs->completed);
}

// ---------------------------------------------------------------------------
// IParsable
// ---------------------------------------------------------------------------
struct ParseableInt : public System::IParsable<ParseableInt> {
    int value = 0;
    ParseableInt Parse(const std::string& s, const System::IFormatProvider*) override {
        ParseableInt r; r.value = std::stoi(s); return r;
    }
    bool TryParse(const std::string& s, const System::IFormatProvider*, ParseableInt& result) override {
        try { result = Parse(s, nullptr); return true; }
        catch (...) { return false; }
    }
};

TEST(IParsableTests2, Parse_IntFromString) {
    ParseableInt p;
    auto r = p.Parse("99", nullptr);
    EXPECT_EQ(r.value, 99);
}

TEST(IParsableTests2, TryParse_InvalidInput_ReturnsFalse) {
    ParseableInt p;
    ParseableInt r;
    EXPECT_FALSE(p.TryParse("xyz", nullptr, r));
}

// ---------------------------------------------------------------------------
// IProgress
// ---------------------------------------------------------------------------
struct ProgressCapture2 : public System::IProgress<int> {
    std::vector<int> reports;
    void Report(const int& v) override { reports.push_back(v); }
};

TEST(IProgressTests2, Report_CapturesValues) {
    ProgressCapture2 pc;
    pc.Report(10); pc.Report(50); pc.Report(100);
    ASSERT_EQ(pc.reports.size(), 3u);
    EXPECT_EQ(pc.reports[2], 100);
}

// ---------------------------------------------------------------------------
// IServiceProvider
// ---------------------------------------------------------------------------
struct SimpleServiceProvider2 : public System::IServiceProvider {
    void* GetService(const std::type_info&) const override { return nullptr; }
};

TEST(IServiceProviderTests2, GetService_ReturnsNull) {
    SimpleServiceProvider2 sp;
    EXPECT_EQ(sp.GetService(typeid(int)), nullptr);
}

// ---------------------------------------------------------------------------
// ISpanFormattable
// ---------------------------------------------------------------------------
struct SpanFormattableInt2 : public System::ISpanFormattable {
    int value;
    explicit SpanFormattableInt2(int v) : value(v) {}
    std::string ToString(const std::string& /*fmt*/) const override {
        return std::to_string(value);
    }
    bool TryFormat(char* dest, std::size_t destLen,
                   std::size_t& charsWritten,
                   const std::string& /*fmt*/) const override {
        std::string s = std::to_string(value);
        if (s.size() > destLen) { charsWritten = 0; return false; }
        std::copy(s.begin(), s.end(), dest);
        charsWritten = s.size();
        return true;
    }
};

TEST(ISpanFormattableTests2, TryFormat_WritesValue) {
    SpanFormattableInt2 sfi(77);
    char buf[32];
    std::size_t written = 0;
    EXPECT_TRUE(sfi.TryFormat(buf, sizeof(buf), written, ""));
    EXPECT_EQ(std::string(buf, written), "77");
}

TEST(ISpanFormattableTests2, TryFormat_WithProvider_IgnoresProviderAndDelegates) {
    // Default TryFormat(..., provider) implementation forwards to the 4-arg overload
    // without requiring implementers to override it (matching the IFormattable::ToString fix).
    // Invoked through the base interface, since the derived class's 4-arg override
    // otherwise hides the base's 5-arg overload from unqualified name lookup.
    SpanFormattableInt2 sfi(123);
    const System::ISpanFormattable& base = sfi;
    char buf[32];
    std::size_t written = 0;
    EXPECT_TRUE(base.TryFormat(buf, sizeof(buf), written, "", nullptr));
    EXPECT_EQ(std::string(buf, written), "123");
}
