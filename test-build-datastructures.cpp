#include "pid-build-datastructures.h"

#include "catch.hpp"

#include <iostream>

using namespace pid;

template<typename T>
auto build_helper(const T &value) {
    pid::builder builder;
    const auto result{pid::build(builder, value)};
    using ResultType = std::remove_reference<decltype(*result)>::type;

    const auto offset{result.offset};
    std::vector<char> data{std::move(builder.data)};

    const auto &reference{reinterpret_cast<const ResultType *>(data.data() + offset)};

    // TODO: maybe return a vector<char> and a casted reference to its beginning instead?
    // then we could also make builder non-copyable and non-movable to prevent odd bugs
    return std::make_pair(reference, std::move(data));
}

TEST_CASE("build vector of ints") {
    std::vector<std::int32_t> v_input{{1, 1, 2, 3, 5, 8}};
    const auto &[result, data] = build_helper(v_input);

    const pid::vector32<std::int32_t> &v{*result};

    REQUIRE(v.size() == 6);
    CHECK(v[0] == 1);
    CHECK(v[1] == 1);
    CHECK(v[2] == 2);
    CHECK(v[3] == 3);
    CHECK(v[4] == 5);
    CHECK(v[5] == 8);
}

TEST_CASE("build vector of strings") {
    std::vector<std::string> v_input{{"a", "b", "c"}};
    const auto &[result, data] = build_helper(v_input);

    const pid::vector32<pid::relative_ptr<pid::string32 >> &v = *result;

    REQUIRE(v.size() == 3);
    CHECK(*v[0] == "a");
    CHECK(*v[1] == "b");
    CHECK(*v[2] == "c");
}

TEST_CASE("build map (int -> int)")
{
    std::map<std::int32_t, std::int32_t> m_input{{42, 1},
                                                 {-1, 2}};
    const auto &[result, data] = build_helper(m_input);

    const pid::map32<std::int32_t, std::int32_t> &m{*result};

    REQUIRE(m.size() == 2);
    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    CHECK(m.at(42) == 1);
    CHECK(m.at(-1) == 2);
}

TEST_CASE("build map (int -> str)")
{
    std::map<std::int32_t, std::string> m_input{{42, "a"},
                                                {-1, "b"}};
    const auto &[result, data] = build_helper(m_input);

    const pid::map32<std::int32_t, pid::relative_ptr<pid::string32>> &m = *result;

    REQUIRE(m.size() == 2);
    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    CHECK(*m.at(42) == "a");
    CHECK(*m.at(-1) == "b");
}

TEST_CASE("build map (str -> int)")
{
    std::map<std::string, std::int32_t> m_input{{"one",   1},
                                                {"two",   2},
                                                {"three", 3}};
    const auto &[result, data] = build_helper(m_input);

    const pid::map32<pid::relative_ptr<pid::string32>, std::int32_t> &m = *result;

    REQUIRE(m.size() == 3);
    CHECK_THROWS_AS(m.at("four"), std::out_of_range);
    CHECK(m.at("one") == 1);
    CHECK(m.at("two") == 2);
    CHECK(m.at("three") == 3);
}

TEST_CASE("build map (int -> [str])")
{
    std::map<std::int32_t, std::vector<std::string>> m_input{
            {1, {}},
            {2, {"two"}},
            {3, {"three"}},
            {4, {"two", "two"}},
            {5, {"five"}},
            {6, {"two", "three"}}};
    const auto &[result, data] = build_helper(m_input);

    const pid::map32<
            std::int32_t, pid::relative_ptr<
                    pid::vector32<
                            pid::relative_ptr<pid::string32>>>> &m = *result;

    REQUIRE(m.size() == 6);
    CHECK_THROWS_AS(m.at(0), std::out_of_range);
    REQUIRE(m.at(1)->size() == 0);
    REQUIRE(m.at(2)->size() == 1);
    CHECK(*(*m.at(2))[0] == "two");
    REQUIRE(m.at(3)->size() == 1);
    CHECK(*(*m.at(3))[0] == "three");
    REQUIRE(m.at(4)->size() == 2);
    CHECK(*(*m.at(4))[0] == "two");
    CHECK(*(*m.at(4))[1] == "two");
    REQUIRE(m.at(5)->size() == 1);
    CHECK(*(*m.at(5))[0] == "five");
    REQUIRE(m.at(6)->size() == 2);
    CHECK(*(*m.at(6))[0] == "two");
    CHECK(*(*m.at(6))[1] == "three");
}

TEST_CASE("build map (str -> (str -> [int]))") {
    std::map<std::string, std::map<std::string, std::vector<std::int32_t>>> m_input{
            {
                    {"a", {}},
                    {"b", {{"b1", {}}}},
                    {"c", {{"c1", {42}},
                           {"c2", {7, 8, 9}}}},
            }};
    const auto &[result, data] = build_helper(m_input);

    const pid::map32<
            pid::relative_ptr<pid::string32>,
            pid::relative_ptr<pid::map32<
                    pid::relative_ptr<pid::string32>,
                    pid::relative_ptr<pid::vector32<std::int32_t>>
            >>
    > &m = *result;

    REQUIRE(m.size() == 3);
    CHECK_THROWS_AS(m.at("foo"), std::out_of_range);
    REQUIRE(m.at("a")->size() == 0);
    REQUIRE(m.at("b")->size() == 1);
    REQUIRE(m.at("b")->at("b1")->size() == 0);
    REQUIRE(m.at("c")->size() == 2);
    REQUIRE(m.at("c")->at("c1")->size() == 1);
    CHECK((*m.at("c")->at("c1"))[0] == 42);
    REQUIRE(m.at("c")->at("c2")->size() == 3);
    CHECK((*m.at("c")->at("c2"))[0] == 7);
    CHECK((*m.at("c")->at("c2"))[1] == 8);
    CHECK((*m.at("c")->at("c2"))[2] == 9);
}

// TODO: test optionals