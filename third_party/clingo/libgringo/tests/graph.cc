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

#include "gringo/graph.hh"
#include "gringo/utility.hh"

#include "tests/tests.hh"

namespace Gringo { namespace Test {

using namespace Gringo::IO;

namespace {

typedef Graph<std::string> G;

std::ostream &operator<<(std::ostream &out, G::SCCVec const &x) {
    auto f = [](std::ostream &out, G::NodeVec const x) {
        auto f = [](std::ostream &out, G::Node const *x) { out << x->data; };
        out << "[";
        print_comma(out, x, ",", f);
        out << "]";
    };
    out << "[";
    print_comma(out, x, ",", f);
    out << "]";
    return out;
}

std::string to_string(G::SCCVec const &x) {
    std::ostringstream out;
    out << x;
    return out.str();
}

G graph(std::initializer_list<std::pair<std::string, std::string>> edges)
{
    G g;
    std::map<std::string, G::Node*> map;
    for (auto &x : edges) {
        auto &a = map[x.first];
        if (a == nullptr) { a = &g.insertNode(x.first); }
        auto &b = map[x.second];
        if (b == nullptr) { b = &g.insertNode(x.second); }
        a->insertEdge(*b);
    }
    return g;
}

} // namespace

TEST_CASE("graph", "[base]") {
    SECTION("test_tarjan") {
        REQUIRE("[[b,c],[d],[a]]" == to_string(graph({{"a","b"},{"a","d"},{"b","c"},{"c","b"},{"d","b"}}).tarjan()));
        REQUIRE("[[h,i],[f,g],[d,c,b,e],[a]]" == to_string(graph({{"a","b"},{"b","c"},{"c","h"},{"c","d"},{"d","e"},{"e","f"},{"e","b"},{"e","c"},{"f","g"},{"g","f"},{"h","i"},{"i","h"}}).tarjan()));
        REQUIRE("[[c,d],[a,b]]" == to_string(graph({{"a","b"},{"a","c"},{"a","d"},{"b","a"},{"c","d"},{"d","c"}}).tarjan()));
        REQUIRE("[[i],[n],[r3],[r4,e,r1,p,r2,s]]" == to_string(graph({{"r1","e"},{"r2","p"},{"r2","i"},{"r3","n"},{"r3","i"},{"r4","s"},{"p","r1"},{"s","r2"},{"s","r3"},{"e","r4"}}).tarjan()));
        G g{graph({{"a","b"},{"a","d"},{"b","c"},{"c","b"},{"d","b"}})};
        REQUIRE("[[b,c],[d],[a]]" == to_string(g.tarjan()));
        REQUIRE("[[b,c],[d],[a]]" == to_string(g.tarjan()));
        REQUIRE("[[b,c],[d],[a]]" == to_string(g.tarjan()));
    }
}

} } // namespace Test Gringo

