#include "pid-build-datastructures.h"

#include "catch.hpp"

#include <iostream>

using namespace pid;

template <typename T>
auto build_helper(const T & value)
{
    pid::builder builder;
    const auto result{pid::build(builder, value)};
    using ResultType = std::remove_reference<decltype(*result)>::type;

    const auto offset{result.offset};
    std::vector<char> data{std::move(builder.data)};

    const auto & reference{reinterpret_cast<const ResultType *>(data.data() + offset)};

    // TODO: maybe return a vector<char> and a casted reference to its beginning instead?
    // then we could also make builder non-copyable and non-movable to prevent odd bugs
    return std::make_pair(reference, std::move(data));
}

TEST_CASE("build vector of ints")
{
    std::vector<std::int32_t> v_input{{1, 1, 2, 3, 5, 8}};
    const auto & [result, data] = build_helper(v_input);

    const pid::vector32<std::int32_t> & v{*result};

    REQUIRE(v.size() == 6);
    CHECK(v[0] == 1);
    CHECK(v[1] == 1);
    CHECK(v[2] == 2);
    CHECK(v[3] == 3);
    CHECK(v[4] == 5);
    CHECK(v[5] == 8);
}

TEST_CASE("build vector of strings")
{
    std::vector<std::string> v_input{{"a", "b", "c"}};
    const auto & [result, data] = build_helper(v_input);

    const pid::vector32<pid::relative_ptr<pid::string32>> & v = *result;

    REQUIRE(v.size() == 3);
    CHECK(*v[0] == "a");
    CHECK(*v[1] == "b");
    CHECK(*v[2] == "c");
}

TEST_CASE("build map (int -> int)")
{
    std::map<std::int32_t, std::int32_t> m_input{{42, 1}, {-1, 2}};
    const auto & [result, data] = build_helper(m_input);

    const pid::map32<std::int32_t, std::int32_t> & m{*result};

    REQUIRE(m.size() == 2);
    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    CHECK(m.at(42) == 1);
    CHECK(m.at(-1) == 2);
}

TEST_CASE("build map (int -> str)")
{
    std::map<std::int32_t, std::string> m_input{{42, "a"}, {-1, "b"}};
    const auto & [result, data] = build_helper(m_input);

    const pid::map32<std::int32_t, pid::relative_ptr<pid::string32>> & m = *result;

    REQUIRE(m.size() == 2);
    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    CHECK(*m.at(42) == "a");
    CHECK(*m.at(-1) == "b");
}
