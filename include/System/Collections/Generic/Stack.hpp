// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stack>
#include <stdexcept>
#include <vector>

namespace System::Collections::Generic {

/**
 * @brief Represents a last-in, first-out (LIFO) collection of objects.
 *
 * C++ counterpart of .NET System.Collections.Generic.Stack<T>.
 * Backed by std::stack<T>; provides O(1) Push, Pop, and Peek.
 *
 * @tparam T The type of elements in the stack.
 */
template<typename T>
class Stack {
    std::stack<T> stack_;

public:
    /** @brief Initializes a new empty Stack<T>. */
    Stack() = default;

    /**
     * @brief Gets the number of elements contained in the Stack.
     *
     * C++ counterpart of .NET Stack<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] int getCountProperty() const { return static_cast<int>(stack_.size()); }

    /**
     * @brief Inserts an object at the top of the Stack (copy).
     *
     * C++ counterpart of .NET Stack<T>.Push(T).
     * @param item The object to push onto the stack.
     */
    void Push(const T& item) { stack_.push(item); }

    /**
     * @brief Inserts an object at the top of the Stack (move).
     *
     * C++ counterpart of .NET Stack<T>.Push(T).
     * @param item The object to push onto the stack (moved).
     */
    void Push(T&& item) { stack_.push(std::move(item)); }

    /**
     * @brief Removes and returns the object at the top of the Stack.
     *
     * C++ counterpart of .NET Stack<T>.Pop().
     * @return The object removed from the top of the Stack.
     * @throws std::runtime_error if the Stack is empty.
     */
    T Pop() {
        if (stack_.empty()) throw std::runtime_error("Stack is empty.");
        T val = std::move(stack_.top());
        stack_.pop();
        return val;
    }

    /**
     * @brief Returns the object at the top of the Stack without removing it.
     *
     * C++ counterpart of .NET Stack<T>.Peek().
     * @return A const reference to the object at the top.
     * @throws std::runtime_error if the Stack is empty.
     */
    [[nodiscard]] const T& Peek() const {
        if (stack_.empty()) throw std::runtime_error("Stack is empty.");
        return stack_.top();
    }

    /**
     * @brief Tries to remove and return the object at the top of the Stack.
     *
     * C++ counterpart of .NET Stack<T>.TryPop(out T).
     * @param result Receives the popped object if successful.
     * @return true if an element was removed; false if the Stack was empty.
     */
    bool TryPop(T& result) {
        if (stack_.empty()) return false;
        result = std::move(stack_.top());
        stack_.pop();
        return true;
    }

    /**
     * @brief Tries to return the object at the top of the Stack without removing it.
     *
     * C++ counterpart of .NET Stack<T>.TryPeek(out T).
     * @param result Receives the top object if successful.
     * @return true if the Stack is non-empty; otherwise false.
     */
    bool TryPeek(T& result) const {
        if (stack_.empty()) return false;
        result = stack_.top();
        return true;
    }

    /**
     * @brief Determines whether the Stack contains the specified element.
     *
     * C++ counterpart of .NET Stack<T>.Contains(T).
     * @param item The object to locate.
     * @return true if the element is found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        std::stack<T> copy = stack_;
        while (!copy.empty()) {
            if (copy.top() == item) return true;
            copy.pop();
        }
        return false;
    }

    /**
     * @brief Removes all objects from the Stack.
     *
     * C++ counterpart of .NET Stack<T>.Clear().
     */
    void Clear() { while (!stack_.empty()) stack_.pop(); }

    /**
     * @brief Copies the Stack elements to a new vector in top-first order.
     *
     * C++ counterpart of .NET Stack<T>.ToArray().
     * @return A std::vector<T> where element 0 is the top of the stack.
     */
    [[nodiscard]] std::vector<T> ToArray() const {
        std::vector<T> result;
        std::stack<T> copy = stack_;
        while (!copy.empty()) {
            result.push_back(copy.top());
            copy.pop();
        }
        return result;
    }
};

} // namespace System::Collections::Generic
