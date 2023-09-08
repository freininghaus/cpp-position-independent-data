#include "pid.h"

#include "catch.hpp"

using namespace pid;

std::vector<char> move_builder_data(builder & b)
{
    const char * p1{b.data.data()};
    std::vector<char> result{b.data};

    const char * p2{result.data()};

    REQUIRE(p1 != p2);

    std::fill(b.data.begin(), b.data.end(), 0xff);

    return result;
}

template <typename T>
const T & as(const std::vector<char> & data)
{
    return *reinterpret_cast<const T *>(data.data());
}

TEST_CASE("plain old data")
{
    builder b;

    {
        builder_offset<std::int32_t> offset{b.add<std::int32_t>()};
        *offset = 42;
    }

    const auto data{move_builder_data(b)};
    const std::int32_t i{as<std::int32_t>(data)};

    REQUIRE(i == 42);
}

TEST_CASE("struct with plain old data members")
{
    struct pod
    {
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
    const pod & p{as<pod>(data)};

    REQUIRE(p.b == true);
    REQUIRE(p.i == 4711);
    REQUIRE(p.x == -1.5);
    REQUIRE(p.byte == 127);
    REQUIRE(p.a[0] == 10);
    REQUIRE(p.a[1] == 20);
    REQUIRE(p.a[2] == 30);
}

TEST_CASE("nested struct")
{
    struct point
    {
        std::int32_t x;
        std::int32_t y;
    };

    struct line
    {
        offset<point> a;
        offset<point> b;
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
    const line & l{as<line>(data)};

    REQUIRE(l.a->x == 3);
    REQUIRE(l.a->y == 5);
    REQUIRE(l.b->x == 8);
    REQUIRE(l.b->y == 13);
}

TEST_CASE("single string")
{
    builder b;

    {
        b.add_string("Hello world!");
    }

    const auto data{move_builder_data(b)};
    const string32 & s{as<string32>(data)};

    REQUIRE(s.size() == 12);
    REQUIRE(std::string_view{s} == "Hello world!");
}

TEST_CASE("strings")
{
    using StringArray = std::array<offset<pid::string32>, 4>;

    builder b;

    {
        builder_offset<StringArray> offset_array = b.add<StringArray>();
        (*offset_array)[0] = b.add_string("");
        (*offset_array)[1] = b.add_string("a");
        (*offset_array)[2] = b.add_string("1234");
        (*offset_array)[3] = b.add_string("UTF-8: Bäume");
    }

    const auto data{move_builder_data(b)};
    const StringArray & a{as<StringArray>(data)};

    REQUIRE(a.size() == 4);

    REQUIRE(a[0]->size() == 0);
    REQUIRE(*a[0] == "");
    REQUIRE("" == *a[0]);
    REQUIRE(*a[0]->end() == 0);

    REQUIRE(a[1]->size() == 1);
    REQUIRE(*a[1] == "a");
    REQUIRE(*a[1]->end() == 0);

    REQUIRE(a[2]->size() == 4);
    REQUIRE(*a[2] == "1234");
    REQUIRE(*a[2]->end() == 0);

    REQUIRE(a[3]->size() == 13);
    REQUIRE(*a[3] == "UTF-8: Bäume");
    REQUIRE(*a[3]->end() == 0);
}

TEST_CASE("vector of ints")
{
    using ItemType = std::int8_t;

    builder b;

    {
        // TODO: it would be nice if we could omit the SizeType argument (std::uint32_t) here, but
        // then we either have
        //  to cast the size that is passed to add_vector to the desired type, or we have to add
        //  type-specific functions like add_vector32.
        builder_offset<vector32<ItemType>> offset_vector =
            b.add_vector<ItemType, std::uint32_t>(3);
        (*offset_vector)[0] = 42;
        (*offset_vector)[1] = 0;
        (*offset_vector)[2] = -1;
    }

    const auto data{move_builder_data(b)};
    const vector32<ItemType> & v{as<vector32<ItemType>>(data)};

    REQUIRE(v.size() == 3);
    REQUIRE(v[0] == 42);
    REQUIRE(v[1] == 0);
    REQUIRE(v[2] == -1);
}
