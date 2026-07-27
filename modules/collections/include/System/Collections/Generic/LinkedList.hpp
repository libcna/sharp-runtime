// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/IEnumerator.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/NullReferenceException.hpp"

namespace System::Collections::Generic {

using SharpRuntime::intcs;

template<typename T> class LinkedList;
template<typename T> class LinkedListNode;

namespace detail {

/**
 * @brief Independently allocated storage for one LinkedList<T> node.
 *
 * C++ counterpart of the private state of .NET's LinkedListNode<T>: the value,
 * the owning list (null once the node is detached), and the neighbour links.
 * Forward links own the chain and backward links borrow it, so an attached list
 * has no reference cycle and a node outlives its owner whenever a public
 * LinkedListNode<T> handle still refers to it.
 *
 * @tparam T The type of the value held by the node.
 */
template<typename T>
struct LinkedListNodeData {
    /** @brief The value held by the node; retained after the node is detached. */
    T item;
    /** @brief The owning list, or nullptr when the node is detached. */
    LinkedList<T>* owner = nullptr;
    /** @brief The next node in the owning list; owns its successor. */
    std::shared_ptr<LinkedListNodeData> next;
    /** @brief The previous node in the owning list; a non-owning back link. */
    std::weak_ptr<LinkedListNodeData> prev;

    /** @brief Initializes node storage holding a copy of @p value. */
    explicit LinkedListNodeData(const T& value) : item(value) {}
    /** @brief Initializes node storage taking ownership of @p value. */
    explicit LinkedListNodeData(T&& value) : item(std::move(value)) {}
};

/**
 * @brief Bidirectional STL iterator over the values of a LinkedList<T>.
 *
 * @note Adaptation: this replaces the former `std::list<T>::iterator` returned by
 * `LinkedList<T>::begin()`/`end()`.  It keeps the same range-for, `std::ranges`, and
 * algorithm compatibility and the same invalidation rules (removing a node invalidates
 * only iterators positioned on that node), but it is not a lifetime-safe handle --
 * `LinkedListNode<T>` is.
 *
 * @tparam T       The element type of the owning list.
 * @tparam IsConst Whether the iterator exposes const access.
 */
template<typename T, bool IsConst>
class LinkedListIterator {
    using data_t = LinkedListNodeData<T>;

    data_t* node_ = nullptr;
    const LinkedList<T>* list_ = nullptr;

    LinkedListIterator(data_t* node, const LinkedList<T>* list) : node_(node), list_(list) {}

    friend class System::Collections::Generic::LinkedList<T>;
    template<typename, bool> friend class LinkedListIterator;
public:
    /** @brief Iterator category required by the standard bidirectional iterator concept. */
    using iterator_category = std::bidirectional_iterator_tag;
    /** @brief Iterator concept required by C++20 ranges. */
    using iterator_concept = std::bidirectional_iterator_tag;
    /** @brief The element type traversed by this iterator. */
    using value_type = T;
    /** @brief The signed distance type between two iterators. */
    using difference_type = std::ptrdiff_t;
    /** @brief Pointer to the traversed element. */
    using pointer = std::conditional_t<IsConst, const T*, T*>;
    /** @brief Reference to the traversed element. */
    using reference = std::conditional_t<IsConst, const T&, T&>;

    /** @brief Default-constructs a singular iterator. */
    LinkedListIterator() = default;

    /** @brief Converts a mutable iterator into a const iterator. */
    template<bool OtherIsConst, typename = std::enable_if_t<IsConst && !OtherIsConst>>
    LinkedListIterator(const LinkedListIterator<T, OtherIsConst>& other)
        : node_(other.node_), list_(other.list_) {}

    /** @brief Returns the element at the current position. */
    reference operator*() const { return node_->item; }
    /** @brief Returns a pointer to the element at the current position. */
    pointer operator->() const { return &node_->item; }

    /** @brief Advances to the next element. */
    LinkedListIterator& operator++() { node_ = node_->next.get(); return *this; }
    /** @brief Advances to the next element and returns the previous position. */
    LinkedListIterator operator++(int) { LinkedListIterator copy = *this; ++(*this); return copy; }
    /** @brief Steps back one element; stepping back from end() reaches the last element. */
    LinkedListIterator& operator--() {
        node_ = node_ != nullptr ? node_->prev.lock().get() : list_->tail_.lock().get();
        return *this;
    }
    /** @brief Steps back one element and returns the previous position. */
    LinkedListIterator operator--(int) { LinkedListIterator copy = *this; --(*this); return copy; }

    /** @brief Returns true if both iterators are at the same position. */
    bool operator==(const LinkedListIterator& other) const { return node_ == other.node_; }
    /** @brief Returns true if the iterators are at different positions. */
    bool operator!=(const LinkedListIterator& other) const { return node_ != other.node_; }
};

} // namespace detail

/**
 * @brief Represents a node in a LinkedList<T>.
 *
 * C++ counterpart of .NET System.Collections.Generic.LinkedListNode<T>.  The
 * node itself is an independently allocated, reference-counted object and this
 * type is a copyable handle to it, mirroring the way .NET hands out node
 * references.  A handle is in exactly one of three states:
 *
 * - **null handle** — default-constructed, or returned by an operation with no
 *   result such as `Find` on a missing value.  `operator bool` is false and
 *   comparison with `nullptr` is true.
 * - **detached node** — a real node with no owning list, either constructed
 *   directly from a value or left behind by `Remove`, `Clear`, or destruction
 *   of the owning list.  It keeps its value, reports no owner, has null
 *   `Next`/`Previous`, and can be attached to a list again.
 * - **attached node** — a node owned by a list, supporting full navigation.
 *
 * Copying a handle shares one node object, so every copy observes the same
 * state.  Retaining a handle across removal or owner destruction is safe: the
 * node stays alive for as long as any handle refers to it.
 *
 * @note Adaptation: `operator bool` reports whether the handle refers to a node
 * at all — the C++ equivalent of a non-null `LinkedListNode<T>?` in .NET — so a
 * detached node is truthy.  Use `getListProperty()` to test list membership.
 *
 * @tparam T The type of the value held by the node.
 */
template<typename T>
class LinkedListNode {
    using data_t = detail::LinkedListNodeData<T>;
    std::shared_ptr<data_t> data_;

    explicit LinkedListNode(std::shared_ptr<data_t> data) : data_(std::move(data)) {}

    /** Requires the handle to refer to a node before its value is accessed. */
    const data_t& requireNode() const {
        if (!data_) throw System::NullReferenceException();
        return *data_;
    }
    data_t& requireNode() {
        if (!data_) throw System::NullReferenceException();
        return *data_;
    }

    friend class LinkedList<T>;
public:
    /** @brief Default-constructs a null handle that refers to no node. */
    LinkedListNode() = default;

    /**
     * @brief Creates a new detached node holding @p value.
     *
     * C++ counterpart of .NET `new LinkedListNode<T>(value)`.  The node belongs
     * to no list until it is passed to `LinkedList<T>::AddFirst`, `AddLast`,
     * `AddBefore`, or `AddAfter`.
     *
     * @note Adaptation: the constructor is `explicit` so that value and
     * existing-node insertion overloads stay unambiguous in C++.
     *
     * @param value The value the new node holds.
     */
    explicit LinkedListNode(const T& value)
        : data_(std::make_shared<data_t>(value)) {}

    /**
     * @brief Creates a new detached node taking ownership of @p value.
     *
     * @param value The value the new node holds.
     */
    explicit LinkedListNode(T&& value)
        : data_(std::make_shared<data_t>(std::move(value))) {}

    /**
     * @brief Returns true if this handle refers to a node; false if it is null.
     *
     * C++ counterpart of checking whether a LinkedListNode<T> reference is non-null in .NET.
     * A detached node is a real node, so it is true; use getListProperty() to test membership.
     */
    explicit operator bool() const { return static_cast<bool>(data_); }

    /**
     * @brief Gets the value contained in the node.
     *
     * C++ counterpart of .NET LinkedListNode<T>.Value getter.  A detached node
     * still holds its value.
     * @return A reference to the value stored in the node.
     * @throws System::NullReferenceException if this handle is null.
     */
    T& getValueProperty() { return requireNode().item; }

    /**
     * @brief Gets the value contained in the node (const overload).
     *
     * C++ counterpart of .NET LinkedListNode<T>.Value getter.
     * @return A const reference to the value stored in the node.
     * @throws System::NullReferenceException if this handle is null.
     */
    const T& getValueProperty() const { return requireNode().item; }

    /**
     * @brief Sets the value contained in the node.
     *
     * C++ counterpart of .NET LinkedListNode<T>.Value setter.  A detached node
     * accepts a new value just as an attached one does.
     * @param value The value to store.
     * @throws System::NullReferenceException if this handle is null.
     */
    void setValueProperty(const T& value) { requireNode().item = value; }

    /**
     * @brief Gets the list this node belongs to, or nullptr if it is detached.
     *
     * C++ counterpart of .NET LinkedListNode<T>.List.
     * @return The owning list, or nullptr for a null handle or a detached node.
     */
    [[nodiscard]] LinkedList<T>* getListProperty() const {
        return data_ ? data_->owner : nullptr;
    }

    /**
     * @brief Gets the next node in the linked list, or a null node if there is none.
     *
     * C++ counterpart of .NET LinkedListNode<T>.Next.  A null handle, a detached
     * node, and the last node of a list all return a null node.
     * @return The next node, or a null node if none exists.
     */
    [[nodiscard]] LinkedListNode<T> getNextProperty() const {
        if (!data_) return {};
        return LinkedListNode<T>(data_->next);
    }

    /**
     * @brief Gets the previous node in the linked list, or a null node if there is none.
     *
     * C++ counterpart of .NET LinkedListNode<T>.Previous.  A null handle, a
     * detached node, and the first node of a list all return a null node.
     * @return The previous node, or a null node if none exists.
     */
    [[nodiscard]] LinkedListNode<T> getPreviousProperty() const {
        if (!data_) return {};
        return LinkedListNode<T>(data_->prev.lock());
    }

    /**
     * @brief Implicit conversion to T for backward-compatible use in value contexts.
     * @throws System::NullReferenceException if this handle is null.
     */
    operator T() const { return requireNode().item; }

    /** @brief Returns true if this handle refers to no node. */
    bool operator==(std::nullptr_t) const { return !data_; }
    /** @brief Returns true if this handle refers to a node. */
    bool operator!=(std::nullptr_t) const { return static_cast<bool>(data_); }
    /** @brief Returns true if the node's value equals @p other; false for a null handle. */
    bool operator==(const T& other) const { return data_ != nullptr && data_->item == other; }
    /** @brief Returns true if the node's value differs from @p other. */
    bool operator!=(const T& other) const { return !(*this == other); }
    /**
     * @brief Returns true if both handles refer to the same node.
     *
     * C++ counterpart of .NET LinkedListNode<T> reference equality; two null
     * handles are equal.
     */
    bool operator==(const LinkedListNode<T>& other) const { return data_ == other.data_; }
    /** @brief Returns true if the handles refer to different nodes. */
    bool operator!=(const LinkedListNode<T>& other) const { return data_ != other.data_; }
};

/**
 * @brief Represents a doubly linked list.
 *
 * C++ counterpart of .NET System.Collections.Generic.LinkedList<T>.  The list
 * owns a chain of independently allocated nodes, so insertion and removal at a
 * known node are O(1) and a `LinkedListNode<T>` handle remains valid even after
 * the node leaves the list.
 *
 * @note GetEnumerator()'s Enumerator detects structural modification (AddFirst/AddLast/
 * AddBefore/AddAfter/Remove/RemoveFirst/RemoveLast/Clear) during iteration via a version
 * counter, matching .NET's InvalidOperationException fail-fast contract (see List<T>/ArrayList's
 * Enumerator in this codebase for the same established pattern), and rejects Current outside a
 * valid enumeration position. Directly using begin()/end() STL iterators still follows plain
 * std::list-style invalidation rules, not .NET's version-checked contract (erasing a node only
 * invalidates that node's own iterator; other iterators remain valid) -- only the
 * GetEnumerator()-returned Enumerator is fail-fast, and only LinkedListNode<T> keeps its node
 * alive after removal or after the list itself is destroyed.
 *
 * @note Lifetime contract: removing a node, clearing the list, or destroying the list detaches
 * the affected nodes.  A detached node keeps its value, reports no owning list, and returns null
 * Next/Previous; it can be inserted into a list again through the existing-node AddFirst/AddLast/
 * AddBefore/AddAfter overloads.  See `docs/LinkedListNodeLifetime.md` for the full design record.
 *
 * @note Adaptation: copying a LinkedList<T> deep-copies its nodes, so handles into the source
 * still refer to the source.  Moving a LinkedList<T> transfers the chain and repoints every
 * node's owner at the destination, which is O(n) rather than the O(1) move of a std::list.
 *
 * @tparam T The type of elements in the linked list.
 */
template<typename T>
class LinkedList {
    using data_t = detail::LinkedListNodeData<T>;
    using node_ptr = std::shared_ptr<data_t>;

    node_ptr head_;
    std::weak_ptr<data_t> tail_;
    intcs count_ = 0;
    intcs version_ = 0;

    template<typename, bool> friend class detail::LinkedListIterator;

    /** Detaches every node currently owned by this list, retaining each value. */
    void detachAll() noexcept {
        node_ptr current = std::move(head_);
        head_.reset();
        tail_.reset();
        count_ = 0;
        while (current) {
            node_ptr next = std::move(current->next);
            current->owner = nullptr;
            current->next.reset();
            current->prev.reset();
            // Releasing `current` here cannot recurse: its forward link is already cleared.
            current = std::move(next);
        }
    }

    // The insert/unlink helpers take their node arguments *by value* on purpose: callers
    // pass members such as `head_`, and these helpers reassign `head_` while still using
    // the argument. A reference parameter would silently alias the reassigned member.

    /** Appends @p node, which must be detached and non-null, to the end of this list. */
    void insertLast(node_ptr node) {
        node_ptr last = tail_.lock();
        if (!last) {
            head_ = node;
        } else {
            node->prev = last;
            last->next = node;
        }
        tail_ = node;
        node->owner = this;
        ++count_;
        ++version_;
    }

    /** Inserts @p node, which must be detached and non-null, before @p position. */
    void insertBefore(node_ptr position, node_ptr node) {
        node_ptr previous = position->prev.lock();
        node->next = position;
        node->prev = previous;
        position->prev = node;
        if (previous) {
            previous->next = node;
        } else {
            head_ = node;
        }
        node->owner = this;
        ++count_;
        ++version_;
    }

    /** Inserts @p node, which must be detached and non-null, after @p position. */
    void insertAfter(node_ptr position, node_ptr node) {
        if (position->next) {
            insertBefore(position->next, std::move(node));
        } else {
            insertLast(std::move(node));
        }
    }

    /** Unlinks @p node from this list and detaches it, retaining its value. */
    void unlink(node_ptr node) {
        node_ptr previous = node->prev.lock();
        node_ptr next = node->next;
        if (previous) {
            previous->next = next;
        } else {
            head_ = next;
        }
        if (next) {
            next->prev = previous;
        } else {
            tail_ = previous;
        }
        node->owner = nullptr;
        node->next.reset();
        node->prev.reset();
        --count_;
        ++version_;
    }

    /** Copies every value of @p other into freshly allocated nodes owned by this list. */
    void copyNodesFrom(const LinkedList<T>& other) {
        for (node_ptr current = other.head_; current; current = current->next) {
            insertLast(std::make_shared<data_t>(current->item));
        }
    }

    /** Takes over @p other's chain and repoints every node's owner at this list. */
    void adoptNodesFrom(LinkedList<T>& other) noexcept {
        head_ = std::move(other.head_);
        tail_ = other.tail_;
        count_ = other.count_;
        for (data_t* current = head_.get(); current != nullptr; current = current->next.get()) {
            current->owner = this;
        }
        other.head_.reset();
        other.tail_.reset();
        other.count_ = 0;
        ++other.version_;
    }

    class Enumerator : public IEnumerator<T> {
        const LinkedList<T>* list_;
        intcs version_;
        data_t* cur_ = nullptr;
        bool started_ = false;
        System::Collections::detail::EnumeratorState state_;
    public:
        explicit Enumerator(const LinkedList<T>* list)
            : list_(list), version_(list->version_), cur_(list->head_.get()) {}
        bool MoveNext() override {
            System::Collections::detail::requireUnmodified(version_ == list_->version_);
            if (state_.isAfterLast()) return false;

            if (!started_) started_ = true;
            else if (cur_ != nullptr) cur_ = cur_->next.get();

            if (cur_ != nullptr) {
                state_.setCurrent();
                return true;
            }
            state_.setAfterLast();
            return false;
        }
        void Reset() override {
            System::Collections::detail::requireUnmodified(version_ == list_->version_);
            cur_ = list_->head_.get();
            started_ = false;
            state_.Reset();
        }
        const T& Current() const override {
            state_.requireCurrent();
            return cur_->item;
        }
    };

    /** Validates that @p node is non-null and belongs to this list. C++ counterpart of .NET LinkedList<T>.ValidateNode. */
    void validateNode(const LinkedListNode<T>& node) const {
        if (!node) throw System::ArgumentNullException("node");
        if (node.data_->owner != this)
            throw System::InvalidOperationException("The LinkedList node does not belong to current LinkedList.");
    }

    /** Validates that @p node is non-null and belongs to no list. C++ counterpart of .NET LinkedList<T>.ValidateNewNode. */
    static void validateNewNode(const LinkedListNode<T>& node) {
        if (!node) throw System::ArgumentNullException("node");
        if (node.data_->owner != nullptr)
            throw System::InvalidOperationException("The LinkedList node already belongs to a LinkedList.");
    }

public:
    /** @brief Initializes a new empty LinkedList<T>. */
    LinkedList() = default;

    /** @brief Detaches every node this list still owns, then releases the list. */
    ~LinkedList() { detachAll(); }

    /**
     * @brief Initializes a list holding an independent copy of every value in @p other.
     *
     * Node handles into @p other keep referring to @p other's nodes.
     */
    LinkedList(const LinkedList<T>& other) { copyNodesFrom(other); }

    /** @brief Initializes a list that takes over @p other's nodes, leaving it empty. */
    LinkedList(LinkedList<T>&& other) noexcept { adoptNodesFrom(other); }

    /**
     * @brief Replaces this list's contents with an independent copy of @p other.
     *
     * Nodes previously owned by this list are detached and keep their values.
     */
    LinkedList<T>& operator=(const LinkedList<T>& other) {
        if (this != &other) {
            detachAll();
            ++version_;
            copyNodesFrom(other);
        }
        return *this;
    }

    /**
     * @brief Replaces this list's contents with @p other's nodes, leaving it empty.
     *
     * Nodes previously owned by this list are detached and keep their values;
     * handles into @p other now report this list as their owner.
     */
    LinkedList<T>& operator=(LinkedList<T>&& other) noexcept {
        if (this != &other) {
            detachAll();
            ++version_;
            adoptNodesFrom(other);
        }
        return *this;
    }

    /**
     * @brief Gets the number of nodes in the linked list.
     *
     * C++ counterpart of .NET LinkedList<T>.Count.
     * @return The number of nodes.
     */
    [[nodiscard]] intcs getCountProperty() const { return count_; }

    /**
     * @brief Gets the first node of the linked list, or a null node if the list is empty.
     *
     * C++ counterpart of .NET LinkedList<T>.First.
     * @return The first node, or a null node.
     */
    [[nodiscard]] LinkedListNode<T> getFirstProperty() { return LinkedListNode<T>(head_); }

    /**
     * @brief Gets the first node of the linked list (const overload), or null if empty.
     *
     * @note Adaptation: like the non-const overload, this hands out a mutable node handle.
     * @return The first node, or a null node.
     */
    [[nodiscard]] LinkedListNode<T> getFirstProperty() const { return LinkedListNode<T>(head_); }

    /**
     * @brief Gets the last node of the linked list, or a null node if the list is empty.
     *
     * C++ counterpart of .NET LinkedList<T>.Last.
     * @return The last node, or a null node.
     */
    [[nodiscard]] LinkedListNode<T> getLastProperty() { return LinkedListNode<T>(tail_.lock()); }

    /**
     * @brief Gets the last node of the linked list (const overload), or null if empty.
     *
     * @note Adaptation: like the non-const overload, this hands out a mutable node handle.
     * @return The last node, or a null node.
     */
    [[nodiscard]] LinkedListNode<T> getLastProperty() const { return LinkedListNode<T>(tail_.lock()); }

    /**
     * @brief Adds a new node containing @p value at the start of the list.
     *
     * C++ counterpart of .NET LinkedList<T>.AddFirst(T).
     * @param value The value to add.
     * @return The new node.
     */
    LinkedListNode<T> AddFirst(const T& value) {
        node_ptr node = std::make_shared<data_t>(value);
        if (head_) insertBefore(head_, node); else insertLast(node);
        return LinkedListNode<T>(node);
    }

    /**
     * @brief Adds the existing detached @p node at the start of the list.
     *
     * C++ counterpart of .NET LinkedList<T>.AddFirst(LinkedListNode<T>).
     * @param node The detached node to attach.
     * @throws System::ArgumentNullException if @p node is a null handle.
     * @throws System::InvalidOperationException if @p node already belongs to a list.
     */
    void AddFirst(LinkedListNode<T> node) {
        validateNewNode(node);
        if (head_) insertBefore(head_, node.data_); else insertLast(node.data_);
    }

    /**
     * @brief Adds a new node containing @p value at the end of the list.
     *
     * C++ counterpart of .NET LinkedList<T>.AddLast(T).
     * @param value The value to add.
     * @return The new node.
     */
    LinkedListNode<T> AddLast(const T& value) {
        node_ptr node = std::make_shared<data_t>(value);
        insertLast(node);
        return LinkedListNode<T>(node);
    }

    /**
     * @brief Adds the existing detached @p node at the end of the list.
     *
     * C++ counterpart of .NET LinkedList<T>.AddLast(LinkedListNode<T>).
     * @param node The detached node to attach.
     * @throws System::ArgumentNullException if @p node is a null handle.
     * @throws System::InvalidOperationException if @p node already belongs to a list.
     */
    void AddLast(LinkedListNode<T> node) {
        validateNewNode(node);
        insertLast(node.data_);
    }

    /**
     * @brief Adds a new node containing @p value immediately before @p node.
     *
     * C++ counterpart of .NET LinkedList<T>.AddBefore(LinkedListNode<T>, T).
     * @param node  The node before which to insert.
     * @param value The value to add.
     * @return The new node.
     * @throws System::ArgumentNullException if @p node is a null handle.
     * @throws System::InvalidOperationException if @p node does not belong to this list.
     */
    LinkedListNode<T> AddBefore(LinkedListNode<T> node, const T& value) {
        validateNode(node);
        node_ptr inserted = std::make_shared<data_t>(value);
        insertBefore(node.data_, inserted);
        return LinkedListNode<T>(inserted);
    }

    /**
     * @brief Adds the existing detached @p newNode immediately before @p node.
     *
     * C++ counterpart of .NET LinkedList<T>.AddBefore(LinkedListNode<T>, LinkedListNode<T>).
     * @param node    The node before which to insert.
     * @param newNode The detached node to attach.
     * @throws System::ArgumentNullException if either handle is null.
     * @throws System::InvalidOperationException if @p node does not belong to this list,
     *         or if @p newNode already belongs to a list.
     */
    void AddBefore(LinkedListNode<T> node, LinkedListNode<T> newNode) {
        validateNode(node);
        validateNewNode(newNode);
        insertBefore(node.data_, newNode.data_);
    }

    /**
     * @brief Adds a new node containing @p value immediately after @p node.
     *
     * C++ counterpart of .NET LinkedList<T>.AddAfter(LinkedListNode<T>, T).
     * @param node  The node after which to insert.
     * @param value The value to add.
     * @return The new node.
     * @throws System::ArgumentNullException if @p node is a null handle.
     * @throws System::InvalidOperationException if @p node does not belong to this list.
     */
    LinkedListNode<T> AddAfter(LinkedListNode<T> node, const T& value) {
        validateNode(node);
        node_ptr inserted = std::make_shared<data_t>(value);
        insertAfter(node.data_, inserted);
        return LinkedListNode<T>(inserted);
    }

    /**
     * @brief Adds the existing detached @p newNode immediately after @p node.
     *
     * C++ counterpart of .NET LinkedList<T>.AddAfter(LinkedListNode<T>, LinkedListNode<T>).
     * @param node    The node after which to insert.
     * @param newNode The detached node to attach.
     * @throws System::ArgumentNullException if either handle is null.
     * @throws System::InvalidOperationException if @p node does not belong to this list,
     *         or if @p newNode already belongs to a list.
     */
    void AddAfter(LinkedListNode<T> node, LinkedListNode<T> newNode) {
        validateNode(node);
        validateNewNode(newNode);
        insertAfter(node.data_, newNode.data_);
    }

    /**
     * @brief Removes the node at the start of the list.
     *
     * C++ counterpart of .NET LinkedList<T>.RemoveFirst().  The removed node is
     * detached and keeps its value.
     * @throws System::InvalidOperationException if the list is empty.
     */
    void RemoveFirst() {
        if (!head_) throw System::InvalidOperationException("The LinkedList is empty.");
        unlink(head_);
    }

    /**
     * @brief Removes the node at the end of the list.
     *
     * C++ counterpart of .NET LinkedList<T>.RemoveLast().  The removed node is
     * detached and keeps its value.
     * @throws System::InvalidOperationException if the list is empty.
     */
    void RemoveLast() {
        node_ptr last = tail_.lock();
        if (!last) throw System::InvalidOperationException("The LinkedList is empty.");
        unlink(last);
    }

    /**
     * @brief Removes the first occurrence of @p value from the list.
     *
     * C++ counterpart of .NET LinkedList<T>.Remove(T).  The removed node is
     * detached and keeps its value.
     * @param value The value to remove.
     * @return true if an element was found and removed; otherwise false.
     */
    bool Remove(const T& value) {
        for (node_ptr current = head_; current; current = current->next) {
            if (current->item == value) {
                unlink(current);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Removes the specified node from the list.
     *
     * C++ counterpart of .NET LinkedList<T>.Remove(LinkedListNode<T>).  The
     * removed node is detached and keeps its value, so every copied handle to it
     * stays usable.
     * @param node The node to remove.
     * @throws System::ArgumentNullException if @p node is a null handle.
     * @throws System::InvalidOperationException if @p node does not belong to this list.
     */
    void Remove(LinkedListNode<T> node) {
        validateNode(node);
        unlink(node.data_);
    }

    /**
     * @brief Removes all nodes from the list.
     *
     * C++ counterpart of .NET LinkedList<T>.Clear().  Every removed node is
     * detached and keeps its value.
     */
    void Clear() { detachAll(); ++version_; }

    /**
     * @brief Determines whether @p value is in the list.
     *
     * C++ counterpart of .NET LinkedList<T>.Contains(T).
     * @param value The value to locate.
     * @return true if the value is found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& value) const {
        for (data_t* current = head_.get(); current != nullptr; current = current->next.get()) {
            if (current->item == value) return true;
        }
        return false;
    }

    /**
     * @brief Finds the first node containing @p value.
     *
     * C++ counterpart of .NET LinkedList<T>.Find(T).
     * @param value The value to search for.
     * @return The first matching node, or a null node if not found.
     */
    [[nodiscard]] LinkedListNode<T> Find(const T& value) {
        for (node_ptr current = head_; current; current = current->next) {
            if (current->item == value) return LinkedListNode<T>(current);
        }
        return {};
    }

    /**
     * @brief Finds the last node containing @p value.
     *
     * C++ counterpart of .NET LinkedList<T>.FindLast(T).
     * @param value The value to search for.
     * @return The last matching node, or a null node if not found.
     */
    [[nodiscard]] LinkedListNode<T> FindLast(const T& value) {
        for (node_ptr current = tail_.lock(); current; current = current->prev.lock()) {
            if (current->item == value) return LinkedListNode<T>(current);
        }
        return {};
    }

    /**
     * @brief Copies the list elements to a vector starting at @p index.
     *
     * C++ counterpart of .NET LinkedList<T>.CopyTo(T[], int).
     * @param dest  The destination vector (must have sufficient capacity).
     * @param index The zero-based index in @p dest at which copying begins.
     * @throws System::ArgumentOutOfRangeException if @p index is negative, or if @p index is
     *         greater than the length of @p dest (real .NET's LinkedList<T>.CopyTo makes this a
     *         *separate* check from the insufficient-space case below, throwing
     *         ArgumentOutOfRangeException rather than ArgumentException for it -- e.g. an
     *         out-of-bounds index against an EMPTY list still throws the range exception, not
     *         the space one, since there is no "insufficient space" to speak of when there is
     *         nothing to copy).
     * @throws System::ArgumentException if @p dest does not have enough remaining room (from
     *         @p index onward) for every element of this list.
     */
    void CopyTo(std::vector<T>& dest, intcs index) const {
        if (index < 0)
            throw System::ArgumentOutOfRangeException("index", "Non-negative number required.");
        if (static_cast<std::size_t>(index) > dest.size())
            throw System::ArgumentOutOfRangeException("index", "Larger than collection size.");
        if (dest.size() - static_cast<std::size_t>(index) < static_cast<std::size_t>(count_))
            throw System::ArgumentException("Destination array is not long enough to copy all the items in the collection.");
        for (data_t* current = head_.get(); current != nullptr; current = current->next.get()) {
            dest[static_cast<std::size_t>(index++)] = current->item;
        }
    }

    /**
     * @brief Returns an enumerator that iterates through the list.
     *
     * C++ counterpart of .NET LinkedList<T>.GetEnumerator().
     * @return A heap-allocated IEnumerator<T>; caller takes ownership.
     */
    [[nodiscard]] IEnumerator<T>* GetEnumerator() {
        return new Enumerator(this);
    }

    /** @brief The mutable STL iterator type of this list. */
    using iterator = detail::LinkedListIterator<T, false>;
    /** @brief The const STL iterator type of this list. */
    using const_iterator = detail::LinkedListIterator<T, true>;

    /** @brief Returns an iterator to the first node (STL interop). */
    iterator begin() { return iterator(head_.get(), this); }
    /** @brief Returns an iterator past the last node (STL interop). */
    iterator end() { return iterator(nullptr, this); }
    /** @brief Returns a const iterator to the first node (STL interop). */
    const_iterator begin() const { return const_iterator(head_.get(), this); }
    /** @brief Returns a const iterator past the last node (STL interop). */
    const_iterator end()   const { return const_iterator(nullptr, this); }
    /** @brief Returns a const iterator to the first node (STL interop). */
    const_iterator cbegin() const { return begin(); }
    /** @brief Returns a const iterator past the last node (STL interop). */
    const_iterator cend()   const { return end(); }
};

} // namespace System::Collections::Generic
