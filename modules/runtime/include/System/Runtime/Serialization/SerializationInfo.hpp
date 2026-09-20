// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Runtime/Serialization/SerializationException.hpp"

namespace System::Runtime::Serialization {

    /**
     * @brief Stores all the data needed to serialize or deserialize an object.
     *
     * C++ counterpart of .NET System.Runtime.Serialization.SerializationInfo: a name-to-value store
     * a type writes its state into and reads its state back out of. Values are held as `std::any`,
     * which is this runtime's representation of a boxed CLR object, so a name is retrieved as the
     * type it was stored as.
     *
     * .NET's own rules are kept, because they are what a ported type's behaviour depends on:
     * - a name may be added only once; a repeat is a SerializationException,
     * - a name that was never added is a SerializationException when read,
     * - a name read as a type it was not stored as is a SerializationException,
     * - names are matched exactly, including case.
     *
     * @note Scope. This is a **value store and nothing more** -- deliberately not .NET's legacy
     *   binary serialization infrastructure. There is no BinaryFormatter, no surrogate selector, no
     *   type resolution by name, no stream format, and no object graph: nothing here writes bytes
     *   anywhere. What it exists for is the one pattern .NET's obsolete-but-documented
     *   `(SerializationInfo, StreamingContext)` constructor/`GetObjectData` pair needs -- a type
     *   writing its own fields out by name and reconstructing itself from them, in process. A
     *   consumer that needs a real serialized stream needs a serializer, which remains out of scope.
     */
    class SerializationInfo {
    public:
        /** @brief Creates an empty SerializationInfo with no type name set. */
        SerializationInfo() = default;

        /**
         * @brief Creates an empty SerializationInfo describing the named type.
         *
         * @param fullTypeName The full name of the type being serialized.
         * @param assemblyName The name of the assembly that type belongs to.
         */
        SerializationInfo(std::string fullTypeName, std::string assemblyName)
            : fullTypeName_(std::move(fullTypeName)), assemblyName_(std::move(assemblyName)) {}

        virtual ~SerializationInfo() = default;

        /**
         * @brief Adds a value to the store under the specified name.
         *
         * C++ counterpart of .NET SerializationInfo.AddValue(string, object).
         *
         * @param name The name to store the value under; must not be empty and must be new.
         * @param value The value to store.
         * @throws SerializationException if @p name is empty or has already been added.
         */
        void AddValue(const std::string& name, std::any value) {
            if (name.empty()) {
                throw SerializationException("A serialized value's name cannot be empty.");
            }
            for (const Entry& entry : entries_) {
                if (entry.name == name) {
                    throw SerializationException(
                        "The value '" + name + "' has already been added to this SerializationInfo.");
                }
            }
            entries_.push_back(Entry{name, std::move(value)});
        }

        /**
         * @brief Adds a value to the store under the specified name.
         *
         * The typed overloads .NET spells out one per primitive. A single template covers them: the
         * stored type is @p TValue, which is the type GetValue<TValue>() must later ask for.
         *
         * @tparam TValue The type of the value being stored.
         * @param name The name to store the value under.
         * @param value The value to store.
         * @throws SerializationException if @p name is empty or has already been added.
         */
        template <typename TValue>
        void AddValue(const std::string& name, TValue value) {
            AddValue(name, std::any(std::move(value)));
        }

        /**
         * @brief Retrieves a value from the store.
         *
         * C++ counterpart of .NET SerializationInfo.GetValue(string, Type), with the type expressed
         * as a template parameter rather than a reflected `Type`.
         *
         * @tparam TValue The type the value was stored as.
         * @param name The name the value was stored under.
         * @return The stored value.
         * @throws SerializationException if @p name was never added, or was stored as another type.
         */
        template <typename TValue>
        [[nodiscard]] TValue GetValue(const std::string& name) const {
            const std::any& value = Find(name);
            const TValue* typed = std::any_cast<TValue>(&value);
            if (typed == nullptr) {
                throw SerializationException(
                    "The value '" + name + "' was not stored as the requested type.");
            }
            return *typed;
        }

        /**
         * @brief Retrieves a value as a boxed object.
         *
         * @param name The name the value was stored under.
         * @return The stored value, boxed.
         * @throws SerializationException if @p name was never added.
         */
        [[nodiscard]] const std::any& GetValue(const std::string& name) const { return Find(name); }

        /** @brief Retrieves a Boolean value. @param name The stored name. @return The value. */
        [[nodiscard]] bool GetBoolean(const std::string& name) const { return GetValue<bool>(name); }

        /** @brief Retrieves an Int32 value. @param name The stored name. @return The value. */
        [[nodiscard]] SharpRuntime::intcs GetInt32(const std::string& name) const {
            return GetValue<SharpRuntime::intcs>(name);
        }

        /** @brief Retrieves an Int64 value. @param name The stored name. @return The value. */
        [[nodiscard]] SharpRuntime::longcs GetInt64(const std::string& name) const {
            return GetValue<SharpRuntime::longcs>(name);
        }

        /** @brief Retrieves a Single value. @param name The stored name. @return The value. */
        [[nodiscard]] SharpRuntime::Single GetSingle(const std::string& name) const {
            return GetValue<SharpRuntime::Single>(name);
        }

        /** @brief Retrieves a Double value. @param name The stored name. @return The value. */
        [[nodiscard]] double GetDouble(const std::string& name) const {
            return GetValue<double>(name);
        }

        /** @brief Retrieves a String value. @param name The stored name. @return The value. */
        [[nodiscard]] std::string GetString(const std::string& name) const {
            return GetValue<std::string>(name);
        }

        /**
         * @brief Returns whether a value has been added under the specified name.
         *
         * Not a .NET member: .NET's only way to ask is to call a getter and catch the exception,
         * which a C++ caller should not have to do to write a tolerant deserializing constructor.
         *
         * @param name The name to look for.
         * @return @c true if a value is stored under @p name.
         */
        [[nodiscard]] bool Contains(const std::string& name) const {
            for (const Entry& entry : entries_) {
                if (entry.name == name) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Gets the number of values that have been added.
         *
         * C++ counterpart of .NET SerializationInfo.MemberCount.
         * @return The number of stored values.
         */
        [[nodiscard]] SharpRuntime::intcs getMemberCountProperty() const {
            return static_cast<SharpRuntime::intcs>(entries_.size());
        }

        /**
         * @brief Gets the full name of the type to serialize.
         *
         * C++ counterpart of .NET SerializationInfo.FullTypeName.
         * @return The stored type name, or an empty string when none was set.
         */
        [[nodiscard]] const std::string& getFullTypeNameProperty() const { return fullTypeName_; }

        /**
         * @brief Sets the full name of the type to serialize.
         * @param value The type name.
         */
        void setFullTypeNameProperty(std::string value) { fullTypeName_ = std::move(value); }

        /**
         * @brief Gets the name of the assembly of the type to serialize.
         *
         * C++ counterpart of .NET SerializationInfo.AssemblyName.
         * @return The stored assembly name, or an empty string when none was set.
         */
        [[nodiscard]] const std::string& getAssemblyNameProperty() const { return assemblyName_; }

        /**
         * @brief Sets the name of the assembly of the type to serialize.
         * @param value The assembly name.
         */
        void setAssemblyNameProperty(std::string value) { assemblyName_ = std::move(value); }

        /**
         * @brief Returns the names of every stored value, in the order they were added.
         *
         * Not a .NET member -- .NET exposes the same information through a
         * SerializationInfoEnumerator, which needs the reflected value types this port has no
         * equivalent for. Order is preserved because a deserializing constructor written against
         * .NET may rely on it.
         *
         * @return The stored names.
         */
        [[nodiscard]] std::vector<std::string> GetNames() const {
            std::vector<std::string> names;
            names.reserve(entries_.size());
            for (const Entry& entry : entries_) {
                names.push_back(entry.name);
            }
            return names;
        }

    private:
        struct Entry {
            std::string name;
            std::any value;
        };

        [[nodiscard]] const std::any& Find(const std::string& name) const {
            for (const Entry& entry : entries_) {
                if (entry.name == name) {
                    return entry.value;
                }
            }
            throw SerializationException(
                "No value named '" + name + "' was added to this SerializationInfo.");
        }

        std::vector<Entry> entries_;
        std::string fullTypeName_;
        std::string assemblyName_;
    };

} // namespace System::Runtime::Serialization
