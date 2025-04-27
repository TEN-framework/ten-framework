#include "tests.hh"
#include <iostream>
#include <fstream>

namespace Clingo { namespace Test {

using V = Variant<int, std::string>;
using VU = Variant<int, std::string, std::unique_ptr<int>>;

struct R;
using VR = Variant<int, R>;
struct R {
    VR x;
};

struct D {
    D(std::string &r) : r(r) { r = "not called"; }
    void visit(int &x) {
        r = std::to_string(x);
    }
    void visit(std::string &x) {
        r = x;
    }
    void visit(std::unique_ptr<int> &x) {
        r = std::to_string(*x);
    }
    std::string &r;
};

struct DA {
    int visit(int const &x, int y) {
        return y + 1 + x;
    }
    int visit(std::string const &, int y) {
        return y + 2;
    }
    int visit(std::unique_ptr<int> const &, int y) {
        return y + 3;
    }
};

TEST_CASE("visitor", "[clingo]") {
    V x{10};
    REQUIRE(x.is<int>());
    REQUIRE(x.accept(DA(), 3) == 14);
    REQUIRE(!x.is<std::string>());
    REQUIRE(x.get<int>() == 10);
    std::string s = "s1";
    x = static_cast<std::string const &>(s);
    REQUIRE(x.get<std::string>() == "s1");
    x = s;
    REQUIRE(x.get<std::string>() == "s1");
    x = std::move(s);
    REQUIRE(x.get<std::string>() == "s1");
    s = "s2";
    REQUIRE(V{s}.get<std::string>() == "s2");
    REQUIRE(V{static_cast<std::string const &>(s)}.get<std::string>() == "s2");
    REQUIRE(V{std::move(s)}.get<std::string>() == "s2");

    V y = x;
    REQUIRE(y.get<std::string>() == "s1");
    x = y;
    REQUIRE(x.get<std::string>() == "s1");

    VR xr{R{1}};
    REQUIRE(xr.get<R>().x.get<int>() == 1);
    xr = std::move(xr.get<R>().x);
    REQUIRE(xr.get<int>() == 1);

#if (__clang__ || __GNUC__ >= 5)
    std::string r;
    y.accept(D(r));
    REQUIRE(r == "s1");
    x.accept(D(r));
    REQUIRE(r == "s1");
    REQUIRE(x.accept(DA(), 3) == 5);

    auto xu = VU::make<std::unique_ptr<int>>(nullptr);
    auto yu = VU::make<std::string>("s1");
    REQUIRE(!xu.get<std::unique_ptr<int>>());

    xu.swap(yu);
    REQUIRE(!yu.get<std::unique_ptr<int>>());
    REQUIRE(xu.get<std::string>() == "s1");
#endif
}

} } // namespace Test Clingo
