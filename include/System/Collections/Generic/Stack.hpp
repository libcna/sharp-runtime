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
        Stack() = default;

        /**
         * @brief Inserts an object at the top of the Stack.
         */
        void Push(const T& item) { stack_.push(item); }
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

        [[nodiscard]] int getCountProperty() const { return static_cast<int>(stack_.size()); }

        [[nodiscard]] bool Contains(const T& item) const {
            std::stack<T> copy = stack_;
            while (!copy.empty()) {
                if (copy.top() == item) return true;
                copy.pop();
            }
            return false;
        }

        void Clear() { while (!stack_.empty()) stack_.pop(); }

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
