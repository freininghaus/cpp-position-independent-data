#pragma once

#include <vector>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <stdexcept>
#include <limits>
#include <algorithm>

namespace pid {
    template <typename T>
    struct builder_offset;

    template <typename T, typename offset_type>
    struct ptr
    {
        // using ItemType = T;
        offset_type offset;

        ptr() : offset{0} {}

        ptr(const struct ptr &) = delete;

        ptr(struct ptr &&) = delete;

        ptr(builder_offset<T> p)
        {
            // required for initialization of std::optional<ptr<T>> with '='
            *this = p;
        }

        auto & operator=(builder_offset<T> p)
        {
            p.assign_to(*this);
            return *this;
        }

        explicit operator bool() const
        {
            return offset != 0;
        }

        T & operator*()
        {
            return *operator->();
        }

        const T & operator*() const
        {
            return *operator->();
        }

        T * operator->()
        {
            return get<T *>();
        }

        const T * operator->() const
        {
            return get<const T *>();
        }

        template <typename Pointer>
        const Pointer get() const
        {
            return reinterpret_cast<const Pointer>(reinterpret_cast<const char *>(this) + offset);
        }

        template <typename Pointer>
        Pointer get()
        {
            return reinterpret_cast<Pointer>(reinterpret_cast<char *>(this) + offset);
        }
    };

    template <typename T>
    using ptr8 = ptr<T, std::int8_t>;

    template <typename T>
    using ptr16 = ptr<T, std::int16_t>;

    template <typename T>
    using ptr32 = ptr<T, std::int32_t>;

    template <typename T>
    using ptr64 = ptr<T, std::int64_t>;

    template <typename SizeType>
    struct generic_string_data
    {
        SizeType string_length;
        char data[];

        generic_string_data(const generic_string_data &) = delete;

        generic_string_data(generic_string_data &&) = delete;

        const char * begin() const
        {
            return data;
        }

        const char * end() const
        {
            return data + string_length;
        }

        operator std::string_view() const
        {
            return {begin(), end()};
        }

        template <typename String>
        bool operator==(const String & other) const
        {
            return std::string_view{*this} == other;
        }

        template <typename String>
        bool operator<(const String & other) const
        {
            return std::string_view{*this} < other;
        }

        template <typename String>
        friend bool operator<(const String & other, const generic_string_data<SizeType> & self)
        {
            return other < std::string_view{self};
        }
    };

    template <typename OffsetType, typename SizeType>
    struct generic_string
    {
    private:
        ptr<generic_string_data<SizeType>, OffsetType> data;

    public:
        generic_string(const generic_string &) = delete;

        generic_string(generic_string &&) = delete;

        generic_string(builder_offset<generic_string_data<SizeType>> p)
        {
            // required for initialization of std::optional<generic_string<OffsetType, SizeType>>
            // with '='
            *this = p;
        }

        auto & operator=(builder_offset<generic_string_data<SizeType>> p)
        {
            p.assign_to(data);
            return *this;
        }

        SizeType size() const
        {
            return data->string_length;
        }

        bool empty() const
        {
            return size() == 0;
        }

        const char * begin() const
        {
            return data->begin();
        }

        const char * end() const
        {
            return data->end();
        }

        operator std::string_view() const
        {
            return {begin(), end()};
        }

        template <typename String>
        bool operator==(const String & other) const
        {
            return std::string_view{*this} == other;
        }

        template <typename String>
        bool operator<(const String & other) const
        {
            return std::string_view{*this} < other;
        }

        template <typename String>
        friend bool operator<(
            const String & other, const generic_string<OffsetType, SizeType> & self)
        {
            return other < std::string_view{self};
        }

        friend std::ostream & operator<<(
            std::ostream & o, const generic_string<OffsetType, SizeType> & s)
        {
            return o << std::string_view{s};
        }
    };

    using string32 = generic_string<std::int32_t, std::uint32_t>;

    template <typename T, typename SizeType>
    struct generic_vector_data
    {
        using const_iterator = const T *;
        using iterator = const_iterator;

        SizeType vector_length;
        T items[];

        generic_vector_data(const generic_vector_data &) = delete;

        generic_vector_data(generic_vector_data &&) = delete;

        SizeType size() const
        {
            return vector_length;
        }

        [[nodiscard]] const_iterator begin() const
        {
            return items;
        }

        [[nodiscard]] const_iterator end() const
        {
            return items + vector_length;
        }

        T & operator[](SizeType index)
        {
            if (index >= 0 and index < vector_length) {
                return items[index];
            } else {
                throw std::out_of_range{"index out of range"};
            }
        }

        const T & operator[](SizeType index) const
        {
            return items[index];
        }
    };

    template <typename T, typename OffsetType, typename SizeType>
    struct generic_vector
    {
        using DataType = generic_vector_data<T, SizeType>;

    private:
        ptr<DataType, OffsetType> data;

    public:
        using const_iterator = DataType::const_iterator;
        using iterator = const_iterator;

        generic_vector(const generic_vector &) = delete;

        generic_vector(generic_vector &&) = delete;

        generic_vector(builder_offset<generic_vector_data<T, SizeType>> p)
        {
            // required for initialization of std::optional<generic_vector<OffsetType, SizeType>>
            // with '='
            *this = p;
        }

        auto & operator=(builder_offset<generic_vector_data<T, SizeType>> p)
        {
            p.assign_to(data);
            return *this;
        }

        SizeType size() const
        {
            return data->size();
        }

        bool empty() const
        {
            return size() == 0;
        }

        [[nodiscard]] const_iterator begin() const
        {
            return data->begin();
        }

        [[nodiscard]] const_iterator end() const
        {
            return data->end();
        }

        T & operator[](SizeType index)
        {
            return (*data)[index];
        }

        const T & operator[](SizeType index) const
        {
            return (*data)[index];
        }

        const T & at(SizeType index) const
        {
            if (index >= 0 and index < size()) {
                return (*data)[index];
            } else {
                throw std::out_of_range{"index out of range"};
            }
        }
    };

    template <typename T>
    using vector32 = generic_vector<T, std::int32_t, std::uint32_t>;

    template <typename Key, typename Value, typename OffsetType, typename SizeType>
    struct generic_map
    {
        using ItemType = std::pair<Key, Value>;
        using VectorType = generic_vector<ItemType, OffsetType, SizeType>;
        using const_iterator = typename VectorType::const_iterator;
        using iterator = const_iterator;

    private:
        VectorType items;

    public:
        auto & operator=(builder_offset<generic_vector_data<ItemType, SizeType>> p)
        {
            items = p;
            return *this;
        }

        SizeType size() const
        {
            return items.size();
        }

        [[nodiscard]] const_iterator begin() const
        {
            return items.begin();
        }

        [[nodiscard]] const_iterator end() const
        {
            return items.end();
        }

        template <typename CompatibleKey>
        const_iterator find(const CompatibleKey & key) const
        {
            const auto it{std::lower_bound(
                items.begin(), items.end(), key,
                [](const auto & map_item, const CompatibleKey & key) {
                    return get_key(map_item) < key;
                })};

            if (it == items.end() || get_key(*it) != key) {
                return end();
            }

            return it;
        }

        template <typename CompatibleKey>
        const Value & at(const CompatibleKey & key) const
        {
            const auto it{find(key)};

            if (it == end()) {
                throw std::out_of_range{"key not found"};
            }

            return it->second;
        }

    private:
        template <typename T, typename offset_type>
        static const auto & get_key(const std::pair<ptr<T, offset_type>, Value> & map_item)
        {
            return *map_item.first;
        }

        template <typename T>
        static const auto & get_key(const std::pair<T, Value> & map_item)
        {
            return map_item.first;
        }
    };

    template <typename Key, typename Value>
    using map32 = generic_map<Key, Value, std::int32_t, std::uint32_t>;
}
