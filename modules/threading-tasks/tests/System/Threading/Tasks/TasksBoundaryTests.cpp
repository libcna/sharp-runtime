// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the Threading.Tasks public-argument boundary.
//
// Ticket #1965 (SR-AUD-231, cause TC-A) -- the CCF-011 empty-callable policy from
// docs/EmptyCallableBoundaryPlan.md applied to every Threading.Tasks entry point that stores a
// callable. Before that ticket an empty std::function was accepted, started, and surfaced as
// std::bad_function_call on a background thread (or, for Parallel, as an AggregateException
// wrapping one per iteration), so an argument error appeared to the caller as a task fault.
//
// This translation unit is the Tasks counterpart of
// modules/threading/tests/System/Threading/ThreadingBoundaryTests.cpp.
#include <gtest/gtest.h>

#include <atomic>
#include <exception>
#include <functional>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "System/AggregateException.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Exception.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/CancellationTokenSource.hpp"
#include "System/Threading/Tasks/Parallel.hpp"
#include "System/Threading/Tasks/Task.hpp"
#include "System/Threading/Tasks/TaskFactory.hpp"

using namespace System::Threading::Tasks;
using System::Threading::CancellationToken;
using System::Threading::CancellationTokenSource;
using SharpRuntime::intcs;

namespace {

    // Every rejection below must carry .NET's own parameter name, because the name is the
    // observable part of the message.
    void expectArgumentNull(const std::function<void()>& call, const std::string& paramName) {
        bool threw = false;
        try {
            call();
        } catch (const System::ArgumentNullException& e) {
            threw = true;
            EXPECT_NE(std::string(e.getMessageProperty()).find("Parameter '" + paramName + "'"),
                      std::string::npos)
                << "message was: " << e.getMessageProperty();
        }
        EXPECT_TRUE(threw) << "expected ArgumentNullException for parameter '" << paramName << "'";
    }

    const std::function<void()> kEmptyAction;
    const std::function<int()> kEmptyFunc;
    const std::function<void(Task)> kEmptyCont;
    const std::function<void(TaskT<int>)> kEmptyContT;
    const std::function<int(TaskT<int>)> kEmptyContFuncT;
    const std::function<void(intcs)> kEmptyBody;
    const std::function<void(intcs, ParallelLoopState&)> kEmptyStateBody;
    const std::function<void(int)> kEmptyEachBody;
    const std::function<void(int, ParallelLoopState&)> kEmptyEachStateBody;

} // namespace

// ---------------------------------------------------------------------------------------------
// Task -- four constructor/factory entries plus ContinueWith
// ---------------------------------------------------------------------------------------------

TEST(TasksEmptyCallableBoundaryTests, TaskCtor_EmptyAction_ThrowsArgumentNull) {
    expectArgumentNull([] { Task t(kEmptyAction); }, "action");
}

TEST(TasksEmptyCallableBoundaryTests, TaskCtorWithToken_EmptyAction_ThrowsArgumentNull) {
    expectArgumentNull([] { Task t(kEmptyAction, CancellationToken::None()); }, "action");
}

TEST(TasksEmptyCallableBoundaryTests, TaskRun_EmptyAction_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)Task::Run(kEmptyAction); }, "action");
}

TEST(TasksEmptyCallableBoundaryTests, TaskRunWithToken_EmptyAction_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)Task::Run(kEmptyAction, CancellationToken::None()); }, "action");
}

// The argument check precedes the already-cancelled-token short circuit, matching .NET, whose
// Task.Run validates the delegate in InternalStartNew before the task exists. Before #1965 this
// call quietly produced a Canceled task and never reported the bad argument at all.
TEST(TasksEmptyCallableBoundaryTests, TaskCtor_EmptyActionWithCancelledToken_StillReportsArgument) {
    CancellationTokenSource cts;
    cts.Cancel();
    expectArgumentNull([&] { Task t(kEmptyAction, cts.getTokenProperty()); }, "action");
}

TEST(TasksEmptyCallableBoundaryTests, ContinueWith_EmptyAction_OnCompletedAntecedent_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)Task::CompletedTask().ContinueWith(kEmptyCont); },
                       "continuationAction");
}

// The deferred-registration path: the antecedent is still running when ContinueWith is called,
// so before #1965 the failure moved to the antecedent's own worker thread.
TEST(TasksEmptyCallableBoundaryTests, ContinueWith_EmptyAction_OnRunningAntecedent_ThrowsArgumentNull) {
    std::atomic<bool> release{false};
    Task antecedent = Task::Run([&] {
        while (!release.load()) { /* deterministic hold, no sleep-based timing */ }
    });
    expectArgumentNull([&] { (void)antecedent.ContinueWith(kEmptyCont); }, "continuationAction");
    release.store(true);
    antecedent.Wait();
}

// No-partial-state: a rejected ContinueWith must leave NO continuation registered, so the
// antecedent completes normally and nothing runs afterwards.
TEST(TasksEmptyCallableBoundaryTests, ContinueWith_Rejected_RegistersNoContinuation) {
    std::atomic<bool> release{false};
    std::atomic<int> ran{0};
    Task antecedent = Task::Run([&] {
        while (!release.load()) { }
    });
    EXPECT_THROW((void)antecedent.ContinueWith(kEmptyCont), System::ArgumentNullException);
    // A real continuation registered afterwards is the only one that may run.
    Task good = antecedent.ContinueWith(std::function<void(Task)>([&](Task) { ++ran; }));
    release.store(true);
    antecedent.Wait();
    good.Wait();
    EXPECT_EQ(ran.load(), 1);
    EXPECT_TRUE(good.getIsCompletedSuccessfullyProperty());
}

// No-partial-state: a rejected Task constructor starts no worker at all.
TEST(TasksEmptyCallableBoundaryTests, TaskCtor_Rejected_StartsNoWorker) {
    std::atomic<int> ran{0};
    EXPECT_THROW(Task t(kEmptyAction), System::ArgumentNullException);
    Task good = Task::Run([&] { ++ran; });
    good.Wait();
    EXPECT_EQ(ran.load(), 1);
}

// The property std::bad_function_call never had: the rejection is inside System::Exception.
TEST(TasksEmptyCallableBoundaryTests, Rejection_IsCatchableAsSystemException) {
    EXPECT_THROW((void)Task::Run(kEmptyAction), System::Exception);
    EXPECT_THROW((void)Task::Run(kEmptyAction), System::ArgumentException);
    EXPECT_THROW((void)Task::Run(kEmptyAction), System::ArgumentNullException);
}

TEST(TasksEmptyCallableBoundaryTests, ValidAction_StillRuns) {
    std::atomic<int> ran{0};
    Task t = Task::Run([&] { ++ran; });
    t.Wait();
    EXPECT_EQ(ran.load(), 1);
    EXPECT_TRUE(t.getIsCompletedSuccessfullyProperty());
}

// ---------------------------------------------------------------------------------------------
// TaskT<TResult> -- .NET names this delegate `function`, not `action`
// ---------------------------------------------------------------------------------------------

TEST(TasksEmptyCallableBoundaryTests, TaskTCtor_EmptyFunc_ThrowsArgumentNullNamedFunction) {
    expectArgumentNull([] { TaskT<int> t(kEmptyFunc); }, "function");
}

TEST(TasksEmptyCallableBoundaryTests, TaskTCtorWithToken_EmptyFunc_ThrowsArgumentNull) {
    expectArgumentNull([] { TaskT<int> t(kEmptyFunc, CancellationToken::None()); }, "function");
}

TEST(TasksEmptyCallableBoundaryTests, TaskTRun_EmptyFunc_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)TaskT<int>::Run(kEmptyFunc); }, "function");
}

TEST(TasksEmptyCallableBoundaryTests, TaskTRunWithToken_EmptyFunc_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)TaskT<int>::Run(kEmptyFunc, CancellationToken::None()); },
                       "function");
}

TEST(TasksEmptyCallableBoundaryTests, TaskTContinueWithAction_Empty_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)TaskT<int>::FromResult(1).ContinueWith(kEmptyContT); },
                       "continuationAction");
}

// The result-producing overload takes an unconstrained callable type rather than a
// std::function, so its check goes through detail::isEmptyCallable.
TEST(TasksEmptyCallableBoundaryTests, TaskTContinueWithFunc_Empty_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)TaskT<int>::FromResult(1).ContinueWith(kEmptyContFuncT); },
                       "continuationFunction");
}

// A lambda has no null state and must never be mistaken for one.
TEST(TasksEmptyCallableBoundaryTests, TaskTContinueWithFunc_Lambda_StillRuns) {
    TaskT<int> doubled =
        TaskT<int>::FromResult(21).ContinueWith([](TaskT<int> t) { return t.getResultProperty() * 2; });
    EXPECT_EQ(doubled.getResultProperty(), 42);
}

TEST(TasksEmptyCallableBoundaryTests, TaskTValidFunc_StillProducesResult) {
    TaskT<int> t = TaskT<int>::Run(std::function<int()>([] { return 9; }));
    EXPECT_EQ(t.getResultProperty(), 9);
}

// ---------------------------------------------------------------------------------------------
// TaskFactory -- inherits the checks by delegation, with .NET's per-overload parameter names
// ---------------------------------------------------------------------------------------------

TEST(TasksEmptyCallableBoundaryTests, FactoryStartNewAction_Empty_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)TaskFactory().StartNew(kEmptyAction); }, "action");
}

TEST(TasksEmptyCallableBoundaryTests, FactoryStartNewActionWithToken_Empty_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)TaskFactory().StartNew(kEmptyAction, CancellationToken::None()); },
                       "action");
}

TEST(TasksEmptyCallableBoundaryTests, FactoryStartNewFunc_Empty_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)TaskFactory().StartNew<int>(kEmptyFunc); }, "function");
}

TEST(TasksEmptyCallableBoundaryTests, FactoryStartNewFuncWithToken_Empty_ThrowsArgumentNull) {
    expectArgumentNull(
        [] { (void)TaskFactory().StartNew<int>(kEmptyFunc, CancellationToken::None()); }, "function");
}

TEST(TasksEmptyCallableBoundaryTests, TaskDotFactoryStartNew_Empty_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)Task::Factory().StartNew(kEmptyAction); }, "action");
}

// ---------------------------------------------------------------------------------------------
// Parallel -- five loop entries named `body`, plus Invoke, which is NOT an ArgumentNull site
// ---------------------------------------------------------------------------------------------

TEST(TasksEmptyCallableBoundaryTests, ParallelFor_EmptyBody_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)Parallel::For(0, 3, kEmptyBody); }, "body");
}

TEST(TasksEmptyCallableBoundaryTests, ParallelForWithOptions_EmptyBody_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)Parallel::For(0, 3, ParallelOptions{}, kEmptyBody); }, "body");
}

TEST(TasksEmptyCallableBoundaryTests, ParallelForWithLoopState_EmptyBody_ThrowsArgumentNull) {
    expectArgumentNull([] { (void)Parallel::For(0, 3, kEmptyStateBody); }, "body");
}

TEST(TasksEmptyCallableBoundaryTests, ParallelForEach_EmptyBody_ThrowsArgumentNull) {
    expectArgumentNull([] {
        std::vector<int> source{1, 2, 3};
        (void)Parallel::ForEach<int>(source, kEmptyEachBody);
    }, "body");
}

TEST(TasksEmptyCallableBoundaryTests, ParallelForEachWithLoopState_EmptyBody_ThrowsArgumentNull) {
    expectArgumentNull([] {
        std::vector<int> source{1, 2, 3};
        (void)Parallel::ForEach<int>(source, kEmptyEachStateBody);
    }, "body");
}

// The data-dependence this ticket removed: with a zero-length range or an empty source the body
// was never reached, so the same wrong call returned a normally completed ParallelLoopResult.
TEST(TasksEmptyCallableBoundaryTests, ParallelFor_EmptyBodyAndEmptyRange_StillThrows) {
    expectArgumentNull([] { (void)Parallel::For(0, 0, kEmptyBody); }, "body");
    expectArgumentNull([] { (void)Parallel::For(0, 0, ParallelOptions{}, kEmptyBody); }, "body");
    expectArgumentNull([] { (void)Parallel::For(0, 0, kEmptyStateBody); }, "body");
}

TEST(TasksEmptyCallableBoundaryTests, ParallelForEach_EmptyBodyAndEmptySource_StillThrows) {
    expectArgumentNull([] {
        std::vector<int> empty;
        (void)Parallel::ForEach<int>(empty, kEmptyEachBody);
    }, "body");
    expectArgumentNull([] {
        std::vector<int> empty;
        (void)Parallel::ForEach<int>(empty, kEmptyEachStateBody);
    }, "body");
}

// No-partial-state: an invalid body runs no iteration at all.
TEST(TasksEmptyCallableBoundaryTests, ParallelFor_Rejected_RunsNoIteration) {
    std::atomic<int> ran{0};
    EXPECT_THROW((void)Parallel::For(0, 8, kEmptyBody), System::ArgumentNullException);
    EXPECT_EQ(ran.load(), 0);
    (void)Parallel::For(0, 4, std::function<void(intcs)>([&](intcs) { ++ran; }));
    EXPECT_EQ(ran.load(), 4);
}

// Parallel.Invoke is the one entry whose .NET counterpart reports a null ELEMENT with a plain
// ArgumentException carrying no parameter name (SR.Parallel_Invoke_ActionNull), not with
// ArgumentNullException. Pinned so the family's usual spelling is not "corrected" back in.
TEST(TasksEmptyCallableBoundaryTests, ParallelInvoke_EmptyAction_ThrowsArgumentExceptionNotNull) {
    bool threw = false;
    try {
        Parallel::Invoke({kEmptyAction});
    } catch (const System::ArgumentNullException&) {
        FAIL() << "Parallel::Invoke must report a null element as ArgumentException, not "
                  "ArgumentNullException";
    } catch (const System::ArgumentException& e) {
        threw = true;
        EXPECT_EQ(std::string(e.getMessageProperty()), "One of the actions was null.");
    }
    EXPECT_TRUE(threw);
}

// No-partial-state: every element is validated before any action starts, so a mixed list runs
// nothing. Before #1965 the valid action ran to completion and only then did the empty one
// surface as an AggregateException.
TEST(TasksEmptyCallableBoundaryTests, ParallelInvoke_MixedList_RunsNothing) {
    std::atomic<int> ran{0};
    EXPECT_THROW(Parallel::Invoke({std::function<void()>([&] { ++ran; }), kEmptyAction}),
                 System::ArgumentException);
    EXPECT_EQ(ran.load(), 0);
}

// An empty argument LIST is not an empty callable: .NET's Parallel.Invoke() with no actions is
// legal and does nothing.
TEST(TasksEmptyCallableBoundaryTests, ParallelInvoke_NoActions_IsLegal) {
    EXPECT_NO_THROW(Parallel::Invoke({}));
}

TEST(TasksEmptyCallableBoundaryTests, ParallelInvoke_ValidActions_StillRun) {
    std::atomic<int> ran{0};
    Parallel::Invoke({std::function<void()>([&] { ++ran; }), std::function<void()>([&] { ++ran; })});
    EXPECT_EQ(ran.load(), 2);
}

// ---------------------------------------------------------------------------------------------
// No public Threading.Tasks entry can still reach std::bad_function_call
// ---------------------------------------------------------------------------------------------

TEST(TasksEmptyCallableBoundaryTests, NoEntryPointReachesBadFunctionCall) {
    const std::vector<std::function<void()>> calls{
        [] { Task t(kEmptyAction); },
        [] { Task t(kEmptyAction, CancellationToken::None()); },
        [] { (void)Task::Run(kEmptyAction); },
        [] { (void)Task::Run(kEmptyAction, CancellationToken::None()); },
        [] { (void)Task::CompletedTask().ContinueWith(kEmptyCont); },
        [] { TaskT<int> t(kEmptyFunc); },
        [] { TaskT<int> t(kEmptyFunc, CancellationToken::None()); },
        [] { (void)TaskT<int>::Run(kEmptyFunc); },
        [] { (void)TaskT<int>::Run(kEmptyFunc, CancellationToken::None()); },
        [] { (void)TaskT<int>::FromResult(1).ContinueWith(kEmptyContT); },
        [] { (void)TaskT<int>::FromResult(1).ContinueWith(kEmptyContFuncT); },
        [] { (void)TaskFactory().StartNew(kEmptyAction); },
        [] { (void)TaskFactory().StartNew(kEmptyAction, CancellationToken::None()); },
        [] { (void)TaskFactory().StartNew<int>(kEmptyFunc); },
        [] { (void)TaskFactory().StartNew<int>(kEmptyFunc, CancellationToken::None()); },
        [] { (void)Task::Factory().StartNew(kEmptyAction); },
        [] { (void)Parallel::For(0, 3, kEmptyBody); },
        [] { (void)Parallel::For(0, 3, ParallelOptions{}, kEmptyBody); },
        [] { (void)Parallel::For(0, 3, kEmptyStateBody); },
        [] { std::vector<int> s{1}; (void)Parallel::ForEach<int>(s, kEmptyEachBody); },
        [] { std::vector<int> s{1}; (void)Parallel::ForEach<int>(s, kEmptyEachStateBody); },
        [] { Parallel::Invoke({kEmptyAction}); },
    };

    for (size_t i = 0; i < calls.size(); ++i) {
        bool sawSystemException = false;
        try {
            calls[i]();
        } catch (const System::AggregateException&) {
            // Listed before System::Exception: AggregateException derives from it, and a
            // deferred per-iteration bad_function_call used to arrive wrapped in one.
            FAIL() << "entry " << i << " still defers the argument error into an aggregate";
        } catch (const System::Exception&) {
            sawSystemException = true;
        } catch (const std::bad_function_call&) {
            FAIL() << "entry " << i << " still reaches std::bad_function_call";
        }
        EXPECT_TRUE(sawSystemException) << "entry " << i << " accepted an empty callable";
    }
}

// ---------------------------------------------------------------------------------------------
// Ticket #1966 (SR-AUD-232, cause TC-B/1) -- ParallelOptions::MaxDegreeOfParallelism
//
// Before that ticket every value <= 0 was silently rewritten to hardware_concurrency(), so 0 and
// -2 ran a completed loop where .NET throws ArgumentOutOfRangeException, and the substitution
// raised the effective degree to the machine's core count -- the opposite of what a degree cap
// is for. Only -1 is a valid special value.
// ---------------------------------------------------------------------------------------------

namespace {

    ParallelLoopResult runForWithDegree(intcs degree, intcs iterations, std::atomic<intcs>& ran) {
        ParallelOptions opts;
        opts.setMaxDegreeOfParallelismProperty(degree);
        return Parallel::For(0, iterations, opts,
                             std::function<void(intcs)>([&](intcs) { ++ran; }));
    }

    // #2388 MOVED THE POINT OF REJECTION, and this helper moved with it rather than being left
    // to pass for the wrong reason. #1966 could only validate at the entry of Parallel::For,
    // because the degree was a public data member with nowhere to put a check; the guard now
    // lives in the setter, where .NET has it, so an invalid degree is refused at ASSIGNMENT and
    // can never reach a loop at all.
    //
    // The "ran 0 iterations" half is kept and is now trivially true -- no loop is ever started.
    // That is the stronger property, and it is .NET's: there, too, an invalid degree cannot be
    // handed to Parallel.For, because ParallelOptions will not hold one.
    void expectDegreeRejected(intcs degree) {
        std::atomic<intcs> ran{0};
        bool threw = false;
        try {
            (void)runForWithDegree(degree, 64, ran);
        } catch (const System::ArgumentOutOfRangeException& e) {
            threw = true;
            EXPECT_NE(std::string(e.getMessageProperty()).find("Parameter 'MaxDegreeOfParallelism'"),
                      std::string::npos)
                << "message was: " << e.getMessageProperty();
        }
        EXPECT_TRUE(threw) << "degree " << degree << " should have been rejected";
        EXPECT_EQ(ran.load(), 0) << "degree " << degree << " ran " << ran.load() << " iterations";
    }

} // namespace

TEST(ParallelDegreeBoundaryTests, DegreeMinusTwo_IsRejectedAndRunsNoBody) {
    expectDegreeRejected(-2);
}

TEST(ParallelDegreeBoundaryTests, DegreeMinusThree_IsRejectedAndRunsNoBody) {
    expectDegreeRejected(-3);
}

TEST(ParallelDegreeBoundaryTests, DegreeZero_IsRejectedAndRunsNoBody) {
    expectDegreeRejected(0);
}

TEST(ParallelDegreeBoundaryTests, DegreeIntMin_IsRejected) {
    expectDegreeRejected(std::numeric_limits<intcs>::min());
}

// -1 is .NET's "unlimited" sentinel and stays valid; this runtime realises it as
// hardware_concurrency(), which is what .NET's own default does.
TEST(ParallelDegreeBoundaryTests, DegreeMinusOne_StaysValidAndRunsEveryIteration) {
    std::atomic<intcs> ran{0};
    ParallelLoopResult result = runForWithDegree(-1, 16, ran);
    EXPECT_TRUE(result.getIsCompletedProperty());
    EXPECT_EQ(ran.load(), 16);
}

TEST(ParallelDegreeBoundaryTests, DegreeOne_IsHonouredExactly) {
    // Deterministic concurrency check: with a degree of 1 no two iterations may ever overlap.
    std::atomic<intcs> inFlight{0};
    std::atomic<intcs> peak{0};
    std::atomic<intcs> ran{0};
    ParallelOptions opts;
    opts.setMaxDegreeOfParallelismProperty(1);
    ParallelLoopResult result =
        Parallel::For(0, 12, opts, std::function<void(intcs)>([&](intcs) {
            ++ran;
            intcs now = ++inFlight;
            intcs seen = peak.load();
            while (now > seen && !peak.compare_exchange_weak(seen, now)) { }
            --inFlight;
        }));
    EXPECT_TRUE(result.getIsCompletedProperty());
    EXPECT_EQ(ran.load(), 12);
    EXPECT_EQ(peak.load(), 1);
}

TEST(ParallelDegreeBoundaryTests, DegreeAboveCoreCount_IsAcceptedAndRunsEveryIteration) {
    const intcs above = static_cast<intcs>(std::thread::hardware_concurrency()) + 4;
    std::atomic<intcs> ran{0};
    ParallelLoopResult result = runForWithDegree(above, 16, ran);
    EXPECT_TRUE(result.getIsCompletedProperty());
    EXPECT_EQ(ran.load(), 16);
}

// #1966 asserted that the degree error WINS OVER #1965's body error, because .NET rejects the
// invalid degree in the ParallelOptions setter, which runs before Parallel.For is even called.
// #2388 put the guard in the setter, so there is no longer an ordering to assert: the invalid
// degree cannot reach Parallel::For at all, which is the situation .NET is actually in. Asserting
// the old ordering here would now pass for the wrong reason -- the throw would come from the
// assignment, outside the EXPECT_THROW, exactly the trap #2359 hit.
TEST(ParallelDegreeBoundaryTests, Fix2388_AnInvalidDegreeCannotReachParallelForAtAll) {
    ParallelOptions opts;
    EXPECT_THROW(opts.setMaxDegreeOfParallelismProperty(0),
                 System::ArgumentOutOfRangeException);

    // The rejected assignment left the previous value in place, so the options object is still
    // usable and the loop runs -- there is no half-applied state to trip over.
    EXPECT_EQ(opts.getMaxDegreeOfParallelismProperty(), -1);
    std::atomic<intcs> ran{0};
    EXPECT_TRUE(Parallel::For(0, 4, opts, std::function<void(intcs)>([&](intcs) { ++ran; }))
                    .getIsCompletedProperty());
    EXPECT_EQ(ran.load(), 4);
}

// The four overloads that take no options are untouched by this ticket.
TEST(ParallelDegreeBoundaryTests, OverloadsWithoutOptions_AreUnchanged) {
    std::atomic<intcs> ran{0};
    EXPECT_TRUE(Parallel::For(0, 5, std::function<void(intcs)>([&](intcs) { ++ran; }))
                    .getIsCompletedProperty());
    EXPECT_EQ(ran.load(), 5);

    ran.store(0);
    EXPECT_TRUE(Parallel::For(0, 5, std::function<void(intcs, ParallelLoopState&)>(
                                        [&](intcs, ParallelLoopState&) { ++ran; }))
                    .getIsCompletedProperty());
    EXPECT_EQ(ran.load(), 5);

    ran.store(0);
    std::vector<int> source{1, 2, 3, 4, 5};
    EXPECT_TRUE(Parallel::ForEach<int>(source, std::function<void(int)>([&](int) { ++ran; }))
                    .getIsCompletedProperty());
    EXPECT_EQ(ran.load(), 5);
}

TEST(ParallelDegreeBoundaryTests, Rejection_IsCatchableAsArgumentException) {
    // #2388: the rejection moved from Parallel::For to the setter, so the call under test moved
    // with it.
    ParallelOptions opts;
    const auto call = [&] { opts.setMaxDegreeOfParallelismProperty(0); };
    EXPECT_THROW(call(), System::ArgumentOutOfRangeException);
    EXPECT_THROW(call(), System::ArgumentException);
    EXPECT_THROW(call(), System::Exception);
}

// =============================================================================================
// #2388 — the MaxDegreeOfParallelism guard moved into the setter, where .NET has it.
//
// #1966 landed the same validation at the entry of every Parallel method, because the degree was
// a public mutable data member with nowhere to put a check, and recorded that as a forced choice
// awaiting an approval. SA-8 granted it and #1969 landed the identical change for
// BoundedChannelOptions::FullMode; this is the ParallelOptions half.
// =============================================================================================

TEST(ParallelOptionsSetterTests, Fix2388_TheDefaultIsMinusOne) {
    EXPECT_EQ(ParallelOptions().getMaxDegreeOfParallelismProperty(), -1);
}

TEST(ParallelOptionsSetterTests, Fix2388_ZeroAndBelowMinusOneAreRejectedAtAssignment) {
    for (intcs degree : {intcs{0}, intcs{-2}, intcs{-3}, SharpRuntime::INTCS_MIN}) {
        ParallelOptions opts;
        EXPECT_THROW(opts.setMaxDegreeOfParallelismProperty(degree),
                     System::ArgumentOutOfRangeException)
            << "degree " << degree;
        EXPECT_EQ(opts.getMaxDegreeOfParallelismProperty(), -1)
            << "a rejected assignment must not half-apply; degree " << degree;
    }
}

TEST(ParallelOptionsSetterTests, Fix2388_MinusOneAndEveryPositiveAreAccepted) {
    // -1 is the ONE valid negative, and the boundary either side of it is what a `< -1` guard
    // written as `<= -1` or `< 0` would get wrong.
    for (intcs degree : {intcs{-1}, intcs{1}, intcs{2}, intcs{64}, SharpRuntime::INTCS_MAX}) {
        ParallelOptions opts;
        ASSERT_NO_THROW(opts.setMaxDegreeOfParallelismProperty(degree)) << "degree " << degree;
        EXPECT_EQ(opts.getMaxDegreeOfParallelismProperty(), degree);
    }
}

TEST(ParallelOptionsSetterTests, Fix2388_TheParameterNameIsThePropertyNotValue) {
    // .NET writes nameof(MaxDegreeOfParallelism) here (Parallel.cs:87-88) and nameof(value) in
    // BoundedChannelOptions (ChannelOptions.cs:96). The reference is inconsistent between its two
    // option types and both are transcribed as they are, so "harmonising" this onto #1969's
    // "value" would be inventing a reference.
    ParallelOptions opts;
    try {
        opts.setMaxDegreeOfParallelismProperty(0);
        FAIL() << "0 must be rejected";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "MaxDegreeOfParallelism");
    }
}
