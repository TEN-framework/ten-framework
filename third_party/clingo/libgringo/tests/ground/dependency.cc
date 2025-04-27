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

#include "gringo/ground/dependency.hh"

#include "tests/tests.hh"
#include "tests/term_helper.hh"

#include <functional>

namespace Gringo { namespace Ground { namespace Test {

namespace {

using namespace Gringo::Test;
using namespace Gringo::IO;

using StringSet = std::set<std::string>;
using V = Symbol;
using S = std::string;

struct TestLookup {
    typedef Lookup<UGTerm> L;
    void add(UTerm const &x) {
        auto occ   = x->gterm();
        auto &term = *occ;
        l.add(term, std::move(occ));
    }
    void matchTerm(L::iterator begin, L::iterator end) {
        for (auto it = begin; it != end; ++it){ str.insert(to_string(*it->first)); }
    }
    std::string match(Symbol const &x) {
        str.clear();
        using namespace std::placeholders;
        l.match(x, std::bind(&TestLookup::matchTerm, this, _1, _2));
        return to_string(str);
    }
    std::string unify(UTerm const &x) {
        str.clear();
        auto term(x->gterm());
        using namespace std::placeholders;
        l.unify(*term, std::bind(&TestLookup::matchTerm, this, _1, _2));
        return to_string(str);
    }
    StringSet str;
    L l;
};

struct TestBodyOccurrence : BodyOccurrence<S> {
    TestBodyOccurrence(unsigned num, UTerm &&term, bool positive = true) : num(num), term(std::move(term)), positive(positive) { }
    virtual UGTerm getRepr() const  { return term->gterm(); }
    virtual bool isPositive() const { return positive; }
    virtual bool isNegative() const { return !positive; }
    virtual void setType(OccurrenceType x)    { type = x; }
    virtual OccurrenceType getType() const { return type; }
    void print(std::ostream &out) const {
        out << *term << "@" << num << ":";
        switch (type) {
            case OccurrenceType::POSITIVELY_STRATIFIED: { out << "!"; break; }
            case OccurrenceType::STRATIFIED:            { out << "."; break; }
            case OccurrenceType::UNSTRATIFIED:          { out << "?"; break; }
        }
    }
    virtual DefinedBy &definedBy() { return defs; }
    virtual void checkDefined(LocSet &, SigSet const &, UndefVec &) const { }
    virtual ~TestBodyOccurrence() { }
    unsigned num;
    UTerm term;
    OccurrenceType type = OccurrenceType::UNSTRATIFIED;
    bool positive;
    DefinedBy defs;
};

struct TestDep {
    Dependency<S,S>::Node &add(S const &name, bool normal) { return dep.add(S(name), normal); }
    void provides(Dependency<S,S>::Node &x, UTerm &&y) {
        heads.emplace_front(to_string(*y));
        dep.provides(x, heads.front(), y->gterm());
    }
    void depends(Dependency<S,S>::Node &x, unsigned num, UTerm &&y, bool positive = true) {
        occs.emplace_front(num, std::move(y), positive);
        dep.depends(x, occs.front());
    }
    S analyze() {
        auto sccs(std::get<0>(dep.analyze()));
        for (auto &component : sccs) { std::sort(component.first.begin(), component.first.end()); }
        std::ostringstream ss;
        ss << "(" << sccs << ",[";
        print_comma(ss, occs, ",", [](std::ostream &out, TestBodyOccurrence const &x) { x.print(out); });
        ss << "])";
        return ss.str();
    }

    std::forward_list<S> heads;
    std::forward_list<TestBodyOccurrence> occs;
    Dependency<S,S> dep;
};

} // namespace

TEST_CASE("ground-dependency", "[ground]") {
    SECTION("match") {
        TestLookup l;
        l.add(fun("f", val(NUM(2))));
        REQUIRE("{f(2)}" == l.match(V::createFun("f", { NUM(2) })));
        l.add(fun("f", var("X")));
        REQUIRE("{f(Y0)}" == l.match(V::createFun("f", { NUM(1) })));
        REQUIRE("{}" == l.match(V::createFun("g", { NUM(1) })));
        l.add(fun("f", var("X"), var("X")));
        l.add(fun("f", var("X"), var("Y")));
        REQUIRE("{f(Y0,Y0),f(Y0,Y1)}" == l.match(V::createFun("f", { NUM(1), NUM(1) })));
        REQUIRE("{f(Y0,Y1)}" == l.match(V::createFun("f", { NUM(1), NUM(2) })));
        l.add(fun("g", lin("X", 3, 7)));
        REQUIRE("{}" == l.match(V::createFun("g", { NUM(2) })));
        REQUIRE("{g((3*Y0+7))}" == l.match(V::createFun("g", { NUM(10) })));
    }

    SECTION("unify") {
        TestLookup l;
        l.add(fun("f", var("X")));
        l.add(fun("f", fun("g", var("X"))));
        REQUIRE("{f(Y0),f(g(Y0))}" == l.unify(fun("f", var("Y"))));
        REQUIRE("{f(Y0),f(g(Y0))}" == l.unify(fun("f", fun("g", var("X")))));
        l.add(fun("f", var("X"), var("X")));
        REQUIRE("{}" == l.unify(fun("f", var("A"), fun("f", var("A")))));
        REQUIRE("{f(Y0,Y0)}" == l.unify(fun("f", var("A"), fun("f", var("B")))));
        l.add(fun("f", val(V::createFun("g", { NUM(1) })), var("X")));
        REQUIRE("{f(Y0,Y0),f(g(1),Y0)}" == l.unify(fun("f", var("A"), val(V::createFun("g", { NUM(2) })))));
    }

    SECTION("dep1") {
        TestDep dep;
        auto &x(dep.add("x.", true));
        dep.provides(x, val(ID("x")));
        auto &y(dep.add("a:-b@1,x@1,y@1.", true));
        dep.provides(y, val(ID("a")));
        dep.depends(y, 1, val(ID("b")));
        dep.depends(y, 1, val(ID("x")));
        dep.depends(y, 1, val(ID("y")));
        auto &z(dep.add("b:-a@1.", true));
        dep.provides(z, val(ID("b")));
        dep.depends(z, 1, val(ID("a")));
        auto &u(dep.add("c:-a@2,b@2.", true));
        dep.provides(u, val(ID("c")));
        dep.depends(u, 2, val(ID("a")));
        dep.depends(u, 2, val(ID("b")));
        REQUIRE("([([x.],1),([a:-b@1,x@1,y@1.,b:-a@1.],1),([c:-a@2,b@2.],1)],[b@2:!,a@2:!,a@1:?,y@1:!,x@1:!,b@1:?])" == dep.analyze());
    }

    SECTION("dep2") {
        TestDep dep;
        auto &x(dep.add("a:-~b@1.", true));
        dep.provides(x, val(ID("a")));
        dep.depends(x, 1, val(ID("b")), false);
        auto &y(dep.add("b:-~a@1,a@2.", true));
        dep.provides(y, val(ID("b")));
        dep.depends(y, 1, val(ID("a")), false);
        dep.depends(y, 2, val(ID("a")));
        REQUIRE("([([a:-~b@1.],0),([b:-~a@1,a@2.],0)],[a@2:.,a@1:.,b@1:?])" == dep.analyze());
    }

    SECTION("dep3") {
        TestDep dep;
        auto &x(dep.add("{a}.", false));
        dep.provides(x, val(ID("a")));
        auto &y(dep.add("b:-a@1.", true));
        dep.provides(y, val(ID("b")));
        dep.depends(y, 1, val(ID("a")));
        REQUIRE("([([{a}.],0),([b:-a@1.],0)],[a@1:.])" == dep.analyze());
    }

    SECTION("dep4") {
        TestDep dep;
        auto &x(dep.add("a:-~b@1.", true));
        dep.provides(x, val(ID("a")));
        dep.depends(x, 1, val(ID("b")), false);
        auto &y(dep.add("a:-~c@1.", true));
        dep.provides(y, val(ID("a")));
        dep.depends(y, 1, val(ID("c")), false);

        auto &b(dep.add("b:-~a@1.", true));
        dep.provides(b, val(ID("b")));
        dep.depends(b, 1, val(ID("a")));
        auto &c(dep.add("c:-~a@2.", true));
        dep.provides(c, val(ID("c")));
        dep.depends(c, 2, val(ID("a")));
        REQUIRE("([([a:-~b@1.],0),([a:-~c@1.],0),([c:-~a@2.],0),([b:-~a@1.],0)],[a@2:.,a@1:.,c@1:?,b@1:?])" == dep.analyze());
    }
}

} } } // namespace Test Ground Gringo

