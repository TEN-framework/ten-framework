#include <catch2/catch.hpp>
#include <tl/optional.hpp>

TEST_CASE("Nullopt", "[nullopt]") {
  tl::optional<int> o1 = tl::nullopt;
  tl::optional<int> o2{tl::nullopt};
  tl::optional<int> o3(tl::nullopt);
  tl::optional<int> o4 = {tl::nullopt};

  REQUIRE(!o1);
  REQUIRE(!o2);
  REQUIRE(!o3);
  REQUIRE(!o4);

  REQUIRE(!std::is_default_constructible<tl::nullopt_t>::value);
}
