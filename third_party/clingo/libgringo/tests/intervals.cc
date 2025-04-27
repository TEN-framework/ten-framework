// {{{ MIT License

// Copyright 2017 Roland Kaminski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// }}}

#include "gringo/intervals.hh"
#include "gringo/utility.hh"

#include "tests/tests.hh"

namespace Gringo { namespace Test {

using IS = IntervalSet<int>;
using LB = IS::LBound;
using RB = IS::RBound;
using IV = IS::Interval;
using S = std::string;

namespace {

LB lb(int x, bool y) { return {x, y}; }
RB rb(int x, bool y) { return {x, y}; }
std::string print(IS const &x) {
    auto f = [](std::ostream &out, IV const &x) {
        out
            << (x.left.inclusive ? "[" : "(")
            << x.left.bound
            << ","
            << x.right.bound
            << (x.right.inclusive ? "]" : ")");
    };
    std::ostringstream out;
    out << "{";
    print_comma(out, x, ",", f);
    out << "}";
    return out.str();
}

}

TEST_CASE("intervals", "[base]") {
    SECTION("boundLess") {
        REQUIRE(!(lb(2,true)  < lb(1,true)));
        REQUIRE(!(lb(2,true)  < lb(1,false)));
        REQUIRE(!(lb(2,false) < lb(1,true)));
        REQUIRE(!(lb(2,false) < lb(1,false)));

        REQUIRE( (lb(1,true)  < lb(2,true)));
        REQUIRE( (lb(1,true)  < lb(2,false)));
        REQUIRE( (lb(1,false) < lb(2,true)));
        REQUIRE( (lb(1,false) < lb(2,false)));

        REQUIRE(!(lb(1,true)  < lb(1,true)));
        REQUIRE( (lb(1,true)  < lb(1,false)));
        REQUIRE(!(lb(1,false) < lb(1,true)));
        REQUIRE(!(lb(1,false) < lb(1,false)));
        // same for RB
        REQUIRE(!(rb(2,true)  < rb(1,true)));
        REQUIRE(!(rb(2,true)  < rb(1,false)));
        REQUIRE(!(rb(2,false) < rb(1,true)));
        REQUIRE(!(rb(2,false) < rb(1,false)));

        REQUIRE( (rb(1,true)  < rb(2,true)));
        REQUIRE( (rb(1,true)  < rb(2,false)));
        REQUIRE( (rb(1,false) < rb(2,true)));
        REQUIRE( (rb(1,false) < rb(2,false)));

        REQUIRE(!(rb(1,true)  < rb(1,true)));
        REQUIRE(!(rb(1,true)  < rb(1,false)));
        REQUIRE( (rb(1,false) < rb(1,true)));
        REQUIRE(!(rb(1,false) < rb(1,false)));
    }

    SECTION("boundLessEq") {
        REQUIRE(!(lb(2,true)  <= lb(1,true)));
        REQUIRE(!(lb(2,true)  <= lb(1,false)));
        REQUIRE(!(lb(2,false) <= lb(1,true)));
        REQUIRE(!(lb(2,false) <= lb(1,false)));

        REQUIRE( (lb(1,true)  <= lb(2,true)));
        REQUIRE( (lb(1,true)  <= lb(2,false)));
        REQUIRE( (lb(1,false) <= lb(2,true)));
        REQUIRE( (lb(1,false) <= lb(2,false)));

        REQUIRE( (lb(1,true)  <= lb(1,true)));
        REQUIRE( (lb(1,true)  <= lb(1,false)));
        REQUIRE(!(lb(1,false) <= lb(1,true)));
        REQUIRE( (lb(1,false) <= lb(1,false)));
        // same for RB
        REQUIRE(!(rb(2,true)  <= rb(1,true)));
        REQUIRE(!(rb(2,true)  <= rb(1,false)));
        REQUIRE(!(rb(2,false) <= rb(1,true)));
        REQUIRE(!(rb(2,false) <= rb(1,false)));

        REQUIRE( (rb(1,true)  <= rb(2,true)));
        REQUIRE( (rb(1,true)  <= rb(2,false)));
        REQUIRE( (rb(1,false) <= rb(2,true)));
        REQUIRE( (rb(1,false) <= rb(2,false)));

        REQUIRE( (rb(1,true)  <= rb(1,true)));
        REQUIRE(!(rb(1,true)  <= rb(1,false)));
        REQUIRE( (rb(1,false) <= rb(1,true)));
        REQUIRE( (rb(1,false) <= rb(1,false)));
    }

    SECTION("boundBefore") {
        // before with gap
        // {
        //  }
        REQUIRE((lb(1,true)  < rb(2,true)));
        REQUIRE((lb(1,true)  < rb(2,false)));
        REQUIRE((lb(1,false) < rb(2,true)));
        REQUIRE((lb(1,false) < rb(2,false)));
        // [        {           {
        // ] - gap  ) - no gap  ) - no gap
        REQUIRE( (lb(1,true)  < rb(1,true)));
        REQUIRE(!(lb(1,true)  < rb(1,false)));
        REQUIRE(!(lb(1,false) < rb(1,true)));
        REQUIRE(!(lb(1,false) < rb(1,false)));
        //  {
        // }
        REQUIRE(!(lb(2,true)  < rb(1,true)));
        REQUIRE(!(lb(2,true)  < rb(1,false)));
        REQUIRE(!(lb(2,false) < rb(1,true)));
        REQUIRE(!(lb(2,false) < rb(1,false)));
        // the other way round
        // }
        //  {
        REQUIRE((rb(1,true)  < lb(2,true)));
        REQUIRE((rb(1,true)  < lb(2,false)));
        REQUIRE((rb(1,false) < lb(2,true)));
        REQUIRE((rb(1,false) < lb(2,false)));
        // }           ]           )
        // [ - no gap  { - no gap  ( - gap
        REQUIRE(!(rb(1,true)  < lb(1,true)));
        REQUIRE(!(rb(1,true)  < lb(1,false)));
        REQUIRE(!(rb(1,false) < lb(1,true)));
        REQUIRE( (rb(1,false) < lb(1,false)));
        //  }
        // {
        REQUIRE(!(rb(2,true)  < lb(1,true)));
        REQUIRE(!(rb(2,true)  < lb(1,false)));
        REQUIRE(!(rb(2,false) < lb(1,true)));
        REQUIRE(!(rb(2,false) < lb(1,false)));
    }

    SECTION("add") {
        IS x;
        x.add(1,true,1,true);
        REQUIRE("{[1,1]}" == print(x));
        x.add(3,true,2,true);
        REQUIRE("{[1,1]}" == print(x));
        x.add(3,true,4,false);
        REQUIRE("{[1,1],[3,4)}" == print(x));
        x.add(2,false,3,false);
        REQUIRE("{[1,1],(2,4)}" == print(x));
        x.add(1,false,2,false);
        REQUIRE("{[1,2),(2,4)}" == print(x));
        x.add(2,true,2,true);
        REQUIRE("{[1,4)}" == print(x));
        x.add(4,true,5,true);
        REQUIRE("{[1,5]}" == print(x));
        x.add(8,false,9,true);
        REQUIRE("{[1,5],(8,9]}" == print(x));
        x.add(11,false,12,true);
        REQUIRE("{[1,5],(8,9],(11,12]}" == print(x));
        x.add(13,false,14,true);
        REQUIRE("{[1,5],(8,9],(11,12],(13,14]}" == print(x));
        x.add(10,true,11,false);
        REQUIRE("{[1,5],(8,9],[10,11),(11,12],(13,14]}" == print(x));
        x.add(9,true,11,true);
        REQUIRE("{[1,5],(8,12],(13,14]}" == print(x));
        x.add(0,true,13,false);
        REQUIRE("{[0,13),(13,14]}" == print(x));
        x.add(-1,true,42,false);
        REQUIRE("{[-1,42)}" == print(x));
        x.add(42,true,43,true);
        REQUIRE("{[-1,43]}" == print(x));
    }

    SECTION("remove") {
        IS x;
        x.add(1,true,50,true);
        REQUIRE("{[1,50]}" == print(x));
        x.remove(1,true,2,true);
        REQUIRE("{(2,50]}" == print(x));
        x.remove(49,false,50,true);
        REQUIRE("{(2,49]}" == print(x));
        x.remove(5,true,6,false);
        REQUIRE("{(2,5),[6,49]}" == print(x));
        x.remove(8,false,9,true);
        REQUIRE("{(2,5),[6,8],(9,49]}" == print(x));
        IS a(x), b(x), c(x);
        a.remove(2,false,13,true);
        REQUIRE("{(13,49]}" == print(a));
        b.remove(4,false,8,true);
        REQUIRE("{(2,4],(9,49]}" == print(b));
        c.remove(4,false,13,true);
        REQUIRE("{(2,4],(13,49]}" == print(c));
    }

    SECTION("contains") {
        IS x;
        x.add(5,true,10,false);
        x.add(1,true,4,false);
        REQUIRE( x.contains({{5,true},{10,false}}));
        REQUIRE(!x.contains({{5,true},{10,true}}));
        REQUIRE( x.contains({{7,true},{8,true}}));
        REQUIRE( x.contains({{5,false},{6,false}}));
    }

    SECTION("intersects") {
        IS x;
        x.add(1,true,4,false);
        x.add(5,true,10,false);
        x.add(11,false,12,false);
        REQUIRE( x.intersects({{5,true},{10,false}}));
        REQUIRE( x.intersects({{5,true},{10,true}}));
        REQUIRE( x.intersects({{7,true},{8,true}}));
        REQUIRE(!x.intersects({{10,true},{11,true}}));
        REQUIRE( x.intersects({{10,true},{12,true}}));
        REQUIRE( x.intersects({{2,true},{7,true}}));
        REQUIRE( x.intersects({{4,true},{7,true}}));
        REQUIRE(!x.intersects({{0,true},{1,false}}));
        REQUIRE( x.intersects({{0,true},{1,true}}));
        REQUIRE(!x.intersects({{12,true},{13,true}}));
    }

    SECTION("intersect") {
        IS x, y;
        x.add({{2,true},{8,false}});
        x.add({{9,false},{13,true}});

        y.add({{1,true},{3,true}});
        y.add({{4,false},{5,true}});
        y.add({{7,true},{10,false}});
        y.add({{11,true},{12,false}});
        REQUIRE("{[2,3],(4,5],[7,8),(9,10),[11,12)}" == print(x.intersect(y)));
        REQUIRE("{[2,3],(4,5],[7,8),(9,10),[11,12)}" == print(y.intersect(x)));
    }

    SECTION("difference") {
        IS x, y;
        x.add({{2,true},{8,false}});
        x.add({{9,false},{13,true}});

        y.add({{1,true},{3,true}});
        y.add({{4,false},{5,true}});
        y.add({{7,true},{10,false}});
        y.add({{11,true},{12,false}});
        REQUIRE("{(3,4],(5,7),[10,11),[12,13]}" == print(x.difference(y)));
        REQUIRE("{[1,2),[8,9]}" == print(y.difference(x)));
    }
}

} } // namespace Test Gringo

