#include <iomanip>
#include "pid.h"

#include "catch.hpp"

using namespace pid;

void dump(const std::vector<char> &data) {
    const auto ints{reinterpret_cast<const std::int32_t *>(data.data())};

    for (std::size_t index{0}; index < data.size() / 4; ++index) {
        std::cout << std::setw(4) << 4 * index << ": ";

        for (std::size_t i{0}; i < 4; ++i) {
            const auto c{data[4 * index + i]};
            if (c >= 32) {
                std::cout << c;
            } else {
                std::cout << '*';
            }
        }

        std::cout << " (" << ints[index] << ")" << std::endl;
    }
}

std::vector<char> move_builder_data(builder &b) {
    const char *p1{b.data.data()};
    std::vector<char> result{b.data};

    const char *p2{result.data()};

    REQUIRE(p1 != p2);

    std::fill(b.data.begin(), b.data.end(), 0xff);

    return result;
}

template<typename T>
const T &as(const std::vector<char> &data) {
    return *reinterpret_cast<const T *>(data.data());
}

TEST_CASE("plain old data") {
    builder b;

    {
        builder_offset<std::int32_t> offset{b.add<std::int32_t>()};
        *offset = 42;
    }

    const auto data{move_builder_data(b)};
    const std::int32_t i{as<std::int32_t>(data)};

    CHECK(i == 42);
}

TEST_CASE("struct with plain old data members") {
    struct pod {
        bool b;
        std::int32_t i;
        double x;
        std::uint8_t byte;
        std::array<std::uint32_t, 3> a;
    };

    builder b;

    {
        builder_offset<pod> offset{b.add<pod>()};
        offset->b = true;
        offset->i = 4711;
        offset->x = -1.5;
        offset->byte = 127;
        offset->a[0] = 10;
        offset->a[1] = 20;
        offset->a[2] = 30;
    }

    const auto data{move_builder_data(b)};
    const pod &p{as<pod>(data)};

    CHECK(p.b == true);
    CHECK(p.i == 4711);
    CHECK(p.x == -1.5);
    CHECK(p.byte == 127);
    CHECK(p.a[0] == 10);
    CHECK(p.a[1] == 20);
    CHECK(p.a[2] == 30);
}

TEST_CASE("nested struct")
{
    struct point {
        std::int32_t x;
        std::int32_t y;
    };

    struct line {
        relative_ptr<point> a;
        relative_ptr<point> b;
    };

    builder b;

    {
        builder_offset<line> offset_l{b.add<line>()};
        builder_offset<point> offset_p1{b.add<point>()};
        builder_offset<point> offset_p2{b.add<point>()};

        offset_p1->x = 3;
        offset_p1->y = 5;
        offset_p2->x = 8;
        offset_p2->y = 13;

        offset_l->a = &*offset_p1;
        offset_l->b = &*offset_p2;
    }

    const auto data{move_builder_data(b)};
    const line &l{as<line>(data)};

    CHECK(l.a->x == 3);
    CHECK(l.a->y == 5);
    CHECK(l.b->x == 8);
    CHECK(l.b->y == 13);
}


TEST_CASE("single string") {
    builder b;

    {
        b.add_string("Hello world!");
    }

    const auto data{move_builder_data(b)};
    const string32 &s{as<string32>(data)};

    CHECK(s.size() == 12);
    CHECK(std::string_view{s} == "Hello world!");
}


TEST_CASE("strings")
{
    using StringArray = std::array<relative_ptr<pid::string32>, 4>;

    builder b;

    {
        builder_offset<StringArray> offset_array = b.add<StringArray>();
        (*offset_array)[0] = b.add_string("");
        (*offset_array)[1] = b.add_string("a");
        (*offset_array)[2] = b.add_string("1234");
        (*offset_array)[3] = b.add_string("UTF-8: Bäume");

    }

    const auto data{move_builder_data(b)};
    const StringArray &a{as<StringArray>(data)};

    REQUIRE(a.size() == 4);

    CHECK(a[0]->size() == 0);
    CHECK(*a[0] == "");
    CHECK("" == *a[0]);
    CHECK(*a[0]->end() == 0);

    CHECK(a[1]->size() == 1);
    CHECK(*a[1] == "a");
    CHECK(*a[1]->end() == 0);

    CHECK(a[2]->size() == 4);
    CHECK(*a[2] == "1234");
    CHECK(*a[2]->end() == 0);

    CHECK(a[3]->size() == 13);
    CHECK(*a[3] == "UTF-8: Bäume");
    CHECK(*a[3]->end() == 0);
}


TEST_CASE("vector of ints")
{
    using ItemType = std::int8_t;

    builder b;

    {
        // TODO: it would be nice if we could omit the SizeType argument (std::uint32_t) here, but then we either have
        //  to cast the size that is passed to add_vector to the desired type, or we have to add type-specific functions
        //  like add_vector32.
        builder_offset<vector32<ItemType>> offset_vector = b.add_vector<ItemType, std::uint32_t>(3);
        (*offset_vector)[0] = 42;
        (*offset_vector)[1] = 0;
        (*offset_vector)[2] = -1;
    }

    const auto data{move_builder_data(b)};
    const vector32<ItemType> &v{as<vector32<ItemType>>(data)};

    REQUIRE(v.size() == 3);
    CHECK(v[0] == 42);
    CHECK(v[1] == 0);
    CHECK(v[2] == -1);
}

TEST_CASE("map int -> string")
{
    builder b;
    b.data.reserve(1024);

    {
        auto map_builder{b.add_map<int32_t, relative_ptr<string32>, std::uint32_t>(5)};

        const auto one = b.add_string("one");
        *map_builder.add_key(1) = one;

        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;

        // check sorting violations
        CHECK_THROWS_AS(map_builder.add_key(-1), std::logic_error);
        //CHECK_THROWS_AS(map_builder.add_key(1), std::logic_error);  // TODO: this should throw!

        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;
        *map_builder.add_key(2) = b.add_string("two");
        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;
        std::cout << "now adding 3 (1)..." << std::endl;
        const auto three = b.add_string("three");
        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;
        std::cout << "now adding 3 (2)..." << std::endl;
        auto key3 = map_builder.add_key(3);
        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;
        std::cout << "now adding 3 (3)..." << std::endl;
        std::cout << key3.offset << std::endl;
        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;


        dump(b.data);


        std::cout << "now adding 3 (4)..." << std::endl;
        relative_ptr<string32> &o_key3 = *key3;
        std::cout << o_key3.offset << std::endl;
        o_key3 = three;
        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;
        *map_builder.add_key(4) = b.add_string("four");
        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;
        *map_builder.add_key(6) = b.add_string("six");
        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;

        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;
        CHECK_THROWS_AS(map_builder.add_key(7), std::out_of_range);

        std::cout << "!!> " << std::string_view{*one} << " " << b.data.capacity() << std::endl;
    }

    const auto data{move_builder_data(b)};
    const auto &m{as<generic_map<std::int32_t, relative_ptr<string32>, std::uint32_t>>(data)};

    REQUIRE(m.size() == 5);
    //CHECK(*m.at(1) == "onw");

    for (const auto &[key, val]: m) {
        std::cout << key << ": " << std::string_view{*val} << std::endl;
    }

    for (const auto &item: m) {
        std::cout << (reinterpret_cast<const char *>(&item) - data.data()) << ": " << item.first << " "
                  << item.second.offset << std::endl;
    }
}
