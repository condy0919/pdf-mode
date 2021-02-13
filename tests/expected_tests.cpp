#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "expected.hpp"

TEST_CASE("emplace") {
    yapdf::Expected<std::string, int> x(yapdf::unexpect, 2);
    REQUIRE(x.hasError());
    REQUIRE_EQ(x.error(), 2);

    x.emplace("foo");
    REQUIRE(x.hasValue());
    REQUIRE_EQ(x.value(), "foo");
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
