#include <catch2/catch.hpp>
#include <tl/optional.hpp>

#include <tuple>
#include <vector>

struct takes_init_and_variadic {
  std::vector<int> v;
  std::tuple<int, int> t;
  template <class... Args>
  takes_init_and_variadic(std::initializer_list<int> l, Args &&... args)
      : v(l), t(std::forward<Args>(args)...) {}
};

TEST_CASE("Make optional", "[make_optional]") {
  auto o1 = tl::make_optional(42);
  auto o2 = tl::optional<int>(42);

  constexpr bool is_same = std::is_same<decltype(o1), tl::optional<int>>::value;
  REQUIRE(is_same);
  REQUIRE(o1 == o2);

  auto o3 = tl::make_optional<std::tuple<int, int, int, int>>(0, 1, 2, 3);
  REQUIRE(std::get<0>(*o3) == 0);
  REQUIRE(std::get<1>(*o3) == 1);
  REQUIRE(std::get<2>(*o3) == 2);
  REQUIRE(std::get<3>(*o3) == 3);

  auto o4 = tl::make_optional<std::vector<int>>({0, 1, 2, 3});
  REQUIRE(o4.value()[0] == 0);
  REQUIRE(o4.value()[1] == 1);
  REQUIRE(o4.value()[2] == 2);
  REQUIRE(o4.value()[3] == 3);

  auto o5 = tl::make_optional<takes_init_and_variadic>({0, 1}, 2, 3);
  REQUIRE(o5->v[0] == 0);
  REQUIRE(o5->v[1] == 1);
  REQUIRE(std::get<0>(o5->t) == 2);
  REQUIRE(std::get<1>(o5->t) == 3);

  auto i = 42;
  auto o6 = tl::make_optional<int&>(i);
  REQUIRE((std::is_same<decltype(o6), tl::optional<int&>>::value));
  REQUIRE(o6);
  REQUIRE(*o6 == 42);    
}
