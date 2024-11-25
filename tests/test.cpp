#include <pid/pid.h>
#include <pid/builder.h>

#include "catch.hpp"

#include <optional>

#include "pid-debug.h"

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

    CHECK(i == 42);
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
    struct point
    {
        std::int32_t x;
        std::int32_t y;
    };

    struct line
    {
        ptr32<point> a;
        ptr32<point> b;
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

        offset_l->a = offset_p1;
        offset_l->b = offset_p2;
    }

    const auto data{move_builder_data(b)};
    const line & l{as<line>(data)};

    CHECK(l.a->x == 3);
    CHECK(l.a->y == 5);
    CHECK(l.b->x == 8);
    CHECK(l.b->y == 13);
}

TEST_CASE("single string")
{
    builder b;

    {
        auto s = b.add<string32>();
        auto data = b.add_string("Hello world!");
        s->data = data;
    }

    const auto data{move_builder_data(b)};
    const string32 & s{as<string32>(data)};

    CHECK(s.size() == 12);
    CHECK(std::string_view{s} == "Hello world!");

    // Verify that we can print strings
    std::ostringstream o;
    o << s;
    CHECK(o.str() == "Hello world!");
}

TEST_CASE("strings")
{
    using StringArray = std::array<pid::string32, 5>;

    builder b;

    {
        builder_offset<StringArray> offset_array = b.add<StringArray>();
        (*offset_array)[0] = b.add_string("");
        (*offset_array)[1] = b.add_string("a");
        (*offset_array)[2] = b.add_string("1234");
        (*offset_array)[3] = b.add_string("UTF-8: Bäume");
        // 4th item is default initialized, i.e., empty
    }

    const auto data{move_builder_data(b)};
    const StringArray & a{as<StringArray>(data)};

    REQUIRE(a.size() == 5);

    CHECK(a[0].empty());
    CHECK(a[0].size() == 0);
    CHECK(a[0] == "");
    CHECK("" == a[0]);
    CHECK(*a[0].end() == 0);

    CHECK(not a[1].empty());
    CHECK(a[1].size() == 1);
    CHECK(a[1] == "a");
    CHECK(*a[1].end() == 0);

    CHECK(not a[1].empty());
    CHECK(a[2].size() == 4);
    CHECK(a[2] == "1234");
    CHECK(*a[2].end() == 0);

    CHECK(not a[1].empty());
    CHECK(a[3].size() == 13);
    CHECK(a[3] == "UTF-8: Bäume");
    CHECK(*a[3].end() == 0);

    CHECK(a[4].empty());
    CHECK(a[4].size() == 0);
    CHECK(a[4] == "");
    CHECK("" == a[4]);
    CHECK(*a[4].end() == 0);

    // Test comparisons
    const string32 & s_1234{a[2]};
    CHECK(s_1234 == "1234");
    CHECK(s_1234 == std::string{"1234"});
    CHECK(std::string{"1234"} == s_1234);
    CHECK(s_1234 < std::string{"234"});
    CHECK(std::string{"123"} < s_1234);
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

        REQUIRE(offset_vector->size() == 3);
    }

    const auto data{move_builder_data(b)};
    const vector32<ItemType> & v{as<vector32<ItemType>>(data)};

    REQUIRE(v.size() == 3);
    CHECK(v[0] == 42);
    CHECK(v[1] == 0);
    CHECK(v[2] == -1);
}

TEST_CASE("map int -> string")
{
    builder b;

    {
        auto map_builder{b.add_map<int32_t, string32, std::uint32_t>(5)};

        *map_builder.add_key(1) = b.add_string("one");

        // check sorting violations
        CHECK_THROWS_AS(map_builder.add_key(-1), std::logic_error);
        CHECK_THROWS_AS(map_builder.add_key(1), std::logic_error);

        *map_builder.add_key(2) = b.add_string("two");
        *map_builder.add_key(3) = b.add_string("three");
        *map_builder.add_key(4) = b.add_string("four");
        *map_builder.add_key(6) = b.add_string("six");

        CHECK_THROWS_AS(map_builder.add_key(7), std::out_of_range);
    }

    const auto data{move_builder_data(b)};
    const auto & m{as<generic_map<std::int32_t, string32, std::uint32_t>>(data)};

    REQUIRE(m.size() == 5);

    CHECK(m.at(1) == "one");
    CHECK(m.at(2) == "two");
    CHECK(m.at(3) == "three");
    CHECK(m.at(4) == "four");
    CHECK(m.at(6) == "six");

    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    CHECK_THROWS_AS(m.at(5), std::out_of_range);
    CHECK_THROWS_AS(m.at(7), std::out_of_range);

    CHECK(m.find(1) == m.begin());
    CHECK(m.find(1)->first == 1);
    CHECK(m.find(1)->second == "one");
    CHECK(m.find(2)->first == 2);
    CHECK(m.find(2)->second == "two");
    CHECK(m.find(5) == m.end());
}

TEST_CASE("map string -> int")
{
    builder b;

    {
        auto map_builder{b.add_map<string32, std::int32_t, std::uint32_t>(5)};

        *map_builder.add_key(b.add_string("four")) = 4;

        // check sorting violations
        CHECK_THROWS_AS(map_builder.add_key(b.add_string("evil")), std::logic_error);
        CHECK_THROWS_AS(map_builder.add_key(b.add_string("four")), std::logic_error);

        *map_builder.add_key(b.add_string("one")) = 1;
        *map_builder.add_key(b.add_string("six")) = 6;
        *map_builder.add_key(b.add_string("three")) = 3;
        *map_builder.add_key(b.add_string("two")) = 2;

        CHECK_THROWS_AS(map_builder.add_key(b.add_string("unicorn")), std::out_of_range);
    }

    const auto data{move_builder_data(b)};
    const auto & m{as<generic_map<string32, std::int32_t, std::uint32_t>>(data)};

    REQUIRE(m.size() == 5);

    CHECK(m.at("one") == 1);
    CHECK(m.at("two") == 2);
    CHECK(m.at("three") == 3);
    CHECK(m.at("four") == 4);
    CHECK(m.at("six") == 6);

    CHECK_THROWS_AS(m.at("a"), std::out_of_range);
    CHECK_THROWS_AS(m.at("m"), std::out_of_range);
    CHECK_THROWS_AS(m.at("z"), std::out_of_range);

    CHECK(m.find("four") == m.begin());
    CHECK(m.find("four")->first == "four");
    CHECK(m.find("four")->second == 4);
    CHECK(m.find("five") == m.end());
}

namespace {
    auto alignment(const auto & rel)
    {
        const auto p{&rel};

        std::size_t result{1};
        while (not(reinterpret_cast<const std::size_t>(p) & result)) {
            result <<= 1;
        }

        return result;
    };

    auto expected_alignment(const auto & p)
    {
        return alignof(decltype(p));
    };
}

TEST_CASE("alignment")
{
    builder b;

    struct test
    {
        ptr32<std::uint8_t> u8;
        ptr32<std::uint16_t> u16;
        ptr32<std::uint32_t> u32;
        ptr32<std::uint64_t> u64;
        ptr32<std::int8_t> i8;
        ptr32<std::int16_t> i16;
        ptr32<vector32<std::int32_t>> v32_i32;
        ptr32<std::int32_t> i32;
        ptr32<vector32<double>> v32_d;
    };

    {
        builder_offset<test> t{b.add<test>()};

        auto u8 = b.add<std::uint8_t>();
        t->u8 = u8;
        *u8 = 8;

        auto u16 = b.add<std::uint16_t>();
        t->u16 = u16;
        *u16 = 16;

        auto u32 = b.add<std::uint32_t>();
        t->u32 = u32;
        *u32 = 32;

        auto u64 = b.add<std::uint64_t>();
        t->u64 = u64;
        *u64 = 64;

        auto i8 = b.add<std::int8_t>();
        t->i8 = i8;
        *i8 = -8;

        auto i16 = b.add<std::int16_t>();
        t->i16 = i16;
        *i16 = -16;

        auto v32_i32 = b.add_vector<std::int32_t, std::uint32_t>(1);
        t->v32_i32 = v32_i32;
        (*v32_i32)[0] = 42;

        // important check - accessing items through a builder_offset and a
        // ptr requires const-correctness in ptr::get()
        REQUIRE(t->v32_i32->size() == 1);

        auto i32 = b.add<std::int32_t>();
        t->i32 = i32;
        *i32 = -32;

        auto v32_d = b.add_vector<double, std::uint32_t>(1);
        t->v32_d = v32_d;
        (*v32_d)[0] = 1.5;
    }

    const auto data{move_builder_data(b)};
    const auto & t{as<test>(data)};

    CHECK(*t.u8 == 8);
    CHECK(*t.u16 == 16);
    CHECK(*t.u32 == 32);
    CHECK(*t.u64 == 64);
    CHECK(*t.i8 == -8);
    CHECK(*t.i16 == -16);
    CHECK(t.v32_i32->size() == 1);
    CHECK((*t.v32_i32)[0] == 42);
    CHECK(*t.i32 == -32);
    CHECK(t.v32_d->size() == 1);
    CHECK((*t.v32_d)[0] == 1.5);

    CHECK(alignment(*t.u8) >= expected_alignment(*t.u8));
    CHECK(alignment(*t.u16) >= expected_alignment(*t.u16));
    CHECK(alignment(*t.u32) >= expected_alignment(*t.u32));
    CHECK(alignment(*t.u64) >= expected_alignment(*t.u64));
    CHECK(alignment(*t.i8) >= expected_alignment(*t.i8));
    CHECK(alignment(*t.i16) >= expected_alignment(*t.i16));
    CHECK(alignment(*t.v32_i32) >= expected_alignment(*t.v32_i32));
    CHECK(alignment(*t.i32) >= expected_alignment(*t.i32));
    CHECK(alignment(*t.v32_d) >= expected_alignment(*t.v32_d));

    // Check that we do not waste space with excessive padding
    CHECK(data.size() == 88);
}

TEST_CASE("alignment of vector data")
{
    builder b;

    constexpr std::uint64_t n0{0x0100000000000002};
    constexpr std::uint64_t n1{0x0300000000000004};

    {
        auto v{b.add_vector<std::uint64_t, std::uint32_t>(2)};

        (*v)[0] = n0;
        (*v)[1] = n1;
    }

    const auto data{move_builder_data(b)};
    const auto & v{as<vector32<std::uint64_t>>(data)};

    REQUIRE(v.size() == 2);
    CHECK(v[0] == n0);
    CHECK(v[1] == n1);

    CHECK(alignment(v[0]) >= expected_alignment(v[0]));
    CHECK(alignment(v[1]) >= expected_alignment(v[1]));

    // Check that we do not waste space with excessive padding
    CHECK(data.size() == 24);
}

TEST_CASE("struct with optionals")
{
    builder b;

    struct test
    {
        std::optional<string32> s1;
        std::optional<string32> s2;

        std::optional<ptr32<vector32<std::int32_t>>> v1;
        std::optional<ptr32<vector32<std::int32_t>>> v2;
        std::optional<ptr32<vector32<std::int32_t>>> v3;
    };

    {
        builder_offset<test> t{b.add<test>()};

        // This does not work because the address of t->s2 is determined before "foo" is added,
        // which triggers a resizing of the buffer:
        // t->s2.emplace(b.add_string("foo"));

        // This works because the address of t->s2 is determined after "foo" is added:
        const auto foo{b.add_string("foo")};
        t->s2.emplace(foo);

        t->v2 = b.add_vector<std::int32_t, std::uint32_t>(0);

        auto v3 = b.add_vector<std::int32_t, std::uint32_t>(2);
        t->v3 = v3;
        (*v3)[0] = 42;
        (*v3)[1] = -1;
    }

    const auto data{move_builder_data(b)};
    const auto & t{as<test>(data)};

    CHECK(not t.s1);

    CHECK(t.s2);
    CHECK(*t.s2 == "foo");

    CHECK(not t.v1);

    CHECK(t.v2);
    CHECK((*t.v2)->size() == 0);
    CHECK((*t.v2)->empty());

    CHECK(t.v3);
    CHECK((*t.v3)->size() == 2);
    CHECK((**t.v3)[0] == 42);
    CHECK((**t.v3)[1] == -1);
}

TEST_CASE("offset overflow")
{
    struct s
    {
        ptr8<std::int32_t> a;
    };

    builder b;

    {
        builder_offset<s> offset_s{b.add<s>()};

        // offsets up to i = 30 fit into an std::int8_t
        for (std::int32_t i{0}; i < 31; ++i) {
            builder_offset<std::int32_t> offset_int32{b.add<std::int32_t>()};
            *offset_int32 = i;
            offset_s->a = offset_int32;
        }

        // offset 31 does not fit into an std::int8_t
        {
            builder_offset<std::int32_t> offset_int32{b.add<std::int32_t>()};
            *offset_int32 = 31;
            CHECK_THROWS_AS(offset_s->a = offset_int32, std::out_of_range);
        }
    }

    const auto data{move_builder_data(b)};
    const auto & result{as<s>(data)};

    CHECK(*result.a == 30);
}

TEST_CASE("different builder")
{
    struct s
    {
        ptr32<std::int32_t> a;
        ptr32<std::int32_t> b;
    };

    builder b1;
    builder b2;

    {
        builder_offset<s> offset_s{b1.add<s>()};

        builder_offset<std::int32_t> incompatible_offset{b2.add<std::int32_t>()};
        *incompatible_offset = 41;

        builder_offset<std::int32_t> compatible_offset{b1.add<std::int32_t>()};
        *compatible_offset = 42;

        CHECK_THROWS_AS(offset_s->a = incompatible_offset, std::invalid_argument);

        offset_s->b = compatible_offset;
    }

    const auto data{move_builder_data(b1)};
    const auto & result{as<s>(data)};

    CHECK(not result.a);
    REQUIRE(result.b);
    CHECK(*result.b == 42);
}
