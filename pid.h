#pragma once

#include <vector>
#include <cstdint>
#include <string_view>
#include <cstring>
#include <stdexcept>
#include <limits>

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
        relative_ptr(Pointer p)
        {
            *this = p;
        }

        // TODO: make sure that Pointer points to T or const T with std::enable_if
        template <typename Pointer>
        auto & operator=(Pointer p)
        {
            if (p) {
                offset =
                    reinterpret_cast<const char *>(&*p) - reinterpret_cast<const char *>(this);
            } else {
                offset = 0;
            }
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

        generic_string(const generic_string &) = delete;

        generic_string(generic_string &&) = delete;

        SizeType size() const
        {
            return string_length;
        }

        bool empty() const
        {
            return string_length == 0;
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

        template <typename String>
        friend bool operator<(const String & other, const generic_string<SizeType> & self)
        {
            return other < std::string_view{self};
        }

        friend std::ostream & operator<<(std::ostream & o, const generic_string<SizeType> & s)
        {
            return o << std::string_view{s};
        }
    };

    using string32 = generic_string<std::uint32_t>;

    template <typename T, typename SizeType>
    struct generic_vector
    {
        using const_iterator = const T *;
        using iterator = const_iterator;

        SizeType vector_length;
        T data[];

        generic_vector(const generic_vector &) = delete;

        generic_vector(generic_vector &&) = delete;

        SizeType size() const
        {
            return vector_length;
        }

        bool empty() const
        {
            return vector_length == 0;
        }

        [[nodiscard]] const_iterator begin() const
        {
            return data;
        }

        [[nodiscard]] const_iterator end() const
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
        using const_iterator = typename VectorType::const_iterator;
        using iterator = const_iterator;

        VectorType items;

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
        builder() {}

        builder(const builder &) = delete;

        builder(builder &&) = delete;

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
        std::size_t next_offset() const
        {
            const std::size_t current_ptr{
                reinterpret_cast<const std::size_t>(data.data()) + data.size()};
            const std::size_t alignment{alignof(T)};
            const std::size_t alignment_mask{
                std::numeric_limits<std::size_t>::max() << (alignment - 1)};
            const std::size_t padding{(alignment - (current_ptr & ~alignment_mask)) % alignment};
            return data.size() + padding;
        }

        template <typename T>
        builder_offset<T> add(std::size_t extra_bytes = 0)
        {
            const auto offset{next_offset<T>()};
            data.resize(offset + sizeof(T) + extra_bytes);
            return {*this, offset};
        }

        template <typename SizeType = std::uint32_t>
        builder_offset<generic_string<SizeType>> add_string(std::string_view s)
        {
            const auto size{s.size()};
            auto result{add<generic_string<SizeType>>(size + 1)};  // add 1 for null terminator
            result->string_length = size;
            std::memcpy(result->data, s.begin(), s.size());
            result->data[s.size()] = 0;

            return result;
        }

        template <typename T, typename SizeType>
        builder_offset<generic_vector<T, SizeType>> add_vector(SizeType size)
        {
            auto result{add<generic_vector<T, SizeType>>(size * sizeof(T))};
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
        const bool valid;

        builder_offset(builder & b) : b{b}, offset{0}, valid{false} {}

        builder_offset(builder & b, std::size_t offset) : b{b}, offset{offset}, valid{true} {}

        builder_offset(const builder_offset & other)
            : b{other.b}, offset{other.offset}, valid{other.valid}
        {
        }

        builder_offset(builder_offset && other)
            : b{other.b}, offset{other.offset}, valid{other.valid}
        {
        }

        explicit operator bool() const
        {
            return valid;
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

        builder_offset<generic_map<Key, Value, SizeType>> offset() const
        {
            return {items.b, items.offset};
        }

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
