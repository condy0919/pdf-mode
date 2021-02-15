#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <tuple>
#include <memory>

#include "expected.hpp"

TEST_CASE("ctors") {
    const yapdf::Expected<int, int> e1 = yapdf::Unexpected(0);
    REQUIRE(e1.hasError());
    REQUIRE_EQ(e1.error(), 0);

    const yapdf::Expected<int, int> e2(yapdf::unexpect, 1);
    REQUIRE(e1.hasError());
    REQUIRE_EQ(e2.error(), 1);

    const yapdf::Expected<int, int> e3(std::in_place, 2);
    REQUIRE(e3.hasValue());
    REQUIRE_EQ(e3.value(), 2);

    const yapdf::Expected<std::tuple<int, int>, int> e4(std::in_place, 0, 1);
    REQUIRE(e4.hasValue());
    REQUIRE_EQ(std::get<0>(e4.value()), 0);
    REQUIRE_EQ(std::get<1>(e4.value()), 1);
}

TEST_CASE("emplace") {
    yapdf::Expected<std::string, int> x(yapdf::unexpect, 2);
    REQUIRE(x.hasError());
    REQUIRE_EQ(x.error(), 2);

    x.emplace("foo");
    REQUIRE(x.hasValue());
    REQUIRE_EQ(x.value(), "foo");

    yapdf::Expected<std::unique_ptr<int>, int> e(nullptr);
    e.emplace(new int(42));
    REQUIRE(e.hasValue());
    REQUIRE_EQ(*e.value(), 42);
}

TEST_CASE("hasValue && hasError") {
    const yapdf::Expected<int, int> x(2);
    REQUIRE(x.hasValue());
    REQUIRE_FALSE(x.hasError());

    const yapdf::Expected<int, int> y(yapdf::unexpect, 3);
    REQUIRE(y.hasError());
    REQUIRE_FALSE(y.hasValue());
}

TEST_CASE("value") {
    const yapdf::Expected<int, int> x(2);
    REQUIRE(x.hasValue());
    REQUIRE_EQ(x.value(), 2);
}

TEST_CASE("error") {
    const yapdf::Expected<int, int> x(yapdf::unexpect, 3);
    REQUIRE(x.hasError());
    REQUIRE_EQ(x.error(), 3);
}

TEST_CASE("valueOr") {
    const yapdf::Expected<int, int> x(2);
    REQUIRE_EQ(x.valueOr(3), 2);

    const yapdf::Expected<int, int> y(yapdf::unexpect, 2);
    REQUIRE_EQ(y.valueOr(3), 3);
}

TEST_CASE("valueOrElse") {
    const yapdf::Expected<int, int> x(2);
    REQUIRE_EQ(x.valueOrElse([](int x) { return x; }), 2);

    const yapdf::Expected<int, int> y(yapdf::unexpect, 3);
    REQUIRE_EQ(y.valueOrElse([](int y) { return y; }), 3);
}

TEST_CASE("expect") {
    const yapdf::Expected<int, int> x(2);
    REQUIRE_EQ(x.expect("dummy"), 2);

    const yapdf::Expected<int, int> y(yapdf::unexpect, 3);
    REQUIRE_THROWS_WITH_AS(y.expect("dummy"), "dummy", yapdf::BadExpectedAccess);
}

TEST_CASE("expectErr") {
    const yapdf::Expected<int, int> x(2);
    REQUIRE_THROWS_WITH_AS(x.expectErr("dummy"), "dummy", yapdf::BadExpectedAccess);

    const yapdf::Expected<int, int> y(yapdf::unexpect, 3);
    REQUIRE_EQ(y.expectErr("dummy"), 3);
}

TEST_CASE("map") {
    const auto e1 = yapdf::Expected<int, int>(21).map([](int x) { return x * 2; });
    REQUIRE(e1.hasValue());
    REQUIRE_EQ(e1.value(), 42);

    const yapdf::Expected<int, int> x1(3);
    const auto y1 = x1.map([](int x) { return x + 1; });
    REQUIRE(y1.hasValue());
    REQUIRE_EQ(y1.value(), 4);

    const yapdf::Expected<int, int> x2(yapdf::unexpect, 3);
    const auto y2 = x2.map([](int x) { return x + 1; });
    REQUIRE(y2.hasError());
    REQUIRE_EQ(y2.error(), 3);
}

TEST_CASE("mapErr") {
    const yapdf::Expected<int, int> e1 = 21;
    const auto r1 = e1.mapErr([](int x) { return x + 1; });
    REQUIRE(r1.hasValue());
    REQUIRE_EQ(r1.value(), 21);

    const yapdf::Expected<int, int> e2 = yapdf::Unexpected(21);
    const auto r2 = e2.mapErr([](int x) { return x * 2; });
    REQUIRE(r2.hasError());
    REQUIRE_EQ(r2.error(), 42);
}

TEST_CASE("andThen") {
    auto square = [](int x) -> yapdf::Expected<int, int> { return x * x; };
    auto error = [](int x) -> yapdf::Expected<int, int> { return yapdf::Unexpected(x); };

    const yapdf::Expected<int, int> e1 = 2;
    REQUIRE_EQ(e1.andThen(square).andThen(square).value(), 16);
    REQUIRE_EQ(e1.andThen(square).andThen(error).error(), 4);
    REQUIRE_EQ(e1.andThen(error).andThen(square).error(), 2);
    REQUIRE_EQ(e1.andThen(error).andThen(error).error(), 2);

    const yapdf::Expected<int, int> e2 = yapdf::Unexpected(3);
    REQUIRE_EQ(e2.andThen(square).andThen(square).error(), 3);
}

TEST_CASE("orElse") {
    auto square = [](int x) -> yapdf::Expected<int, int> { return x * x; };
    auto error = [](int x) -> yapdf::Expected<int, int> { return yapdf::Unexpected(x); };

    const yapdf::Expected<int, int> e1 = yapdf::Unexpected(2);
    REQUIRE_EQ(e1.orElse(square).orElse(square).value(), 4);
    REQUIRE_EQ(e1.orElse(square).orElse(error).value(), 4);
    REQUIRE_EQ(e1.orElse(error).orElse(square).value(), 4);
    REQUIRE_EQ(e1.orElse(error).orElse(error).error(), 2);

    const yapdf::Expected<int, int> e2 = 3;
    REQUIRE_EQ(e2.orElse(square).orElse(square).value(), 3);
}
