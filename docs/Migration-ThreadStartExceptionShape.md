<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ThreadStartException` has .NET's constructor set and is sealed (ticket #1958, SR-AUD-196)

*2026-08-19.* `System::Threading::ThreadStartException` loses its two message-taking constructors,
gains .NET's reason-taking one, and becomes `final`.

**This is a public source break**, landed under **SA-8** with SA-2's five conditions discharged.
No layout or vtable change.

---

## 1. What was wrong

.NET's type is, in full:

```csharp
public sealed class ThreadStartException : SystemException
{
    internal ThreadStartException()                 : base(SR.Arg_ThreadStartException) { ... }
    internal ThreadStartException(Exception? reason) : base(SR.Arg_ThreadStartException, reason) { ... }
}                                                                 // ThreadStartException.cs:11-24
```

Both constructors pass the **same fixed message** — *"Thread failed to start."* — so .NET has
**no message-taking constructor at all**. This port published three, two of which let a caller
supply any message they liked while the object still claimed `COR_E_THREADSTART`. That is an
exception .NET can never produce.

The port's own doc-comment already said *"Constructors are internal in .NET — only the runtime
creates instances."* The documentation and the code disagreed.

## 2. What changed

| | Was | Is |
|---|---|---|
| `ThreadStartException()` | present | present — message unchanged |
| `ThreadStartException(std::string)` | present | **removed** |
| `ThreadStartException(std::string, exception_ptr)` | present | **removed** |
| `ThreadStartException(std::exception_ptr reason)` | absent | **present** — fixed message, retains the reason |
| the class | not sealed | **`final`**, as .NET's `sealed` |
| `HResult` | `COR_E_THREADSTART` | unchanged |
| layout / vtable | — | unchanged |

**The surviving overload takes the reason alone**, which is why it *replaces* the old
`(message, inner)` pair rather than sitting beside it: the message is not the caller's to choose.

## 3. To migrate

```cpp
ThreadStartException("start failed");            // was
ThreadStartException();                          // now

ThreadStartException("start failed", inner);     // was
ThreadStartException(inner);                     // now
```

If you were relying on a custom message, that message was never one .NET would produce for this
type.

## 4. One difference remains, deliberately, and it is filed rather than guessed

.NET marks both constructors `internal`. **C++ has no equivalent**, and the mechanical
translations are not equivalent to one another:

* `private` with no friend makes the type impossible to instantiate anywhere — arguably a .NET
  user's exact position, but it is then dead code and costs the tests that verify the fixed
  message and HResult;
* `private` plus `friend class Thread` matches #2298's `LocalDataStoreSlot` precedent, but the
  friend would be **dead**: this port's `Thread` never throws it and cannot, because the window
  .NET throws it in does not exist here (.NET throws after the OS thread starts but before user
  code runs; `std::thread` either constructs or throws `std::system_error`).

Choosing between those changes the public surface, so it is **ticket #2390** — recorded as a
general question, since the answer should be the rule for every ported type with `internal`
members, not a one-off.

## 5. Evidence

Five mutations, **all caught**, two at compile time:

| Mutation | Caught by |
|---|---|
| M1 — the reason constructor loses the fixed message | `Fix1958_TheReasonCtorKeepsTheFixedMessage` |
| M2 — the default message is changed | `Fix1958_TheMessageIsFixedAndNotSuppliable` |
| M3 — the class is no longer sealed | `Decl1958_TheTypeIsSealed` (compile time) |
| M4 — a message constructor is reinstated | `Fix1958_TheMessageIsFixedAndNotSuppliable` (compile time) |
| M5 — the reason constructor drops the reason | `Fix1958_TheReasonCtorKeepsTheFixedMessage` |

Negative consumer fixture:
`test/consumer/threading_threadstartexception_shape_negative.cpp`, four sites, all rejected.
Fixture set grows to **41 fixtures / 215 sites**. Site 3 is the **string literal** spelling — the
one most likely to survive a careless migration, because a literal is not a `std::string` and a
reader scanning for `std::string` will miss it. Site 4 is the `final` violation.

Gate: **17,498 run, 17,498 passed, 0 failed, 0 skipped** across 38 executables — `+1` on 17,497:
two message-taking cases were replaced by three (`SharpRuntimeTests_Threading` 521 → 522), and
`SharpRuntimeIntegrationTests` is unchanged because its case was rewritten in place. Module graph
unchanged at 41/93.

## 6. First-party migration, and a counting correction

**Four sites, not two.** An earlier grep over `modules/` and `test/` found only the two in
`Batch8ThreadingTests.cpp` and **missed** `tests/integration/`, which is a separate tree — the
compiler found the other two in `ExceptionHResultPopulationTests.cpp`. Nothing in any
`modules/*/src` constructs this type at all.

## 7. Downstream, measured

`ThreadStartException` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`.
Neither repository was modified, and no downstream ticket is needed.

## 8. Scope

This closes #1958's seventh finding. Only **SR-AUD-209** remains — making `AutoResetEvent` and
`ManualResetEvent` derive from `WaitHandle`, a vtable *and* base-class change that SA-3
explicitly excludes.
