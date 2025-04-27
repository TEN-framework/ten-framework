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

#include <cmath>
#include <cstddef>

#include <math/wide_integer/uintwide_t.h>

#include "gringo/term.hh"
#include "gringo/base.hh"
#include "gringo/logger.hh"
#include "gringo/graph.hh"

// #define CLINGO_DEBUG_INEQUALITIES

namespace Gringo {

using slack_t = math::wide_integer::int128_t;

// {{{ definition of Defines

Defines::DefMap const &Defines::defs() const {
    return defs_;
}

void Defines::add(Location const &loc, String name, UTerm &&value, bool defaultDef, Logger &log) {
    auto it = defs_.find(name);
    if (it == defs_.end()) {
        defs_.emplace(name, make_tuple(defaultDef, loc, std::move(value)));
    }
    else if (std::get<0>(it->second) && !defaultDef) {
        it->second = make_tuple(defaultDef, loc, std::move(value));
    }
    else if (std::get<0>(it->second) || !defaultDef) {
        GRINGO_REPORT(log, Warnings::RuntimeError)
            << loc << ": error: redefinition of constant:\n"
            << "  #const " << name << "=" << *value << ".\n"
            << std::get<1>(it->second) << ": note: constant also defined here\n";
    }
}

void Defines::init(Logger &log) {
    using DefineGraph = Graph<Defines::DefMap::iterator>;
    using NodeMap     = std::unordered_map<String, DefineGraph::Node*>;

    DefineGraph graph;
    NodeMap nodes;
    for (auto it = defs_.begin(), end = defs_.end(); it != end; ++it) {
        nodes.emplace(it->first, &graph.insertNode(it));
    }
    for (auto &x : nodes) {
        Term::VarSet vals;
        std::get<2>(x.second->data->second)->collectIds(vals);
        for (const auto &y : vals) {
            auto it = nodes.find(y);
            if (it != nodes.end()) {
                x.second->insertEdge(*it->second);
            }
        }
    }
    for (auto &scc : graph.tarjan()) {
        if (scc.size() > 1) {
            std::ostringstream msg;
            msg
                << std::get<1>(scc.back()->data->second) << ": error: cyclic constant definition:\n"
                << "  #const " << scc.back()->data->first << "=" << *std::get<2>(scc.back()->data->second) << ".\n";
            scc.pop_back();
            for (auto &x : scc) {
                msg
                    << std::get<1>(x->data->second) << ": note: cycle involves definition:\n"
                    << "  #const " << x->data->first << "=" << *std::get<2>(x->data->second) << ".\n";
            }
            GRINGO_REPORT(log, Warnings::RuntimeError) << msg.str();
        }
        for (auto &x : scc) {
            Term::replace(std::get<2>(x->data->second), std::get<2>(x->data->second)->replace(*this, true));
        }
    }
}

bool Defines::empty() const {
    return defs_.empty();
}

void Defines::apply(Symbol x, Symbol &retVal, UTerm &retTerm, bool replace) {
    if (x.type() == SymbolType::Fun) {
        if (x.sig().arity() > 0) {
            SymVec args;
            bool changed = true;
            for (std::size_t i = 0, e = x.args().size; i != e; ++i) {
                UTerm rt;
                args.emplace_back();
                apply(x.args()[i], args.back(), rt, true);
                if (rt) {
                    Location loc{rt->loc()};
                    UTermVec tArgs;
                    args.pop_back();
                    for (auto &y : args) {
                        tArgs.emplace_back(make_locatable<ValTerm>(rt->loc(), y));
                    }
                    tArgs.emplace_back(std::move(rt));
                    for (++i; i != e; ++i) {
                        Symbol rv;
                        tArgs.emplace_back();
                        apply(x.args()[i], rv, tArgs.back(), true);
                        if (!tArgs.back()) {
                            if (rv.type() == SymbolType::Special) {
                                rv = x.args()[i];
                            }
                            tArgs.back() = make_locatable<ValTerm>(loc, rv);
                        }
                    }
                    retTerm = make_locatable<FunctionTerm>(loc, x.name(), std::move(tArgs));
                    return;
                }
                if (args.back().type() == SymbolType::Special) {
                    args.back() = x.args()[i];
                }
                else {
                    changed = true;
                }
            }
            if (changed) {
                retVal = Symbol::createFun(x.name(), Potassco::toSpan(args));
            }
        }
        else if (replace) {
            auto it(defs_.find(x.name()));
            if (it != defs_.end()) {
                retVal = std::get<2>(it->second)->isEDB();
                if (retVal.type() == SymbolType::Special) {
                    retTerm = get_clone(std::get<2>(it->second));
                }
            }
        }
    }
}

// }}}

// {{{ definition of IESolver

namespace {

slack_t floordiv(slack_t n, slack_t m) {
    auto a = n / m;
    auto b = n % m;
    if (((n < 0) != (m < 0)) && b != 0) {
        --a;
    }
    return a;
}

slack_t ceildiv(slack_t n, slack_t m) {
    auto a = n / m;
    auto b = n % m;
    if (((n < 0) != (m < 0)) && b != 0) {
        ++a;
    }
    return a;
}

template<typename I>
I div(bool positive, I a, I b) {
    return positive ? floordiv(a, b) : ceildiv(a, b);
}

template<typename I>
int clamp(I a) {
    if (a > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (a < std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(a);

}

int clamp_div(bool positive, slack_t a, int64_t b) {
    if (a == std::numeric_limits<slack_t>::min() && b == -1) {
        return std::numeric_limits<int>::max();
    }
    return clamp(div<slack_t>(positive, a, b));
}

template<class It, class Merge>
It merge_adjancent(It first, It last, Merge m) {
    if (first == last) {
        return last;
    }

    auto result = first;
    while (++first != last) {
        if (!m(*result, *first) && ++result != first) {
            *result = std::move(*first);
        }
    }
    return ++result;
}

bool update_bound(IEBoundMap &bounds, IETerm const &term, slack_t slack, int num_unbounded) {
    bool positive = term.coefficient > 0;
    auto type = positive ? IEBound::Upper : IEBound::Lower;
    if (num_unbounded == 0) {
        slack += static_cast<slack_t>(term.coefficient) * bounds[term.variable].get(type);
    }
    else if (num_unbounded > 1 || bounds[term.variable].isSet(type)) {
        return false;
    }

    auto value = clamp_div(positive, slack, term.coefficient);
    return bounds[term.variable].refine(positive ? IEBound::Lower : IEBound::Upper, value);
}

void update_slack(IEBoundMap &bounds, IETerm const &term, slack_t &slack, int &num_unbounded) {
    auto type = term.coefficient > 0 ? IEBound::Upper : IEBound::Lower;
    if (bounds[term.variable].isSet(type)) {
        slack -= static_cast<slack_t>(term.coefficient) * bounds[term.variable].get(type);
    }
    else {
        ++num_unbounded;
    }
}

#ifdef CLINGO_DEBUG_INEQUALITIES

std::ostream &operator<<(std::ostream &out, IEBound const &bound) {
    out << "[";
    if (bound.isSet(IEBound::Lower)) {
        out << bound.get(IEBound::Lower);
    }
    else {
        out << "-inf";
    }
    out << ",";
    if (bound.isSet(IEBound::Upper)) {
        out << bound.get(IEBound::Upper);
    }
    else {
        out << "+inf";
    }
    out << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, IE const &ie) {
    bool comma = false;
    if (ie.terms.empty()) {
        out << "0";
    }
    for (auto const &term : ie.terms) {
        if (comma) {
            out << " + ";
        }
        comma = true;
        out << term.coefficient << "*" << term.variable->name;
    }
    out << " >= " << ie.bound;
    return out;
}

#endif

} // namespace

bool IEBound::isSet(Type type) const {
    return type == Lower ? hasLower_ : hasUpper_;
}

int IEBound::get(Type type) const {
    return type == Lower ? lower_ : upper_;
}

void IEBound::set(Type type, int bound) {
    if (type == Lower) {
        hasLower_ = true;
        lower_ = bound;
    }
    else {
        hasUpper_ = true;
        upper_ = bound;
    }
}

bool IEBound::refine(Type type, int bound) {
    if (!isSet(type)) {
        set(type, bound);
        return true;
    }
    if (type == Lower && bound > lower_) {
        lower_ = bound;
        return true;
    }
    if (type == Upper && bound < upper_) {
        upper_ = bound;
        return true;
    }
    return false;
}

bool IEBound::refine(IEBound const &bound) {
    bool ret = false;
    if (bound.isSet(Lower)) {
        ret = refine(Lower, bound.get(Lower)) || ret;
    }
    if (bound.isSet(Upper)) {
        ret = refine(Upper, bound.get(Upper)) || ret;
    }
    return ret;
}

bool IEBound::isBounded() const {
    return hasLower_ && hasUpper_;
}

bool IEBound::isImproving(IEBound const &other) const {
    if (!other.isBounded() || !isBounded()) {
        return false;
    }
    return other.lower_ < lower_ || upper_ < other.upper_;
}

bool VarTermCmp::operator()(VarTerm const *a, VarTerm const *b) const {
    return a->name < b->name;
}

bool IESolver::isImproving(VarTerm const *var, IEBound const &bound) const {
    auto it = fixed_.find(var);
    if (it == fixed_.end()) {
        return bound.isBounded();
    }
    return bound.isImproving(it->second);
}

void IESolver::add(IE ie, bool ignoreIfFixed) {
    auto &terms = ie.terms;

    // remove terms not associated with a variable
    auto last = std::partition(
        terms.begin(), terms.end(),
        [](auto &term) {
            return term.variable != nullptr && term.coefficient != 0;
        });
    for (auto end = terms.end(), current = last; current != end; ++current) {
        ie.bound -= current->coefficient;
    }
    terms.erase(last, terms.end());

    // sort according to variables
    std::sort(
        terms.begin(), terms.end(),
        [](auto const &a, auto const &b) {
            return a.variable->name < b.variable->name;
        });

    // combine adjacent terms referring to the same variable
    terms.erase(merge_adjancent(
        terms.begin(), terms.end(),
        [](auto &a, auto &b) {
            if (a.variable->name == b.variable->name) {
                a.coefficient += b.coefficient;
                return true;
            }
            return false;
        }), terms.end());
    ies_.emplace_back(std::move(ie));

    if (ies_.back().terms.size() == 1 && ignoreIfFixed) {
        auto term = ies_.back().terms.back();
        if (term.coefficient == 1) {
            fixed_[term.variable].refine(IEBound::Lower, clamp(ies_.back().bound));
        }
        else if (term.coefficient == -1) {
            fixed_[term.variable].refine(IEBound::Upper, clamp(-ies_.back().bound));
        }
    }
}

void IESolver::compute() {
#ifdef CLINGO_DEBUG_INEQUALITIES
    for (auto &ie : ies_) {
        std::cerr << ie << std::endl;
    }
#endif
    // initialize bound computation and incorporate bounds from parent
    bounds_.clear();
    ctx_.gatherIEs(*this);
    if (parent_ != nullptr) {
        for (auto &bound : parent_->bounds_) {
            fixed_[bound.first].refine(bound.second);
            bounds_[bound.first].refine(bound.second);
        }
    }

    // compute bounds
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto const &ie : ies_) {
            slack_t slack = ie.bound;
            int num_unbounded = 0;
            for (auto const &term : ie.terms) {
                // In case the slack cannot be updated due to an overflow, no bounds are calculated.
                update_slack(bounds_, term, slack, num_unbounded);
            }
            if (num_unbounded == 0 && slack > 0) {
                // we simply set all bounds to empty intervals
                for (auto const &ie : ies_) {
                    for (auto const &term : ie.terms) {
                        bounds_[term.variable].set(IEBound::Lower, 1);
                        bounds_[term.variable].set(IEBound::Upper, 0);
                    }
                }
                changed = false;
                break;
            }
            if (num_unbounded <= 1) {
                for (auto const &term : ie.terms) {
                    if (update_bound(bounds_, term, slack, num_unbounded)) {
#ifdef CLINGO_DEBUG_INEQUALITIES
                        std::cerr << "  update bound using " << ie << std::endl;
                        std::cerr << "    the new bound for " << *term.variable << " is "<< bounds_[term.variable] << std::endl;
#endif
                        changed = true;
                    }
                }
            }
        }
    }

    // add computed bounds and then compute bounds for nested scopes
    for (auto const &bound : bounds_) {
        if (isImproving(bound.first, bound.second)) {
            ctx_.addIEBound(*bound.first, bound.second);
        }
    }
    for (auto &solver : subSolvers_) {
        solver.compute();
    }
}

void IESolver::add(IEContext &context) {
    subSolvers_.emplace_front(context, this);
}

// }}}

// {{{ definition of GRef

GRef::GRef(UTerm &&name)
    : type(EMPTY)
    , name(std::move(name))
    , value(Symbol::createNum(0))
    , term(nullptr) { }

GRef::operator bool() const {
    return type != EMPTY;
}

void GRef::reset() {
    type = EMPTY;
}

GRef &GRef::operator=(Symbol const &x) {
    type  = VALUE;
    value = x;
    return *this;
}

GRef &GRef::operator=(GTerm &x) {
    type = TERM;
    term = &x;
    return *this;
}

bool GRef::occurs(GRef &x) const {
    switch (type) {
        case VALUE: { return false; }
        case TERM:  { return term->occurs(x); }
        case EMPTY: { return this == &x; }
    }
    assert(false);
    return false;
}

bool GRef::match(Symbol const &x) const {
    switch (type) {
        case VALUE: { return value == x; }
        case TERM:  { return term->match(x); }
        case EMPTY: { assert(false); }
    }
    return false;
}

template <class T>
bool GRef::unify(T &x) {
    switch (type) {
        case VALUE: { return x.match(value); }
        case TERM:  { return term->unify(x); }
        case EMPTY: { assert(false); }
    }
    return false;
}

// }}}

// {{{1 definition of GValTerm

GValTerm::GValTerm(Symbol val)
: val_(val) { }

bool GValTerm::operator==(GTerm const &other) const {
    const auto *t = dynamic_cast<GValTerm const*>(&other);
    return (t != nullptr) && val_ == t->val_;
}

size_t GValTerm::hash() const {
    return get_value_hash(typeid(GValTerm).hash_code(), val_);
}

void GValTerm::print(std::ostream &out) const {
    out << val_;
}

Sig GValTerm::sig() const {
    return val_.sig();
}

GTerm::EvalResult GValTerm::eval() const {
    return {true, val_};
}

bool GValTerm::occurs(GRef &x) const {
    return false;
}

void GValTerm::reset() { }

bool GValTerm::match(Symbol const &x) {
    return val_ == x;
}

bool GValTerm::unify(GTerm &x) {
    return x.match(val_);
}

bool GValTerm::unify(GFunctionTerm &x) {
    return x.match(val_);
}

bool GValTerm::unify(GLinearTerm &x) {
    return x.match(val_);
}

bool GValTerm::unify(GVarTerm &x) {
    return x.match(val_);
}

// {{{1 definition of GFunctionTerm

GFunctionTerm::GFunctionTerm(String name, UGTermVec &&args)
: sign_(false)
, name_(name)
, args_(std::move(args)) { }

// Note: uses structural comparisson of names of variable terms (VarTerm/LinearTerm)
bool GFunctionTerm::operator==(GTerm const &other) const {
    const auto *t = dynamic_cast<GFunctionTerm const*>(&other);
    return t != nullptr &&
           sig() == t->sig() &&
           is_value_equal_to(args_, t->args_);
}

size_t GFunctionTerm::hash() const {
    return get_value_hash(typeid(GFunctionTerm).hash_code(), sig(), args_);
}

void GFunctionTerm::print(std::ostream &out) const {
    if ((sig()).sign()) {
        out << "-";
    }
    out << name_;
    out << "(";
    print_comma(out, args_, ",", [](std::ostream &out, UGTerm const &x) { out << *x; });
    out << ")";
}

Sig GFunctionTerm::sig() const {
    return {name_, numeric_cast<uint32_t>(args_.size()), sign_};
}

GTerm::EvalResult GFunctionTerm::eval() const {
    return {false, Symbol()};
}

bool GFunctionTerm::occurs(GRef &x) const {
    for (const auto &y : args_) {
        if (y->occurs(x)) {
            return true;
        }
    }
    return false;
}

void GFunctionTerm::reset() {
    for (auto &y : args_) {
        y->reset();
    }
}

bool GFunctionTerm::match(Symbol const &x) {
    if (x.type() != SymbolType::Fun || sig() != x.sig()) {
        return false;
    }
    auto i = 0;
    for (auto &y : args_) {
        if (!y->match(x.args()[i++])) {
            return false;
        }
    }
    return true;

}

bool GFunctionTerm::unify(GTerm &x) {
    return x.unify(*this);
}

bool GFunctionTerm::unify(GFunctionTerm &x) {
    if (sig() != x.sig()) {
        return false;
    }
    for (auto it = args_.begin(), jt = x.args_.begin(), ie = args_.end(); it != ie; ++it, ++jt) {
        if (!(*it)->unify(**jt)) {
            return false;
        }
    }
    return true;
}

bool GFunctionTerm::unify(GLinearTerm &x) {
    static_cast<void>(x);
    return false;
}

bool GFunctionTerm::unify(GVarTerm &x) {
    if (*x.ref) {
        return x.ref->unify(*this);
    }
    if (!occurs(*x.ref)) {
        *x.ref = *this;
        return true;
    }
    return false;
}

// {{{1 definition of GLinearTerm

GLinearTerm::GLinearTerm(const SGRef& ref, int m, int n)
: ref_(ref), m_(m), n_(n) {
    assert(ref);
}

bool GLinearTerm::operator==(GTerm const &other) const {
    const auto *t = dynamic_cast<GLinearTerm const*>(&other);
    return t != nullptr &&
           *ref_->name == *t->ref_->name &&
           m_ == t->m_ &&
           n_ == t->n_;
}

size_t GLinearTerm::hash() const {
    return get_value_hash(typeid(GLinearTerm).hash_code(), ref_->name, m_, n_);
}

void GLinearTerm::print(std::ostream &out) const {
    out << "(" << m_ << "*" << *ref_->name << "+" << n_ << ")";
}

Sig GLinearTerm::sig() const {
    throw std::logic_error("must not be called");
}

GTerm::EvalResult GLinearTerm::eval() const {
    return {false, Symbol()};
}

bool GLinearTerm::occurs(GRef &x) const {
    return ref_->occurs(x);
}

void GLinearTerm::reset() {
    ref_->reset();
}

bool GLinearTerm::match(Symbol const &x) {
    if (x.type() != SymbolType::Num) {
        return false;
    }
    int y = x.num();
    y-= n_;
    if (y % m_ != 0) {
        return false;
    }
    y /= m_;
    if (*ref_) {
        return ref_->match(Symbol::createNum(y));
    }
    *ref_ = Symbol::createNum(y);
    return true;
}

bool GLinearTerm::unify(GTerm &x) {
    return x.unify(*this);
}

bool GLinearTerm::unify(GLinearTerm &x) {
    static_cast<void>(x);
    // Note: more could be done but this would be somewhat involved
    //       because this would require rational numbers
    //       as of now this simply unifies too much
    return true;
}

bool GLinearTerm::unify(GFunctionTerm &x) {
    return false;
}

bool GLinearTerm::unify(GVarTerm &x) {
    if (*x.ref) {
        return x.ref->unify(*this);
    }
    // see not at: GLinearTerm::unify(GLinearTerm &x)
    return true;

}

// {{{1 definition of GVarTerm

GVarTerm::GVarTerm(const SGRef& ref)
: ref(ref) {
    assert(ref);
}

bool GVarTerm::operator==(GTerm const &other) const {
    const auto *t = dynamic_cast<GVarTerm const*>(&other);
    return (t != nullptr) && *ref->name == *t->ref->name;
}

size_t GVarTerm::hash() const {
    return get_value_hash(typeid(GVarTerm).hash_code(), ref->name);
}

void GVarTerm::print(std::ostream &out) const {
    out << *ref->name;
}

Sig GVarTerm::sig() const {
    throw std::logic_error("must not be called");
}

GTerm::EvalResult GVarTerm::eval() const {
    return {false, Symbol()};
}

bool GVarTerm::occurs(GRef &x) const {
    return ref->occurs(x);
}

void GVarTerm::reset() {
    ref->reset();
}

bool GVarTerm::match(Symbol const &x) {
    if (*ref) {
        return ref->match(x);
    }
    *ref = x;
    return true;

}

bool GVarTerm::unify(GTerm &x) {
    return x.unify(*this);
}

bool GVarTerm::unify(GLinearTerm &x) {
    if (*ref) {
        return ref->unify(x);
    }
    // see note at: GLinearTerm::unify(GLinearTerm &x)
    return true;
}

bool GVarTerm::unify(GFunctionTerm &x) {
    if (*ref) {
        return ref->unify(x);
    }
    if (!x.occurs(*ref)) {
        *ref = x;
        return true;
    }
    return false;
}

bool GVarTerm::unify(GVarTerm &x) {
    if (*ref) {
        return ref->unify(x);
    }
    if (*x.ref) {
        return x.ref->unify(*this);
    }
    if (ref->name != x.ref->name) {
        *ref = x;
    }
    return true;
}

// }}}1

// {{{1 definition of operator<< for BinOp and UnOp

int eval(UnOp op, int x) {
    switch (op) {
        case UnOp::NEG: { return -x; }
        case UnOp::ABS: { return std::abs(x); }
        case UnOp::NOT: { return ~x; }
    }
    assert(false);
    return 0;
}

std::ostream &operator<<(std::ostream &out, UnOp op) {
    switch (op) {
        case UnOp::ABS: { out << "#abs"; break; }
        case UnOp::NOT: { out << "~"; break; }
        case UnOp::NEG: { out << "-"; break; }
    }
    return out;
}

namespace {

inline int ipow(int a, int b) {
    if (b < 0) {
        return 0;
    }
    int r = 1;
    while (b > 0) {
        if((b & 1) != 0) {
            r *= a;
        }
        b >>= 1;
        a *= a;
    }
    return r;
}

void add_(IETermVec &terms, int64_t coefficient, VarTerm const *var = nullptr) {
    // NOTE: could use sorting and maybe use inequalities right away
    for (auto &term : terms) {
        if (var == term.variable || (term.variable != nullptr && var != nullptr && term.variable->name == var->name)) {
            term.coefficient += coefficient;
            return;
        }
    }
    terms.emplace_back(IETerm{coefficient, var});
}

bool toNum_(IETermVec const &terms, int64_t &res) {
    res = 0;
    for (auto const &term : terms) {
        if (term.variable == nullptr) {
            res += term.coefficient;
        }
        if (term.variable != nullptr) {
            return false;
        }
    }
    return true;
}

} // namespace

int eval(BinOp op, int x, int y) {
    switch (op) {
        case BinOp::XOR: { return x ^ y; }
        case BinOp::OR:  { return x | y; }
        case BinOp::AND: { return x & y; }
        case BinOp::ADD: { return x + y; }
        case BinOp::SUB: { return x - y; }
        case BinOp::MUL: { return x * y; }
        case BinOp::MOD: {
            assert(y != 0 && "must be checked before call");
            return x % y;
        }
        case BinOp::POW: { return ipow(x, y); }
        case BinOp::DIV: {
            assert(y != 0 && "must be checked before call");
            return x / y;
        }
    }
    assert(false);
    return 0;
}

int Term::toNum(bool &undefined, Logger &log) {
    bool undefined_arg = false;
    Symbol y(eval(undefined_arg, log));
    if (y.type() == SymbolType::Num) {
        undefined = undefined || undefined_arg;
        return y.num();
    }
    if (!undefined_arg) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << loc() << ": info: number expected:\n"
            << "  " << *this << "\n";
    }
    undefined = true;
    return 0;
}

std::ostream &operator<<(std::ostream &out, BinOp op) {
    switch (op) {
        case BinOp::AND: { out << "&"; break; }
        case BinOp::OR:  { out << "?"; break; }
        case BinOp::XOR: { out << "^"; break; }
        case BinOp::POW: { out << "**"; break; }
        case BinOp::ADD: { out << "+"; break; }
        case BinOp::SUB: { out << "-"; break; }
        case BinOp::MUL: { out << "*"; break; }
        case BinOp::DIV: { out << "/"; break; }
        case BinOp::MOD: { out << "\\"; break; }
    }
    return out;
}

void addIETerm(IETermVec &terms, IETerm const &term) {
    add_(terms, term.coefficient, term.variable);
}

void subIETerm(IETermVec &terms, IETerm const &term) {
    add_(terms, -term.coefficient, term.variable);
}

// {{{1 definition of Term and auxiliary functions

namespace {

UTerm wrap(UTerm &&x) {
    UTermVec args;
    args.emplace_back(std::move(x));
    return make_locatable<FunctionTerm>(args.front()->loc(), "#b", std::move(args));
}

} // namespace

UTermVec unpool(UTerm const &x) {
    UTermVec pool;
    x->unpool(pool);
    return pool;
}

SGRef Term::_newRef(RenameMap &names, Term::ReferenceMap &refs) const {
    UTerm x(renameVars(names));
    auto &ref = refs[x.get()];
    if (!ref) {
        ref = std::make_shared<GRef>(std::move(x));
    }
    return ref;
}

UGTerm Term::gterm() const {
    RenameMap names;
    ReferenceMap refs;
    return gterm(names, refs);
}


UGFunTerm Term::gfunterm(RenameMap &names, ReferenceMap &refs) const {
    static_cast<void>(names);
    static_cast<void>(refs);
    return nullptr;
}

void Term::collect(VarTermSet &vars) const {
    VarTermBoundVec ret;
    collect(ret, false);
    for (auto &y : ret) {
        vars.emplace(*y.first);
    }
}


bool Term::isZero(Logger &log) const {
    bool undefined = false;
    return getInvertibility() == Term::CONSTANT && eval(undefined, log) == Symbol::createNum(0);
}

bool Term::bind(VarSet &bound) const {
    VarTermBoundVec occs;
    collect(occs, false);
    bool ret = false;
    for (auto &x : occs) {
        x.first->bindRef = bound.insert(x.first->name).second;
        if (x.first->bindRef) {
            ret = true;
        }
    }
    return ret;
}

UTerm Term::insert(ArithmeticsMap &arith, AuxGen &auxGen, UTerm &&term, bool eq) {
    unsigned level = term->getLevel();
    if (arith[level]->find(term) == arith[level]->end()) {
        level = numeric_cast<unsigned>(arith.size() - 1);
    }
    auto ret = arith[level]->emplace(std::move(term), nullptr);
    if (ret.second) {
        ret.first->second = auxGen.uniqueVar(ret.first->first->loc(), level, "#Arith");
    }
    if (eq) {
        auto ret2 = arith[level]->emplace(get_clone(ret.first->second), nullptr);
        if (ret2.second) {
            ret2.first->second = get_clone(ret.first->first);
        }
    }
    return get_clone(ret.first->second);
}

// {{{1 definition of AuxGen

String AuxGen::uniqueName(char const *prefix) {
    return {(prefix + std::to_string((*auxNum_)++)).c_str()};
}

UTerm AuxGen::uniqueVar(Location const &loc, unsigned level, const char *prefix) {
    return make_locatable<VarTerm>(loc, uniqueName(prefix), std::make_shared<Symbol>(), level);
}

// {{{1 definition of SimplifyState

String SimplifyState::createName(char const *prefix) {
    return gen_.uniqueName(prefix);
}

SimplifyState::SimplifyRet SimplifyState::createScript(Location const &loc, String name, UTermVec &&args, bool arith) {
    scripts_.emplace_back(gen_.uniqueVar(loc, level_, "#Script"), name, std::move(args));
    if (arith) {
        return make_locatable<LinearTerm>(loc, static_cast<VarTerm&>(*std::get<0>(scripts_.back())), 1, 0); // NOLINT
    }
    return UTerm{std::get<0>(scripts_.back())->clone()};

}

std::unique_ptr<LinearTerm> SimplifyState::createDots(Location const &loc, UTerm &&left, UTerm &&right) {
    dots_.emplace_back(gen_.uniqueVar(loc, level_, "#Range"), std::move(left), std::move(right));
    return make_locatable<LinearTerm>(loc, static_cast<VarTerm&>(*std::get<0>(dots_.back())), 1, 0); // NOLINT
}


// {{{1 definition of SimplifyState::SimplifyRet

SimplifyState::SimplifyRet::SimplifyRet(SimplifyRet &&x) noexcept
: type_(x.type_) {
    switch(type_) {
        case LINEAR:
        case REPLACE:   { x.type_ = UNTOUCHED; }
        case UNTOUCHED: {
            term_ = x.term_; // NOLINT
            break;
        }
        case UNDEFINED:
        case CONSTANT: {
            val_ = x.val_; // NOLINT
            break;
        }
    }
}

SimplifyState::SimplifyRet &SimplifyState::SimplifyRet::operator=(SimplifyRet &&x) noexcept {
    type_ = x.type_;
    switch(type_) {
        case LINEAR:
        case REPLACE:   {
            x.type_ = UNTOUCHED;
        }
        case UNTOUCHED: {
            term_ = x.term_; // NOLINT
            break;
        }
        case UNDEFINED:
        case CONSTANT: {
            val_ = x.val_; // NOLINT
            break;
        }
    }
    return *this;
}

SimplifyState::SimplifyRet::SimplifyRet(Term &x, bool project)
: type_(UNTOUCHED)
, project_(project), term_(&x) { }

SimplifyState::SimplifyRet::SimplifyRet(std::unique_ptr<LinearTerm> &&x)
: type_(LINEAR)
, term_(x.release()) { }

SimplifyState::SimplifyRet::SimplifyRet(UTerm &&x) // NOLINT
: type_(REPLACE)
, term_(x.release()) { }

SimplifyState::SimplifyRet::SimplifyRet(Symbol const &x) // NOLINT
: type_(CONSTANT)
, val_(x) { }

SimplifyState::SimplifyRet::SimplifyRet() // NOLINT
: type_(UNDEFINED) { }

bool SimplifyState::SimplifyRet::notNumeric() const {
    switch (type_) {
        case UNDEFINED: { return true; }
        case LINEAR:    { return false; }
        case CONSTANT:  { return val_.type() != SymbolType::Num; } // NOLINT
        case REPLACE:
        case UNTOUCHED: { return term_->isNotNumeric(); } // NOLINT
    }
    assert(false);
    return false;
}

bool SimplifyState::SimplifyRet::constant() const  {
    return type_ == CONSTANT;
}

bool SimplifyState::SimplifyRet::isZero() const {
    return constant() &&
           val_.type() == SymbolType::Num && // NOLINT
           val_.num() == 0; // NOLINT
}

LinearTerm &SimplifyState::SimplifyRet::lin() const {
    return static_cast<LinearTerm&>(*term_); // NOLINT
}

SimplifyState::SimplifyRet &SimplifyState::SimplifyRet::update(UTerm &x, bool arith) {
    switch (type_) {
        case CONSTANT: {
            x = make_locatable<ValTerm>(x->loc(), val_); // NOLINT
            return *this;
        }
        case LINEAR: {
            // we do not need a trivial linear if its context already requires
            // integers
            if (arith && lin().isVar()) {
                type_ = UNTOUCHED;
                x = lin().toVar();
                delete term_; // NOLINT
                return *this;
            }
        }
        case REPLACE:  {
            type_ = UNTOUCHED;
            x.reset(term_); // NOLINT
            return *this;
        }
        case UNDEFINED:
        case UNTOUCHED: { return *this; }
    }
    throw std::logic_error("SimplifyState::SimplifyRet::update: must not happen");
}

bool SimplifyState::SimplifyRet::undefined() const {
    return type_ == UNDEFINED;
}

bool SimplifyState::SimplifyRet::notFunction() const {
    switch (type_) {
        case UNDEFINED:
        case LINEAR:    { return true; }
        case CONSTANT:  { return val_.type() != SymbolType::Fun; } // NOLINT
        case REPLACE:
        case UNTOUCHED: { return term_->isNotFunction(); } // NOLINT
    }
    assert(false);
    return false;
}

SimplifyState::SimplifyRet::~SimplifyRet() noexcept {
    if (type_ == LINEAR || type_ == REPLACE) {
        delete term_; // NOLINT
    }
}

// }}}1

// {{{1 definition of PoolTerm

PoolTerm::PoolTerm(UTermVec &&terms)
: args_(std::move(terms)) { }

void PoolTerm::rename(String name) {
    static_cast<void>(name);
    throw std::logic_error("must not be called");
}

unsigned PoolTerm::getLevel() const {
    unsigned level = 0;
    for (const auto &x : args_) {
        level = std::max(x->getLevel(), level);
    }
    return level;
}

unsigned PoolTerm::projectScore() const {
    throw std::logic_error("Term::projectScore must be called after Term::unpool");
}

bool PoolTerm::isNotNumeric() const {
    return false;
}

bool PoolTerm::isNotFunction() const {
    return false;
}

Term::Invertibility PoolTerm::getInvertibility() const {
    return Term::NOT_INVERTIBLE;
}

void PoolTerm::print(std::ostream &out) const {
    print_comma(out, args_, ";", [](std::ostream &out, UTerm const &y) { out << *y; });
}

Term::SimplifyRet PoolTerm::simplify(SimplifyState &state, bool positional, bool arithmetic, Logger &log) {
    static_cast<void>(state);
    static_cast<void>(positional);
    static_cast<void>(arithmetic);
    static_cast<void>(log);
    throw std::logic_error("Term::simplify must be called after Term::unpool");
}

Term::ProjectRet PoolTerm::project(bool rename, AuxGen &auxGen) {
    static_cast<void>(rename);
    static_cast<void>(auxGen);
    throw std::logic_error("Term::project must be called after Term::unpool");
}

bool PoolTerm::hasVar() const {
    for (const auto &x : args_) {
        if (x->hasVar()) {
            return true;
        }
    }
    return false;
}

bool PoolTerm::hasPool() const {
    return true;
}

void PoolTerm::collect(VarTermBoundVec &vars, bool bound) const {
    for (const auto &y : args_) {
        y->collect(vars, bound);
    }
}

void PoolTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    for (const auto &y : args_) {
        y->collect(vars, minLevel, maxLevel);
    }
}

Symbol PoolTerm::eval(bool &undefined, Logger &log) const {
    static_cast<void>(undefined);
    static_cast<void>(log);
    throw std::logic_error("Term::unpool must be called before Term::eval");
}

bool PoolTerm::match(Symbol const &val) const {
    static_cast<void>(val);
    throw std::logic_error("Term::unpool must be called before Term::match");
}

void PoolTerm::unpool(UTermVec &x) const {
    for (const auto &t : args_) {
        t->unpool(x);
    }
}

UTerm PoolTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    static_cast<void>(arith);
    static_cast<void>(auxGen);
    static_cast<void>(forceDefined);
    throw std::logic_error("Term::rewriteArithmetics must be called before Term::rewriteArithmetics");
}

bool PoolTerm::operator==(Term const &other) const {
    const auto *t = dynamic_cast<PoolTerm const*>(&other);
    return t != nullptr &&
           is_value_equal_to(args_, t->args_);
}

size_t PoolTerm::hash() const {
    return get_value_hash(typeid(PoolTerm).hash_code(), args_);
}

PoolTerm *PoolTerm::clone() const {
    return make_locatable<PoolTerm>(loc(), get_clone(args_)).release();
}

Sig PoolTerm::getSig() const {
    throw std::logic_error("Term::getSig must not be called on PoolTerm");
}

UTerm PoolTerm::renameVars(RenameMap &names) const {
    UTermVec args;
    for (const auto &x : args_) {
        args.emplace_back(x->renameVars(names));
    }
    return make_locatable<PoolTerm>(loc(), std::move(args));
}

UGTerm PoolTerm::gterm(RenameMap &names, ReferenceMap &refs) const {
    return gringo_make_unique<GVarTerm>(_newRef(names, refs));
}

void PoolTerm::collectIds(VarSet &vars) const {
    for (const auto &y : args_) {
        y->collectIds(vars);
    }
}

UTerm PoolTerm::replace(Defines &defs, bool replace) {
    for (auto &y : args_) {
        Term::replace(y, y->replace(defs, replace));
    }
    return nullptr;
}

double PoolTerm::estimate(double size, VarSet const &bound) const {
    static_cast<void>(size);
    static_cast<void>(bound);
    return 0;
}

Symbol PoolTerm::isEDB() const {
    return {};
}

bool PoolTerm::isAtom() const {
    for (const auto &x : args_) {
        if (!x->isAtom()) {
            return false;
        }
    }
    return true;
}

bool PoolTerm::addToLinearTerm(IETermVec &terms) const {
    static_cast<void>(terms);
    throw std::logic_error("PoolTerm::addToLinearTerm must not be called");
}

// {{{1 definition of ValTerm

ValTerm::ValTerm(Symbol value)
: value_(value) { }

bool ValTerm::addToLinearTerm(IETermVec &terms) const {
    if (value_.type() == SymbolType::Num) {
        add_(terms, value_.num());
        return true;
    }
    return false;
}

unsigned ValTerm::projectScore() const {
    return 0;
}

void ValTerm::rename(String name) {
    value_ = Symbol::createId(name);
}

unsigned ValTerm::getLevel() const {
    return 0;
}

bool ValTerm::isNotNumeric() const {
    return value_.type() != SymbolType::Num;
}

bool ValTerm::isNotFunction() const {
    return value_.type() != SymbolType::Fun;
}

Term::Invertibility ValTerm::getInvertibility() const {
    return Term::CONSTANT;
}

void ValTerm::print(std::ostream &out) const {
    out << value_;
}

Term::SimplifyRet ValTerm::simplify(SimplifyState &state, bool positional, bool arithmetic, Logger &log) {
    static_cast<void>(state);
    static_cast<void>(positional);
    static_cast<void>(arithmetic);
    static_cast<void>(log);
    return {value_};
}

Term::ProjectRet ValTerm::project(bool rename, AuxGen &auxGen) {
    static_cast<void>(rename);
    static_cast<void>(auxGen);
    assert(!rename);
    return std::make_tuple(nullptr, UTerm(clone()), UTerm(clone()));
}

bool ValTerm::hasVar() const {
    return false;
}

bool ValTerm::hasPool() const {
    return false;
}

void ValTerm::collect(VarTermBoundVec &vars, bool bound) const {
    static_cast<void>(vars);
    static_cast<void>(bound);
}

void ValTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    static_cast<void>(vars);
    static_cast<void>(minLevel);
    static_cast<void>(maxLevel);
}

Symbol ValTerm::eval(bool &undefined, Logger &log) const {
    static_cast<void>(undefined);
    static_cast<void>(log);
    return value_;
}

bool ValTerm::match(Symbol const &val) const {
    return value_ == val;
}

void ValTerm::unpool(UTermVec &x) const {
    x.emplace_back(UTerm(clone()));
}

UTerm ValTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    static_cast<void>(arith);
    static_cast<void>(auxGen);
    static_cast<void>(forceDefined);
    return nullptr;
}

bool ValTerm::operator==(Term const &other) const {
    const auto *t = dynamic_cast<ValTerm const*>(&other);
    return (t != nullptr) && value_ == t->value_;
}

size_t ValTerm::hash() const {
    return get_value_hash(typeid(ValTerm).hash_code(), value_);
}

ValTerm *ValTerm::clone() const {
    return make_locatable<ValTerm>(loc(), value_).release();
}

Sig ValTerm::getSig() const {
    if (value_.type() == SymbolType::Fun) {
        return value_.sig();
    }
    throw std::logic_error("Term::getSig must not be called on ValTerm");

}

UTerm ValTerm::renameVars(RenameMap &names) const {
    static_cast<void>(names);
    return UTerm{clone()};
}

UGTerm ValTerm::gterm(RenameMap &names, ReferenceMap &refs) const {
    static_cast<void>(names);
    static_cast<void>(refs);
    return gringo_make_unique<GValTerm>(value_);
}

void ValTerm::collectIds(VarSet &vars) const {
    if (value_.type() == SymbolType::Fun && value_.sig().arity() == 0) {
        vars.emplace(value_.name());
    }
}

UTerm ValTerm::replace(Defines &defs, bool replace) {
    Symbol retVal;
    UTerm retTerm;
    defs.apply(value_, retVal, retTerm, replace);
    if (retVal.type() != SymbolType::Special) {
        value_ = retVal;
        return nullptr;
    }
    return retTerm;
}

double ValTerm::estimate(double size, VarSet const &bound) const {
    static_cast<void>(size);
    static_cast<void>(bound);
    return 0;
}

Symbol ValTerm::isEDB() const {
    return value_;
}

bool ValTerm::isAtom() const {
    return value_.type() == SymbolType::Fun;
}

// {{{1 definition of VarTerm

VarTerm::VarTerm(String name, const SVal& ref, unsigned level, bool bindRef)
: name(name)
, ref(name == "_" ? std::make_shared<Symbol>() : ref)
, bindRef(bindRef)
, level(level) {
    assert(ref || name == "_");
}

bool VarTerm::addToLinearTerm(IETermVec &terms) const {
    add_(terms, 1, this);
    return true;
}

unsigned VarTerm::projectScore() const {
    return 0;
}

void VarTerm::rename(String name) {
    static_cast<void>(name);
    throw std::logic_error("must not be called");
}

unsigned VarTerm::getLevel() const {
    return level;
}

bool VarTerm::isNotNumeric() const {
    return false;
}

bool VarTerm::isNotFunction() const {
    return false;
}

Term::Invertibility VarTerm::getInvertibility() const {
    return Term::INVERTIBLE;
}

void VarTerm::print(std::ostream &out) const {
    out << name;
}

Term::SimplifyRet VarTerm::simplify(SimplifyState &state, bool positional, bool arithmetic, Logger &log) {
    static_cast<void>(log);
    if (name == "_") {
        if (positional) {
            return {*this, true};
        }
        name = state.createName("#Anon");
    }
    if (arithmetic) {
        return {make_locatable<LinearTerm>(loc(), *this, 1, 0)};
    }
    return {*this, false};
}

Term::ProjectRet VarTerm::project(bool rename, AuxGen &auxGen) {
    assert(!rename);
    static_cast<void>(rename);
    if (name == "_") {
        UTerm r(make_locatable<ValTerm>(loc(), Symbol::createId("#p")));
        UTerm x(r->clone());
        UTerm y(auxGen.uniqueVar(loc(), 0, "#P"));
        return std::make_tuple(std::move(r), std::move(x), std::move(y));
    }
    UTerm y(auxGen.uniqueVar(loc(), 0, "#X"));
    UTerm x(wrap(UTerm(y->clone())));
    return std::make_tuple(wrap(UTerm(clone())), std::move(x), std::move(y));
}

bool VarTerm::hasVar() const {
    return true;
}

bool VarTerm::hasPool() const {
    return false;
}

void VarTerm::collect(VarTermBoundVec &vars, bool bound) const {
    vars.emplace_back(const_cast<VarTerm*>(this), bound); // NOLINT
}

void VarTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    if (minLevel <= level && level <= maxLevel) {
        vars.emplace(name);
    }
}

Symbol VarTerm::eval(bool &undefined, Logger &log) const {
    static_cast<void>(undefined);
    static_cast<void>(log);
    return *ref;
}

bool VarTerm::match(Symbol const &val) const {
    if (bindRef) {
        *ref = val;
        return true;
    }
    return val == *ref;
}

void VarTerm::unpool(UTermVec &x) const {
    x.emplace_back(UTerm(clone()));
}

UTerm VarTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    static_cast<void>(arith);
    static_cast<void>(auxGen);
    static_cast<void>(forceDefined);
    return nullptr;
}

bool VarTerm::operator==(Term const &other) const {
    const auto *t = dynamic_cast<VarTerm const*>(&other);
    return t != nullptr &&
           name == t->name &&
           level == t->level &&
           (name != "_" || t == this);
}

size_t VarTerm::hash() const {
    // NOTE: in principle ref and level could be used as identity
    //       if not for the way gterms are constructed
    //       which create arbitrary references
    return get_value_hash(typeid(VarTerm).hash_code(), name, level);
}

VarTerm *VarTerm::clone() const {
    return make_locatable<VarTerm>(loc(), name, ref, level, bindRef).release();
}

Sig VarTerm::getSig() const {
    throw std::logic_error("Term::getSig must not be called on VarTerm");
}

UTerm VarTerm::renameVars(RenameMap &names) const {
    auto ret(names.emplace(name, std::make_pair(name, nullptr)));
    if (ret.second) {
        ret.first->second.first  = ((bindRef ? "X" : "Y") + std::to_string(names.size() - 1)).c_str();
        ret.first->second.second = std::make_shared<Symbol>();
    }
    return make_locatable<VarTerm>(loc(), ret.first->second.first, ret.first->second.second, level, bindRef);
}

UGTerm VarTerm::gterm(RenameMap &names, ReferenceMap &refs) const {
    return gringo_make_unique<GVarTerm>(_newRef(names, refs));
}

void VarTerm::collectIds(VarSet &vars) const {
    static_cast<void>(vars);
}

UTerm VarTerm::replace(Defines &defs, bool replace) {
    static_cast<void>(defs);
    static_cast<void>(replace);
    return nullptr;
}

double VarTerm::estimate(double size, VarSet const &bound) const {
    return bound.find(name) == bound.end() ? size : 0.0;
}

Symbol VarTerm::isEDB() const {
    return {};
}

// {{{1 definition of LinearTerm

LinearTerm::LinearTerm(VarTerm const &var, int m, int n)
: var_(static_cast<VarTerm*>(var.clone()))
, m_(m)
, n_(n) { }

LinearTerm::LinearTerm(UVarTerm &&var, int m, int n)
: var_(std::move(var))
, m_(m)
, n_(n) { }

bool LinearTerm::addToLinearTerm(IETermVec &terms) const {
    add_(terms, m_, var_.get());
    add_(terms, n_);
    return true;
}

bool LinearTerm::isVar() const {
    return m_ == 1 && n_ == 0;
}

LinearTerm::UVarTerm LinearTerm::toVar() {
    return std::move(var_);
}

void LinearTerm::invert() {
    m_ *= -1;
    n_ *= -1;
}

void LinearTerm::add(int c) {
    n_ += c;
}

void LinearTerm::mul(int c) {
    m_ *= c;
    n_ *= c;
}

unsigned LinearTerm::projectScore() const {
    return 0;
}

void LinearTerm::rename(String name) {
    throw std::logic_error("must not be called");
}

bool LinearTerm::hasVar() const {
    return true;
}

bool LinearTerm::hasPool() const {
    return false;
}

void LinearTerm::collect(VarTermBoundVec &vars, bool bound) const {
    var_->collect(vars, bound);
}

void LinearTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    var_->collect(vars, minLevel, maxLevel);
}

unsigned LinearTerm::getLevel() const {
    return var_->getLevel();
}

bool LinearTerm::isNotNumeric() const {
    return false;
}

bool LinearTerm::isNotFunction() const {
    return true;
}

Term::Invertibility LinearTerm::getInvertibility() const {
    return Term::INVERTIBLE;
}

void LinearTerm::print(std::ostream &out) const  {
    if (m_ == 1) {
        out << "(" << *var_ << "+" << n_ << ")";
    }
    else if (n_ == 0) {
        out << "(" << m_ << "*" << *var_ << ")";
    }
    else {
        out << "(" << m_ << "*" << *var_ << "+" << n_ << ")";
    }
}

Term::SimplifyRet LinearTerm::simplify(SimplifyState &state, bool positional, bool arithmetic, Logger &log) {
    static_cast<void>(state);
    static_cast<void>(positional);
    static_cast<void>(arithmetic);
    static_cast<void>(log);
    return {*this, false};
}

Term::ProjectRet LinearTerm::project(bool rename, AuxGen &auxGen) {
    assert(!rename); static_cast<void>(rename);
    UTerm y(auxGen.uniqueVar(loc(), 0, "#X"));
    UTerm x(wrap(UTerm(y->clone())));
    return std::make_tuple(wrap(make_locatable<LinearTerm>(loc(), std::move(var_), m_, n_)), std::move(x), std::move(y));
}

Symbol LinearTerm::eval(bool &undefined, Logger &log) const {
    bool undefined_arg = false;
    Symbol value = var_->eval(undefined_arg, log);
    if (value.type() == SymbolType::Num) {
        undefined = undefined || undefined_arg;
        return Symbol::createNum(m_ * value.num() + n_);
    }
    if (!undefined_arg) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << loc() << ": info: operation undefined:\n"
            << "  " << *this << "\n";
    }
    undefined = true;
    return Symbol::createNum(0);
}

bool LinearTerm::match(Symbol const &val) const {
    if (val.type() == SymbolType::Num) {
        assert(m_ != 0);
        int c(val.num() - n_);
        if (c % m_ == 0) {
            return var_->match(Symbol::createNum(c / m_));
        }
    }
    return false;
}

void LinearTerm::unpool(UTermVec &x) const {
    x.emplace_back(UTerm(clone()));
}

UTerm LinearTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    if (forceDefined) {
        return Term::insert(arith, auxGen, make_locatable<LinearTerm>(loc(), *var_, m_, n_), true);
    }
    return nullptr;
}

bool LinearTerm::operator==(Term const &other) const {
    const auto *t = dynamic_cast<LinearTerm const*>(&other);
    return t != nullptr &&
           m_ == t->m_ &&
           n_ == t->n_ &&
           is_value_equal_to(var_, t->var_);
}

size_t LinearTerm::hash() const {
    return get_value_hash(typeid(LinearTerm).hash_code(), m_, n_, var_->hash());
}

LinearTerm *LinearTerm::clone() const {
    return make_locatable<LinearTerm>(loc(), *var_, m_, n_).release();
}

Sig LinearTerm::getSig() const {
    throw std::logic_error("Term::getSig must not be called on LinearTerm");
}

UTerm LinearTerm::renameVars(RenameMap &names) const {
    return make_locatable<LinearTerm>(loc(), UVarTerm(static_cast<VarTerm*>(var_->renameVars(names).release())), m_, n_); // NOLINT
}

UGTerm LinearTerm::gterm(RenameMap &names, ReferenceMap &refs) const {
    return gringo_make_unique<GLinearTerm>(var_->_newRef(names, refs), m_, n_);
}

void LinearTerm::collectIds(VarSet &vars) const {
    static_cast<void>(vars);
}

UTerm LinearTerm::replace(Defines &defs, bool replace) {
    static_cast<void>(defs);
    static_cast<void>(replace);
    return nullptr;
}

double LinearTerm::estimate(double size, VarSet const &bound) const {
    return var_->estimate(size, bound);
}

Symbol LinearTerm::isEDB() const {
    return {};
}

// {{{1 definition of UnOpTerm

UnOpTerm::UnOpTerm(UnOp op, UTerm &&arg)
: op_(op)
, arg_(std::move(arg)) { }

bool UnOpTerm::addToLinearTerm(IETermVec &terms) const {
    IETermVec arg;
    if (!arg_->addToLinearTerm(arg)) {
        return false;
    }
    for (auto &term : arg) {
        if (term.variable != nullptr) {
            if (op_ != UnOp::NEG) {
                return false;
            }
            add_(terms, -term.coefficient, term.variable);
        }
        else {
            // cannot happen because simplify is be called before bound extraction
            return false;
        }
    }
    return true;
}

unsigned UnOpTerm::projectScore() const {
    return arg_->projectScore();
}

void UnOpTerm::rename(String name) {
    throw std::logic_error("must not be called");
}

unsigned UnOpTerm::getLevel() const {
    return arg_->getLevel();
}

bool UnOpTerm::isNotNumeric() const {
    return false;
}
bool UnOpTerm::isNotFunction() const {
    return op_ != UnOp::NEG;
}

Term::Invertibility UnOpTerm::getInvertibility() const {
    return op_ == UnOp::NEG ? Term::INVERTIBLE : Term::NOT_INVERTIBLE;
}

void UnOpTerm::print(std::ostream &out) const {
    if (op_ == UnOp::ABS) {
        out << "|" << *arg_ << "|";
    }
    else {
        // TODO: parenthesis are problematic if I want to use this term for predicates
        out << "(" << op_ << *arg_ << ")";
    }
}

Term::SimplifyRet UnOpTerm::simplify(SimplifyState &state, bool positional, bool arithmetic, Logger &log) {
    bool multiNeg = !arithmetic && op_ == UnOp::NEG;
    SimplifyRet ret(arg_->simplify(state, false, !multiNeg, log));
    if (ret.undefined()) {
        return {};
    }
    if ((multiNeg && ret.notNumeric() && ret.notFunction()) || (!multiNeg && ret.notNumeric())) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << loc() << ": info: operation undefined:\n"
            << "  " << *this << "\n";
        return {};
    }
    switch (ret.type()) {
        case SimplifyRet::CONSTANT: {
            auto val = ret.value();
            if (val.type() == SymbolType::Num) {
                return {Symbol::createNum(Gringo::eval(op_, val.num()))};
            }
            assert(val.type() == SymbolType::Fun);
            return {val.flipSign()};
        }
        case SimplifyRet::LINEAR: {
            if (op_ == UnOp::NEG) {
                ret.lin().invert();
                return ret;
            }
        }
        default: {
            ret.update(arg_, !multiNeg);
            return {*this, false};
        }
    }
}

Term::ProjectRet UnOpTerm::project(bool rename, AuxGen &auxGen) {
    assert(!rename); static_cast<void>(rename);
    UTerm y(auxGen.uniqueVar(loc(), 0, "#X"));
    UTerm x(wrap(UTerm(y->clone())));
    return std::make_tuple(wrap(make_locatable<UnOpTerm>(loc(), op_, std::move(arg_))), std::move(x), std::move(y));
}

bool UnOpTerm::hasVar() const {
    return arg_->hasVar();
}

bool UnOpTerm::hasPool() const {
    return arg_->hasPool();
}

void UnOpTerm::collect(VarTermBoundVec &vars, bool bound) const {
    arg_->collect(vars, bound && op_ == UnOp::NEG);
}

void UnOpTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    arg_->collect(vars, minLevel, maxLevel);
}

Symbol UnOpTerm::eval(bool &undefined, Logger &log) const {
    bool undefined_arg = false;
    Symbol value = arg_->eval(undefined_arg, log);
    if (value.type() == SymbolType::Num) {
        undefined = undefined || undefined_arg;
        int num = value.num();
        switch (op_) {
            case UnOp::NEG: { return Symbol::createNum(-num); }
            case UnOp::ABS: { return Symbol::createNum(std::abs(num)); }
            case UnOp::NOT: { return Symbol::createNum(~num); }
        }
        assert(false);
        return Symbol::createNum(0);
    }
    if (op_ == UnOp::NEG && value.type() == SymbolType::Fun) {
        undefined = undefined || undefined_arg;
        return value.flipSign();
    }
    if (!undefined_arg) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << loc() << ": info: operation undefined:\n"
            << "  " << *this << "\n";
    }
    undefined = true;
    return Symbol::createNum(0);
}

bool UnOpTerm::match(Symbol const &val) const  {
    if (op_ != UnOp::NEG) {
        throw std::logic_error("Term::rewriteArithmetics must be called before Term::match");
    }
    if (val.type() == SymbolType::Num) {
        return arg_->match(Symbol::createNum(-val.num()));
    }
    if (val.type() == SymbolType::Fun) {
        return arg_->match(val.flipSign());
    }
    return false;
}

void UnOpTerm::unpool(UTermVec &x) const {
    auto f = [&](UTerm &&y) { x.emplace_back(make_locatable<UnOpTerm>(loc(), op_, std::move(y))); };
    Term::unpool(arg_, Gringo::unpool, f);
}

UTerm UnOpTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    if (!forceDefined && op_ == UnOp::NEG) {
        Term::replace(arg_, arg_->rewriteArithmetics(arith, auxGen, false));
        return nullptr;
    }
            return Term::insert(arith, auxGen, make_locatable<UnOpTerm>(loc(), op_, std::move(arg_)), forceDefined && op_ == UnOp::NEG);

}

bool UnOpTerm::operator==(Term const &other) const {
    const auto *t = dynamic_cast<UnOpTerm const*>(&other);
    return (t != nullptr) && op_ == t->op_ && is_value_equal_to(arg_, t->arg_);
}

size_t UnOpTerm::hash() const {
    return get_value_hash(typeid(UnOpTerm).hash_code(), size_t(op_), arg_);
}

UnOpTerm *UnOpTerm::clone() const {
    return make_locatable<UnOpTerm>(loc(), op_, get_clone(arg_)).release();
}

Sig UnOpTerm::getSig() const {
    if (op_ == UnOp::NEG) {
        return arg_->getSig().flipSign();
    }
    throw std::logic_error("Term::getSig must not be called on UnOpTerm");
}

UTerm UnOpTerm::renameVars(RenameMap &names) const {
    return make_locatable<UnOpTerm>(loc(), op_, arg_->renameVars(names));
}

UGTerm UnOpTerm::gterm(RenameMap &names, ReferenceMap &refs) const {
    if (op_ == UnOp::NEG) {
        UGFunTerm fun(arg_->gfunterm(names, refs));
        if (fun) {
            fun->flipSign();
            return std::move(fun);
        }
    }
    return gringo_make_unique<GVarTerm>(_newRef(names, refs));
}

UGFunTerm UnOpTerm::gfunterm(RenameMap &names, ReferenceMap &refs) const {
    if (op_ != UnOp::NEG) {
        return nullptr;
    }
    UGFunTerm fun(arg_->gfunterm(names, refs));
    if (!fun) {
        return nullptr;
    }
    fun->flipSign();
    return fun;
}

void UnOpTerm::collectIds(VarSet &vars) const {
    arg_->collectIds(vars);
}

UTerm UnOpTerm::replace(Defines &defs, bool replace) {
    static_cast<void>(replace);
    Term::replace(arg_, arg_->replace(defs, true));
    return nullptr;
}

double UnOpTerm::estimate(double size, VarSet const &bound) const {
    return 0;
}

Symbol UnOpTerm::isEDB() const {
    return {};
}

bool UnOpTerm::isAtom() const {
    return op_ == UnOp::NEG && arg_->isAtom();
}

// {{{1 definition of BinOpTerm

BinOpTerm::BinOpTerm(BinOp op, UTerm &&left, UTerm &&right)
: op_(op)
, left_(std::move(left))
, right_(std::move(right)) { }

bool BinOpTerm::addToLinearTerm(IETermVec &terms) const {
    IETermVec left;
    IETermVec right;
    if (!left_->addToLinearTerm(left)) {
        return false;
    }
    if (!right_->addToLinearTerm(right)) {
        return false;
    }
    switch (op_) {
        case BinOp::ADD:
        case BinOp::SUB: {
            for (auto &term : left) {
                add_(terms, term.coefficient, term.variable);
            }
            for (auto &term : right) {
                add_(terms, op_ == BinOp::ADD ? term.coefficient : -term.coefficient, term.variable);
            }
            return true;
        }
        case BinOp::MUL: {
            int64_t num = 0;
            if (toNum_(left, num)) {
                for (auto &term : right) {
                    add_(terms, num * term.coefficient, term.variable);
                }
                return true;
            }
            if (toNum_(right, num)) {
                for (auto &term : left) {
                    add_(terms, num * term.coefficient, term.variable);
                }
                return true;
            }
            break;
        }
        case BinOp::OR:
        case BinOp::AND:
        case BinOp::DIV:
        case BinOp::MOD:
        case BinOp::POW:
        case BinOp::XOR: {
            // cannot happen because simplify must be called before bound extraction
            break;
        }
    }
    return false;
}

unsigned BinOpTerm::projectScore() const {
    return left_->projectScore() + right_->projectScore();
}

void BinOpTerm::rename(String name) {
    throw std::logic_error("must not be called");
}

unsigned BinOpTerm::getLevel() const {
    return std::max(left_->getLevel(), right_->getLevel());
}

bool BinOpTerm::isNotNumeric() const  {
    return false;
}
bool BinOpTerm::isNotFunction() const {
    return true;
}

Term::Invertibility BinOpTerm::getInvertibility() const {
    return Term::NOT_INVERTIBLE;
}

void BinOpTerm::print(std::ostream &out) const {
    out << "(" << *left_ << op_ << *right_ << ")";
}

Term::SimplifyRet BinOpTerm::simplify(SimplifyState &state, bool positional, bool arithmetic, Logger &log) {
    auto retLeft(left_->simplify(state, false, true, log));
    auto retRight(right_->simplify(state, false, true, log));
    if (retLeft.undefined() || retRight.undefined()) {
        return {};
    }
    if (retLeft.notNumeric() || retRight.notNumeric() || ((op_ == BinOp::DIV || op_ == BinOp::MOD) && retRight.isZero())) {
        retLeft.update(left_, true); retRight.update(right_, true);
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << loc() << ": info: operation undefined:\n"
            << "  " << *this << "\n";
        return {};
    }
    if (op_ == BinOp::MUL && (retLeft.isZero() || retRight.isZero())) {
        // NOTE: keep binary operation untouched
    }
    else if (retLeft.type() == SimplifyRet::CONSTANT && retRight.type() == SimplifyRet::CONSTANT) {
        auto left  = retLeft.value().num();
        auto right = retRight.value().num();
        if (op_ == BinOp::POW && left == 0 && right < 0) {
            GRINGO_REPORT(log, Warnings::OperationUndefined)
                << loc() << ": info: operation undefined:\n"
                << "  " << *this << "\n";
            return {};
        }
        return {Symbol::createNum(Gringo::eval(op_, left, right))};
    }
    else if (retLeft.type() == SimplifyRet::CONSTANT && retRight.type() == SimplifyRet::LINEAR) {
        if (op_ == BinOp::ADD) {
            retRight.lin().add(retLeft.value().num());
            return retRight;
        }
        if (op_ == BinOp::SUB) {
            retRight.lin().invert();
            retRight.lin().add(retLeft.value().num());
            return retRight;
        }
        if (op_ == BinOp::MUL) {
            retRight.lin().mul(retLeft.value().num());
            return retRight;
        }
    }
    else if (retLeft.type() == SimplifyRet::LINEAR && retRight.type() == SimplifyRet::CONSTANT) {
        if (op_ == BinOp::ADD) {
            retLeft.lin().add(retRight.value().num());
            return retLeft;
        }
        if (op_ == BinOp::SUB) {
            retLeft.lin().add(-retRight.value().num());
            return retLeft;
        }
        if (op_ == BinOp::MUL) {
            // multiply
            retLeft.lin().mul(retRight.value().num());
            return retLeft;
        }
    }
    retLeft.update(left_, true);
    retRight.update(right_, true);
    return {*this, false};
}

Term::ProjectRet BinOpTerm::project(bool rename, AuxGen &auxGen) {
    assert(!rename); static_cast<void>(rename);
    UTerm y(auxGen.uniqueVar(loc(), 0, "#X"));
    UTerm x(wrap(UTerm(y->clone())));
    return std::make_tuple(wrap(make_locatable<BinOpTerm>(loc(), op_, std::move(left_), std::move(right_))), std::move(x), std::move(y));
}

bool BinOpTerm::hasVar() const {
    return left_->hasVar() || right_->hasVar();
}

bool BinOpTerm::hasPool() const {
    return left_->hasPool() || right_->hasPool();
}

void BinOpTerm::collect(VarTermBoundVec &vars, bool bound) const {
    left_->collect(vars, false);
    right_->collect(vars, false);
}

void BinOpTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    left_->collect(vars, minLevel, maxLevel);
    right_->collect(vars, minLevel, maxLevel);
}

Symbol BinOpTerm::eval(bool &undefined, Logger &log) const {
    bool undefined_arg = false;
    Symbol l(left_->eval(undefined_arg, log));
    Symbol r(right_->eval(undefined_arg, log));
    bool defined =
        l.type() == SymbolType::Num &&
        r.type() == SymbolType::Num &&
        ((op_ != BinOp::DIV && op_ != BinOp::MOD) || r.num() != 0) &&
        (op_ != BinOp::POW || l.num() != 0 || r.num() >= 0);
    if (defined) {
        undefined = undefined || undefined_arg;
        return Symbol::createNum(Gringo::eval(op_, l.num(), r.num()));
    }
    if (!undefined_arg) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << loc() << ": info: operation undefined:\n"
            << "  " << *this << "\n";
    }
    undefined = true;
    return Symbol::createNum(0);
}

bool BinOpTerm::match(Symbol const &val) const {
    static_cast<void>(val);
    throw std::logic_error("Term::rewriteArithmetics must be called before Term::match");
}

void BinOpTerm::unpool(UTermVec &x) const {
    auto f = [&](UTerm &&l, UTerm &&r) { x.emplace_back(make_locatable<BinOpTerm>(loc(), op_, std::move(l), std::move(r))); };
    Term::unpool(left_, right_, Gringo::unpool, Gringo::unpool, f);
}

UTerm BinOpTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    return Term::insert(arith, auxGen, make_locatable<BinOpTerm>(loc(), op_, std::move(left_), std::move(right_)));
}

bool BinOpTerm::operator==(Term const &other) const {
    const auto *t = dynamic_cast<BinOpTerm const*>(&other);
    return (t != nullptr) && op_ == t->op_ && is_value_equal_to(left_, t->left_) && is_value_equal_to(right_, t->right_);
}

size_t BinOpTerm::hash() const {
    return get_value_hash(typeid(BinOpTerm).hash_code(), size_t(op_), left_, right_);
}

BinOpTerm *BinOpTerm::clone() const {
    return make_locatable<BinOpTerm>(loc(), op_, get_clone(left_), get_clone(right_)).release();
}

Sig BinOpTerm::getSig() const {
    throw std::logic_error("Term::getSig must not be called on DotsTerm");
}

UTerm BinOpTerm::renameVars(RenameMap &names) const {
    UTerm term(left_->renameVars(names));
    return make_locatable<BinOpTerm>(loc(), op_, std::move(term), right_->renameVars(names));
}

UGTerm BinOpTerm::gterm(RenameMap &names, ReferenceMap &refs) const {
    return gringo_make_unique<GVarTerm>(_newRef(names, refs));
}

void BinOpTerm::collectIds(VarSet &vars) const {
    left_->collectIds(vars);
    right_->collectIds(vars);
}

UTerm BinOpTerm::replace(Defines &defs, bool replace) {
    static_cast<void>(replace);
    Term::replace(left_, left_->replace(defs, true));
    Term::replace(right_, right_->replace(defs, true));
    return nullptr;
}

double BinOpTerm::estimate(double size, VarSet const &bound) const {
    static_cast<void>(size);
    static_cast<void>(bound);
    return 0;
}

Symbol BinOpTerm::isEDB() const {
    return {};
}

// {{{1 definition of DotsTerm

DotsTerm::DotsTerm(UTerm &&left, UTerm &&right)
: left_(std::move(left))
, right_(std::move(right)) { }

bool DotsTerm::addToLinearTerm(IETermVec &terms) const {
    static_cast<void>(terms);
    throw std::logic_error("PoolTerm::addToLinearTerm must not be called");
}

unsigned DotsTerm::projectScore() const {
    return 2;
}

void DotsTerm::rename(String name) {
    throw std::logic_error("must not be called");
}

unsigned DotsTerm::getLevel() const {
    return std::max(left_->getLevel(), right_->getLevel());
}

bool DotsTerm::isNotNumeric() const  {
    return false;
}

bool DotsTerm::isNotFunction() const {
    return true;
}

Term::Invertibility DotsTerm::getInvertibility() const {
    return Term::NOT_INVERTIBLE;
}

void DotsTerm::print(std::ostream &out) const {
    out << "(" << *left_ << ".." << *right_ << ")";
}

Term::SimplifyRet DotsTerm::simplify(SimplifyState &state, bool positional, bool arithmetic, Logger &log) {
    if (!left_->simplify(state, false, false, log).update(left_, true).undefined() && !right_->simplify(state, false, false, log).update(right_, true).undefined()) {
        return { state.createDots(loc(), std::move(left_), std::move(right_)) };
    }
    return {};

}

Term::ProjectRet DotsTerm::project(bool rename, AuxGen &gen) {
    static_cast<void>(rename);
    static_cast<void>(gen);
    throw std::logic_error("Term::project must be called after Term::simplify");
}

bool DotsTerm::hasVar() const {
    return left_->hasVar() || right_->hasVar();
}

bool DotsTerm::hasPool() const  {
    return left_->hasPool() || right_->hasPool();
}

void DotsTerm::collect(VarTermBoundVec &vars, bool bound) const {
    left_->collect(vars, false);
    right_->collect(vars, false);
}

void DotsTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    left_->collect(vars, minLevel, maxLevel);
    right_->collect(vars, minLevel, maxLevel);
}

Symbol DotsTerm::eval(bool &undefined, Logger &log) const {
    static_cast<void>(undefined);
    static_cast<void>(log);
    throw std::logic_error("Term::rewriteDots must be called before Term::eval");
}

bool DotsTerm::match(Symbol const &val) const  {
    static_cast<void>(val);
    throw std::logic_error("Term::rewriteDots must be called before Term::match");
}

void DotsTerm::unpool(UTermVec &x) const {
    auto f = [&](UTerm &&l, UTerm &&r) { x.emplace_back(make_locatable<DotsTerm>(loc(), std::move(l), std::move(r))); };
    Term::unpool(left_, right_, Gringo::unpool, Gringo::unpool, f);
}

UTerm DotsTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    static_cast<void>(arith);
    static_cast<void>(auxGen);
    static_cast<void>(forceDefined);
    throw std::logic_error("Term::rewriteDots must be called before Term::rewriteArithmetics");
}

bool DotsTerm::operator==(Term const &other) const {
    static_cast<void>(other);
    // Note: each DotsTerm is associated to a unique variable
    return false;
}

size_t DotsTerm::hash() const {
    return get_value_hash(typeid(DotsTerm).hash_code(), left_, right_);
}

DotsTerm *DotsTerm::clone() const {
    return make_locatable<DotsTerm>(loc(), get_clone(left_), get_clone(right_)).release();
}

Sig DotsTerm::getSig() const {
    throw std::logic_error("Term::getSig must not be called on LuaTerm");
}

UTerm DotsTerm::renameVars(RenameMap &names) const {
    UTerm term(left_->renameVars(names));
    return make_locatable<DotsTerm>(loc(), std::move(term), right_->renameVars(names));
}

UGTerm DotsTerm::gterm(RenameMap &names, ReferenceMap &refs) const  {
    return gringo_make_unique<GVarTerm>(_newRef(names, refs));
}

void DotsTerm::collectIds(VarSet &vars) const {
    left_->collectIds(vars);
    right_->collectIds(vars);
}

UTerm DotsTerm::replace(Defines &defs, bool replace) {
    static_cast<void>(replace);
    Term::replace(left_, left_->replace(defs, true));
    Term::replace(right_, right_->replace(defs, true));
    return nullptr;
}

double DotsTerm::estimate(double size, VarSet const &bound) const {
    static_cast<void>(size);
    static_cast<void>(bound);
    return 0;
}

Symbol DotsTerm::isEDB() const {
    return {};
}

// {{{1 definition of LuaTerm

LuaTerm::LuaTerm(String name, UTermVec &&args)
: name_(name)
, args_(std::move(args)) { }

bool LuaTerm::addToLinearTerm(IETermVec &terms) const {
    static_cast<void>(terms);
    throw std::logic_error("PoolTerm::addToLinearTerm must not be called");
}

unsigned LuaTerm::projectScore() const {
    return 2;
}

void LuaTerm::rename(String name) {
    throw std::logic_error("must not be called");
}

unsigned LuaTerm::getLevel() const {
    unsigned level = 0;
    for (const auto &x : args_) {
        level = std::max(x->getLevel(), level);
    }
    return level;
}

bool LuaTerm::isNotNumeric() const {
    return false;
}
bool LuaTerm::isNotFunction() const {
    return false;
}

Term::Invertibility LuaTerm::getInvertibility() const {
    return Term::NOT_INVERTIBLE;
}

void LuaTerm::print(std::ostream &out) const {
    out << "@" << name_ << "(";
    print_comma(out, args_, ",", [](std::ostream &out, UTerm const &y) { out << *y; });
    out << ")";
}

Term::SimplifyRet LuaTerm::simplify(SimplifyState &state, bool positional, bool arith, Logger &log) {
    for (auto &arg : args_) {
        if (arg->simplify(state, false, false, log).update(arg, false).undefined()) {
            return {};
        }
    }
    return state.createScript(loc(), name_, std::move(args_), arith);
}

Term::ProjectRet LuaTerm::project(bool rename, AuxGen &auxGen) {
    assert(!rename); static_cast<void>(rename);
    UTerm y(auxGen.uniqueVar(loc(), 0, "#X"));
    UTerm x(wrap(UTerm(y->clone())));
    return std::make_tuple(make_locatable<LuaTerm>(loc(), name_, std::move(args_)), std::move(x), std::move(y));
}

bool LuaTerm::hasVar() const {
    for (const auto &x : args_) {
        if (x->hasVar()) {
            return true;
        }
    }
    return false;
}

bool LuaTerm::hasPool() const {
    for (const auto &x : args_) {
        if (x->hasPool()) {
            return true;
        }
    }
    return false;
}

void LuaTerm::collect(VarTermBoundVec &vars, bool bound) const {
    for (const auto &y : args_) {
        y->collect(vars, false);
    }
}

void LuaTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    for (const auto &y : args_) {
        y->collect(vars, minLevel, maxLevel);
    }
}

Symbol LuaTerm::eval(bool &undefined, Logger &log) const {
    static_cast<void>(undefined);
    static_cast<void>(log);
    throw std::logic_error("Term::simplify must be called before Term::eval");
}

bool LuaTerm::match(Symbol const &val) const {
    static_cast<void>(val);
    throw std::logic_error("Term::rewriteArithmetics must be called before Term::match");
}

void LuaTerm::unpool(UTermVec &x) const {
    auto f = [&](UTermVec &&args) { x.emplace_back(make_locatable<LuaTerm>(loc(), name_, std::move(args))); };
    Term::unpool(args_.begin(), args_.end(), Gringo::unpool, f);
}

UTerm LuaTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    return Term::insert(arith, auxGen, make_locatable<LuaTerm>(loc(), name_, std::move(args_)));
}

bool LuaTerm::operator==(Term const &other) const {
    const auto *t = dynamic_cast<LuaTerm const*>(&other);
    return (t != nullptr) && name_ == t->name_ && is_value_equal_to(args_, t->args_);
}

size_t LuaTerm::hash() const {
    return get_value_hash(typeid(LuaTerm).hash_code(), name_, args_);
}

LuaTerm *LuaTerm::clone() const {
    return make_locatable<LuaTerm>(loc(), name_, get_clone(args_)).release();
}

Sig LuaTerm::getSig() const {
    return {name_, numeric_cast<uint32_t>(args_.size()), false};
}

UTerm LuaTerm::renameVars(RenameMap &names) const {
    UTermVec args;
    for (const auto &x : args_) {
        args.emplace_back(x->renameVars(names));
    }
    return make_locatable<LuaTerm>(loc(), name_, std::move(args));
}

UGTerm LuaTerm::gterm(RenameMap &names, ReferenceMap &refs) const {
    return gringo_make_unique<GVarTerm>(_newRef(names, refs));
}

void LuaTerm::collectIds(VarSet &vars) const {
    for (const auto &y : args_) {
        y->collectIds(vars);
    }
}

UTerm LuaTerm::replace(Defines &defs, bool replace) {
    static_cast<void>(replace);
    for (auto &y : args_) {
        Term::replace(y, y->replace(defs, true));
    }
    return nullptr;
}

double LuaTerm::estimate(double size, VarSet const &bound) const {
    static_cast<void>(size);
    static_cast<void>(bound);
    return 0;
}

Symbol LuaTerm::isEDB() const {
    return {};
}

// {{{1 definition of FunctionTerm

FunctionTerm::FunctionTerm(String name, UTermVec &&args)
: name_(name)
, args_(std::move(args)) { }

bool FunctionTerm::addToLinearTerm(IETermVec &terms) const {
    static_cast<void>(terms);
    return false;
}

UTermVec const &FunctionTerm::arguments() {
    return args_;
};

unsigned FunctionTerm::projectScore() const {
    unsigned ret = 0;
    for (const auto &x : args_) {
        ret += x->projectScore();
    }
    return ret;
}

void FunctionTerm::rename(String name) {
    name_ = name;
}

unsigned FunctionTerm::getLevel() const {
    unsigned level = 0;
    for (const auto &x : args_) {
        level = std::max(x->getLevel(), level);
    }
    return level;
}

bool FunctionTerm::isNotNumeric() const {
    return true;
}
bool FunctionTerm::isNotFunction() const {
    return false;
}

Term::Invertibility FunctionTerm::getInvertibility() const {
    return Term::INVERTIBLE;
}

void FunctionTerm::print(std::ostream &out) const {
    out << name_ << "(";
    print_comma(out, args_, ",", [](std::ostream &out, UTerm const &y) { out << *y; });
    if (name_ == "" && args_.size() == 1) {
        out << ",";
    }
    out << ")";
}

Term::SimplifyRet FunctionTerm::simplify(SimplifyState &state, bool positional, bool arithmetic, Logger &log) {
    static_cast<void>(arithmetic);
    bool constant  = true;
    bool projected = false;
    for (auto &arg : args_) {
        auto ret(arg->simplify(state, positional, false, log));
        if (ret.undefined()) {
            return {};
        }
        constant  = constant  && ret.constant();
        projected = projected || ret.project();
        ret.update(arg, false);
    }
    if (constant) {
        bool undefined = false;
        return {eval(undefined, log)};
    }
    return {*this, projected};
}

Term::ProjectRet FunctionTerm::project(bool rename, AuxGen &auxGen) {
    UTermVec argsProjected;
    UTermVec argsProject;
    for (auto &arg : args_) {
        auto ret(arg->project(false, auxGen));
        Term::replace(arg, std::move(std::get<0>(ret)));
        argsProjected.emplace_back(std::move(std::get<1>(ret)));
        argsProject.emplace_back(std::move(std::get<2>(ret)));
    }
    String oldName = name_;
    if (rename) {
        name_ = String((std::string("#p_") + name_.c_str()).c_str());
    }
    return std::make_tuple(
            nullptr,
            make_locatable<FunctionTerm>(loc(), name_, std::move(argsProjected)),
            make_locatable<FunctionTerm>(loc(), oldName, std::move(argsProject)));
}

bool FunctionTerm::hasVar() const {
    for (const auto &x : args_) {
        if (x->hasVar()) {
            return true;
        }
    }
    return false;
}

bool FunctionTerm::hasPool() const {
    for (const auto &x : args_) {
        if (x->hasPool()) {
            return true;
        }
    }
    return false;
}

void FunctionTerm::collect(VarTermBoundVec &vars, bool bound) const {
    for (const auto &y : args_) {
        y->collect(vars, bound);
    }
}

void FunctionTerm::collect(VarSet &vars, unsigned minLevel , unsigned maxLevel) const {
    for (const auto &y : args_) {
        y->collect(vars, minLevel, maxLevel);
    }
}

Symbol FunctionTerm::eval(bool &undefined, Logger &log) const {
    cache_.clear();
    for (const auto &term : args_) {
        cache_.emplace_back(term->eval(undefined, log));
    }
    return Symbol::createFun(name_, Potassco::toSpan(cache_));
}

bool FunctionTerm::match(Symbol const &val) const {
    if (val.type() == SymbolType::Fun) {
        Sig s(val.sig());
        if (!s.sign() && s.name() == name_ && s.arity() == args_.size()) {
            auto i = 0;
            for (const auto &term : args_) {
                if (!term->match(val.args()[i++])) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

void FunctionTerm::unpool(UTermVec &x) const {
    auto f = [&](UTermVec &&args) { x.emplace_back(make_locatable<FunctionTerm>(loc(), name_, std::move(args))); };
    Term::unpool(args_.begin(), args_.end(), Gringo::unpool, f);
}

UTerm FunctionTerm::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen, bool forceDefined) {
    for (auto &arg : args_) {
        Term::replace(arg, arg->rewriteArithmetics(arith, auxGen, forceDefined));
    }
    return nullptr;
}

bool FunctionTerm::operator==(Term const &other) const {
    const auto *t = dynamic_cast<FunctionTerm const*>(&other);
    return (t != nullptr) && name_ == t->name_ && is_value_equal_to(args_, t->args_);
}

size_t FunctionTerm::hash() const {
    return get_value_hash(typeid(FunctionTerm).hash_code(), name_, args_);
}

FunctionTerm *FunctionTerm::clone() const {
    return make_locatable<FunctionTerm>(loc(), name_, get_clone(args_)).release();
}

Sig FunctionTerm::getSig() const {
    return {name_, numeric_cast<uint32_t>(args_.size()), false};
}

UTerm FunctionTerm::renameVars(RenameMap &names) const {
    UTermVec args;
    for (const auto &x : args_) {
        args.emplace_back(x->renameVars(names));
    }
    return make_locatable<FunctionTerm>(loc(), name_, std::move(args));
}

UGTerm FunctionTerm::gterm(RenameMap &names, ReferenceMap &refs) const {
    return gfunterm(names, refs);
}

UGFunTerm FunctionTerm::gfunterm(RenameMap &names, ReferenceMap &refs) const {
    UGTermVec args;
    for (const auto &x : args_) {
        args.emplace_back(x->gterm(names, refs));
    }
    return gringo_make_unique<GFunctionTerm>(name_, std::move(args));
}

void FunctionTerm::collectIds(VarSet &vars) const {
    for (const auto &y : args_) {
        y->collectIds(vars);
    }
}

UTerm FunctionTerm::replace(Defines &defs, bool replace) {
    static_cast<void>(replace);
    for (auto &y : args_) {
        Term::replace(y, y->replace(defs, true));
    }
    return nullptr;
}

double FunctionTerm::estimate(double size, VarSet const &bound) const {
    double ret = 0.0;
    if (!args_.empty()) {
        auto len = static_cast<double>(args_.size());
        double root = std::max(1.0, std::pow((name_.empty() ? size : size / 2.0), 1.0 / len)); // NOLINT
        for (const auto &x : args_) {
            ret += x->estimate(root, bound);
        }
        ret /= len;
    }
    return ret;
}

Symbol FunctionTerm::isEDB() const {
    cache_.clear();
    for (const auto &x : args_) {
        cache_.emplace_back(x->isEDB());
        if (cache_.back().type() == SymbolType::Special) {
            return {};
        }
    }
    return Symbol::createFun(name_, Potassco::toSpan(cache_));
}

bool FunctionTerm::isAtom() const {
    return true;
}

// }}}1

} // namespace Gringo
