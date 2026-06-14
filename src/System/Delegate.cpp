// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Delegate.hpp"
#include "System/NotImplementedException.hpp"

namespace System {

void Delegate::Invoke() const {
    if (!invocationList_.empty()) {
        for (const auto& d : invocationList_) d->Invoke();
    } else if (invoke_) {
        invoke_();
    }
}

bool Delegate::getHasSingleTargetProperty() const {
    return invocationList_.size() <= 1;
}

std::vector<std::shared_ptr<Delegate>> Delegate::GetInvocationList() const {
    if (!invocationList_.empty()) return invocationList_;
    return { const_cast<Delegate*>(this)->shared_from_this() };
}

std::shared_ptr<Delegate> Delegate::Clone() const {
    return std::make_shared<Delegate>(*this);
}

std::any Delegate::DynamicInvoke(const std::vector<std::any>&) {
    throw NotImplementedException("DynamicInvoke is not supported in sharp-runtime");
}

std::shared_ptr<Delegate> Delegate::Combine(
        std::shared_ptr<Delegate> a, std::shared_ptr<Delegate> b) {
    if (!a) return b;
    if (!b) return a;

    std::vector<std::shared_ptr<Delegate>> combined;

    const auto& la = a->invocationList_;
    if (la.empty()) combined.push_back(a);
    else combined.insert(combined.end(), la.begin(), la.end());

    const auto& lb = b->invocationList_;
    if (lb.empty()) combined.push_back(b);
    else combined.insert(combined.end(), lb.begin(), lb.end());

    return std::shared_ptr<Delegate>(new Delegate(MulticastTag{}, std::move(combined)));
}

std::shared_ptr<Delegate> Delegate::Combine(
        const std::vector<std::shared_ptr<Delegate>>& delegates) {
    std::shared_ptr<Delegate> result;
    for (const auto& d : delegates) result = Combine(result, d);
    return result;
}

std::shared_ptr<Delegate> Delegate::Remove(
        std::shared_ptr<Delegate> source, std::shared_ptr<Delegate> value) {
    if (!source) return nullptr;
    if (!value)  return source;

    const auto& sl = source->invocationList_;
    if (sl.empty()) {
        // Single-target delegate
        if (source.get() == value.get() || source->Equals(*value)) return nullptr;
        return source;
    }

    auto list = sl;
    for (SharpRuntime::intcs i = static_cast<SharpRuntime::intcs>(list.size()) - 1; i >= 0; --i) {
        if (list[i].get() == value.get() || list[i]->Equals(*value)) {
            list.erase(list.begin() + i);
            if (list.empty()) return nullptr;
            if (list.size() == 1) return list[0];
            return std::shared_ptr<Delegate>(new Delegate(MulticastTag{}, std::move(list)));
        }
    }
    return source; // Not found — return unchanged
}

std::shared_ptr<Delegate> Delegate::RemoveAll(
        std::shared_ptr<Delegate> source, std::shared_ptr<Delegate> value) {
    std::shared_ptr<Delegate> result = source;
    while (result) {
        auto next = Remove(result, value);
        if (next.get() == result.get()) break; // Nothing removed
        result = next;
    }
    return result;
}

} // namespace System
