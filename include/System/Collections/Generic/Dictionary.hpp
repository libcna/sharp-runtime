#pragma once

#include <unordered_map>
#include <stdexcept>
#include <string>

namespace System::Collections::Generic
{
    /**
     * @brief A generic collection of key-value pairs.
     *
     * C++ counterpart of the .NET System.Collections.Generic.Dictionary<TKey,TValue> class.
     * Backed by std::unordered_map<TKey, TValue>.
     *
     * @tparam TKey   The type of keys in the dictionary.
     * @tparam TValue The type of values in the dictionary.
     */
    template<typename TKey, typename TValue>
    class Dictionary
    {
    private:
        std::unordered_map<TKey, TValue> map_;

    public:
        Dictionary() = default;

        /// Gets the number of key-value pairs.
        [[nodiscard]] int getCountProperty() const
        {
            return static_cast<int>(map_.size());
        }

        /// Adds a key-value pair. Throws if the key already exists.
        void Add(const TKey& key, const TValue& value)
        {
            if (map_.count(key))
                throw std::invalid_argument("An element with the same key already exists.");
            map_[key] = value;
        }

        /// Removes the entry for the given key. Returns true if found.
        bool Remove(const TKey& key)
        {
            return map_.erase(key) > 0;
        }

        /// Returns true if the dictionary contains the given key.
        [[nodiscard]] bool ContainsKey(const TKey& key) const
        {
            return map_.count(key) > 0;
        }

        /// Tries to get a value; returns false if not found.
        bool TryGetValue(const TKey& key, TValue& outValue) const
        {
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            outValue = it->second;
            return true;
        }

        /// Removes all entries.
        void Clear() { map_.clear(); }

        TValue& operator[](const TKey& key) { return map_[key]; }

        [[nodiscard]] const TValue& operator[](const TKey& key) const
        {
            auto it = map_.find(key);
            if (it == map_.end())
                throw std::out_of_range("Key not found in Dictionary.");
            return it->second;
        }

        auto begin()        { return map_.begin(); }
        auto end()          { return map_.end(); }
        [[nodiscard]] auto begin() const { return map_.cbegin(); }
        [[nodiscard]] auto end()   const { return map_.cend(); }

        /// Returns the underlying std::unordered_map for STL interop.
        [[nodiscard]] const std::unordered_map<TKey, TValue>& ToMap() const { return map_; }
        [[nodiscard]] std::unordered_map<TKey, TValue>& ToMap() { return map_; }
    };
}
