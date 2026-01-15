#pragma once

#include "pid.h"

namespace pid {
    template <typename Key, typename Value, typename SizeType>
    struct generic_map_builder;

    struct builder_offset_mover;

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
            constexpr std::size_t alignment{alignof(T)};
            constexpr std::size_t alignment_mask{
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
        builder_offset<detail::generic_string_data<SizeType>> add_string(std::string_view s)
        {
            const auto size{s.size()};
            auto result{add<detail::generic_string_data<SizeType>>(
                size + 1)};  // add 1 for null terminator
            result->string_length = size;
            std::memcpy(result->data, s.begin(), s.size());
            result->data[s.size()] = 0;

            return result;
        }

        template <typename T, typename SizeType>
        builder_offset<detail::generic_vector_data<T, SizeType>> add_vector(SizeType size)
        {
            auto result{add<detail::generic_vector_data<T, SizeType>>(size * sizeof(T))};
            result->vector_length = size;

            return result;
        }

        template <typename Key, typename Value, typename SizeType>
        generic_map_builder<Key, Value, SizeType> add_map(SizeType size)
        {
            using MapBuilderType = generic_map_builder<Key, Value, SizeType>;
            using ItemType = typename MapBuilderType::ItemType;
            using VectorDataType = typename MapBuilderType::VectorDataType;

            const builder_offset<VectorDataType> items{add_vector<ItemType, SizeType>(size)};
            return MapBuilderType{items};
        }

        struct builder_offset_mover
        {
            builder & destination;
            const builder & source;

            const std::size_t additional_offset;

            template <typename T>
            builder_offset<T> operator()(const builder_offset<T> & source_offset) const
            {
                if (&source_offset.b != &source) {
                    throw std::invalid_argument{
                        "builder_offset does not point to the data of the correct builder"};
                }

                if (not source_offset) {
                    return {destination};
                }

                return {destination, source_offset.offset + additional_offset};
            }
        };

        // By default, we assume that the data in 'other' needs 64-bit alignment.
        template <typename AlignmentType = std::uint64_t>
        builder_offset_mover add_sub_builder(const builder & other)
        {
            const auto offset{next_offset<AlignmentType>()};
            data.resize(offset + other.data.size());
            std::memcpy(data.data() + offset, other.data.data(), other.data.size());

            return builder_offset_mover{*this, other, offset};
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

        template <typename offset_type>
        void assign_to(detail::ptr<T, offset_type> & dest) const
        {
            if (*this) {
                const char * dest_position{reinterpret_cast<const char *>(&dest)};

                {
                    const char * builder_data_start{b.data.data()};
                    const char * builder_data_end{b.data.data() + b.data.size()};

                    if (dest_position < builder_data_start or dest_position >= builder_data_end) {
                        throw std::invalid_argument{
                            "Pointer does not belong to the data of the correct builder"};
                    }
                }

                const std::ptrdiff_t offset64 =
                    reinterpret_cast<const char *>(&**this) - dest_position;
                if (offset64 < std::numeric_limits<offset_type>::min()
                    or offset64 > std::numeric_limits<offset_type>::max()) {
                    throw std::out_of_range{"Pointer is too far away"};
                }
                dest.offset = static_cast<offset_type>(offset64);
            } else {
                dest.offset = 0;
            }
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
        using ItemType = std::pair<Key, Value>;
        using VectorDataType = detail::generic_vector_data<ItemType, SizeType>;

        builder_offset<VectorDataType> items;
        SizeType current_size{0};

        builder_offset<detail::generic_vector_data<ItemType, SizeType>> offset() const
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

            auto & item{(*items)[current_size]};
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
                const auto & last_key{last_item.first};
                if (not(last_key < *p)) {
                    throw std::logic_error{"unsorted"};
                }
            }

            auto & item{(*items)[current_size]};
            item.first = p;

            auto result{items.b.convert_to_builder_offset(&item.second)};
            ++current_size;

            return result;
        }
    };

}
