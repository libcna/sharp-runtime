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
     * @tparam T The type of elements in the stack.
     *
     * @note Status: Implemented
     */
    template<typename T>
    class Stack {
    private:
        std::stack<T> stack_;

    public:
        /// Default-constructs an empty Stack.
        Stack() = default;

        /**
         * @brief Inserts an object at the top of the Stack.
         */
        /// Pushes a copy of item onto the top of the Stack.
        void Push(const T& item) { stack_.push(item); }
        /// Pushes item by move onto the top of the Stack.
        void Push(T&& item)      { stack_.push(std::move(item)); }

        /**
         * @brief Removes and returns the object at the top of the Stack.
         */
        T Pop() {
            if (stack_.empty()) throw std::runtime_error("Stack is empty.");
            T val = std::move(stack_.top());
            stack_.pop();
            return val;
        }

        /**
         * @brief Returns the object at the top of the Stack without removing it.
         */
        [[nodiscard]] const T& Peek() const {
            if (stack_.empty()) throw std::runtime_error("Stack is empty.");
            return stack_.top();
        }

        /// Gets the number of elements contained in the Stack.
        [[nodiscard]] int getCountProperty() const { return static_cast<int>(stack_.size()); }

        /// Returns true if the Stack contains the specified element.
        [[nodiscard]] bool Contains(const T& item) const {
            std::stack<T> copy = stack_;
            while (!copy.empty()) {
                if (copy.top() == item) return true;
                copy.pop();
            }
            return false;
        }

        /// Removes all objects from the Stack.
        void Clear() { while (!stack_.empty()) stack_.pop(); }

        /// Copies the Stack elements to a new vector (top-first order).
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
