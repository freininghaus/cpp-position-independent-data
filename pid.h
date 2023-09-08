#pragma once

#include <vector>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <stdexcept>

namespace pid {
    template <typename T, typename offset_type = std::int32_t>
    struct relative_ptr
    {
        // using ItemType = T;
        offset_type offset;

        relative_ptr(const struct relative_ptr &) = delete;

        relative_ptr(struct relative_ptr &&) = delete;

        // TODO: make sure that Pointer points to T or const T with std::enable_if
        template <typename Pointer>
        auto & operator=(Pointer p)
        {
            offset = reinterpret_cast<const char *>(&*p) - reinterpret_cast<const char *>(this);
            return *this;
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
        Pointer get() const
        {
            return reinterpret_cast<Pointer>(reinterpret_cast<const char *>(this) + offset);
        }
    };

    template <typename SizeType>
    struct generic_string
    {
        SizeType string_length;
        char data[];

        SizeType size() const
        {
            return string_length;
        }

        [[nodiscard]] const char * begin() const
        {
            return data;
        }

        [[nodiscard]] const char * end() const
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
    };

    using string32 = generic_string<std::uint32_t>;

    template <typename T, typename SizeType>
    struct generic_vector
    {
        SizeType vector_length;
        T data[];

        SizeType size() const
        {
            return vector_length;
        }

        [[nodiscard]] const T * begin() const
        {
            return data;
        }

        [[nodiscard]] const T * end() const
        {
            return data + vector_length;
        }

        T & operator[](SizeType index)
        {
            return data[index];
        }

        const T & operator[](SizeType index) const
        {
            return data[index];
        }
    };

    template <typename T>
    using vector32 = generic_vector<T, std::uint32_t>;

    template <typename Key, typename Value, typename SizeType>
    struct generic_map
    {
        using ItemType = std::pair<Key, Value>;
        using VectorType = generic_vector<ItemType, SizeType>;

        VectorType items;

        SizeType size() const
        {
            return items.size();
        }

        [[nodiscard]] const ItemType * begin() const
        {
            return items.begin();
        }

        [[nodiscard]] const ItemType * end() const
        {
            return items.end();
        }

        template <typename CompatibleKey>
        const Value & at(const CompatibleKey & key) const
        {
            const auto it{std::lower_bound(
                items.begin(), items.end(), key,
                [](const auto & map_item, const CompatibleKey & key) {
                    return get_key(map_item) < key;
                })};

            if (it == items.end() || get_key(*it) != key) {
                throw std::out_of_range{"key not found"};
            }

            return it->second;
        }

    private:
        template <typename T>
        static const auto & get_key(const std::pair<relative_ptr<T>, Value> & map_item)
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
    using map32 = generic_map<Key, Value, std::uint32_t>;

    template <typename T>
    struct builder_offset;

    template <typename Key, typename Value, typename SizeType>
    struct generic_map_builder;

    struct builder
    {
        std::vector<char> data;

        template <typename T>
        builder_offset<T> convert_to_builder_offset(T * p)
        {
            auto c{reinterpret_cast<const char *>(p)};
            if (c < data.data() or c > data.data() + data.size()) {
                throw std::out_of_range{"Pointer does not point to builder data"};
            }

            const std::size_t offset{
                static_cast<std::size_t>(reinterpret_cast<char *>(p) - data.data())};

            return {*this, offset};
        }

        template <typename T>
        builder_offset<T> add()
        {
            // TODO: alignment!
            const auto offset{data.size()};
            data.resize(data.size() + sizeof(T));
            return {*this, offset};
        }

        template <typename SizeType = std::uint32_t>
        builder_offset<generic_string<SizeType>> add_string(std::string_view s)
        {
            // TODO: alignment!
            const auto total_size = (sizeof(SizeType) + s.size() + 1 + 3) / 4 * 4;
            const auto offset{data.size()};

            data.resize(data.size() + total_size);
            builder_offset<generic_string<SizeType>> result{*this, offset};
            result->string_length = s.size();
            std::memcpy(result->data, s.begin(), s.size());
            result->data[s.size()] = 0;

            return result;
        }

        template <typename T, typename SizeType>
        builder_offset<generic_vector<T, SizeType>> add_vector(SizeType size)
        {
            // TODO: alignment!
            const auto total_size = sizeof(SizeType) + sizeof(T) * size;
            // const auto total_size = sizeof(SizeType) * size;
            const auto offset{data.size()};
            data.resize(data.size() + total_size);

            builder_offset<generic_vector<T, SizeType>> result{*this, offset};
            result->vector_length = size;

            return result;
        }

        template <typename Key, typename Value, typename SizeType>
        generic_map_builder<Key, Value, SizeType> add_map(SizeType size)
        {
            using MapBuilderType = generic_map_builder<Key, Value, SizeType>;
            using ItemType = typename MapBuilderType::ItemType;
            using VectorType = typename MapBuilderType::VectorType;

            const builder_offset<VectorType> items{add_vector<ItemType, SizeType>(size)};
            return MapBuilderType{items};
        }
    };

    template <typename T>
    struct builder_offset
    {
        builder & b;
        const std::size_t offset;

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
            return reinterpret_cast<T *>(b.data.data() + offset);
        }

        const T * operator->() const
        {
            return reinterpret_cast<const T *>(b.data.data() + offset);
        }
    };

    template <typename Key, typename Value, typename SizeType>
    struct generic_map_builder
    {
        using MapType = generic_map<Key, Value, SizeType>;
        using VectorType = typename MapType::VectorType;
        using ItemType = typename MapType::ItemType;

        builder_offset<VectorType> items;
        SizeType current_size{0};

        builder_offset<Value> add_key(const Key & key)
        {
            if (current_size == items->size()) {
                throw std::out_of_range{"map is full"};
            }

            if (current_size > 0 and not((*items)[current_size - 1].first < key)) {
                throw std::logic_error{"unsorted"};
            }

            ItemType & item{(*items)[current_size]};
            item.first = key;

            auto result{items.b.convert_to_builder_offset(&item.second)};
            ++current_size;

            return result;
        }

        template <typename Pointer>
        builder_offset<Value> add_key(Pointer p)
        {
            if (current_size == items->size()) {
                throw std::out_of_range{"map is full"};
            }

            if (current_size > 0) {
                const auto & last_item{(*items)[current_size - 1]};
                const auto & last_key{*last_item.first};
                if (not(last_key < *p)) {
                    throw std::logic_error{"unsorted"};
                }
            }

            ItemType & item{(*items)[current_size]};
            item.first = p;

            auto result{items.b.convert_to_builder_offset(&item.second)};
            ++current_size;

            return result;
        }
    };
}
