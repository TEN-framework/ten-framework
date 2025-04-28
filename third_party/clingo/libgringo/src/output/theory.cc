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

#include "gringo/output/theory.hh"
#include "gringo/output/literal.hh"
#include "gringo/backend.hh"
#include "gringo/logger.hh"
#include "gringo/output/literals.hh"
#include "potassco/basic_types.h"
#include <cstring>
#include <tuple>

namespace Gringo { namespace Output {

namespace {

bool addSeen(std::vector<bool>& vec, Potassco::Id_t id) {
    if (vec.size() <= id) { vec.resize(id + 1, false); }
    bool seen = vec[id];
    if (!seen) { vec[id] = true; }
    return !seen;
}

} // namespace

// {{{1 definition of TheoryParser

TheoryParser::Elem::Elem(String op, bool unary)
: tokenType_(Op)
, op_(op, unary) { }

TheoryParser::Elem::Elem(UTheoryTerm term)
: tokenType_(Id)
, term_(std::move(term))  {}

TheoryParser::Elem::Elem(Elem &&elem) noexcept
: tokenType_(elem.type()) {
    if (elem.type() == Id) {
        new (&term()) UTheoryTerm(std::move(elem.term()));
    }
    else {
        new (&op()) std::pair<String, bool>(std::move(elem.op()));
    }
}

TheoryParser::Elem::~Elem() noexcept {
    if (type() == Id) {
        term().~UTheoryTerm();
    }
    else {
        op().~pair();
    }
}

TheoryParser::TokenType TheoryParser::Elem::type() {
    return tokenType_;
}

std::pair<String,bool> &TheoryParser::Elem::op() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    return op_;
}

UTheoryTerm &TheoryParser::Elem::term() {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    return term_;
}

TheoryParser::TheoryParser(Location const &loc, TheoryTermDef const &def)
: loc_(loc)
, def_(def) { }

bool TheoryParser::check(String op) {
    if (stack_.size() < 2) { return false; }
    assert(stack_.back().type() == Id);
    auto current  = def_.getPrioAndAssoc(op);
    auto previous = def_.getPrio(stack_[stack_.size() - 2].op().first, stack_[stack_.size() - 2].op().second);
    return previous > current.first || (previous == current.first && current.second);
}

void TheoryParser::reduce() {
    assert(stack_.back().type() == Id);
    auto b = std::move(stack_.back().term());
    stack_.pop_back();
    assert(stack_.back().type() == Op);
    auto op = stack_.back().op();
    stack_.pop_back();
    if (!op.second) {
        assert(stack_.back().type() == Id);
        auto a = std::move(stack_.back().term());
        stack_.pop_back();
        stack_.emplace_back(gringo_make_unique<BinaryTheoryTerm>(std::move(a), op.first, std::move(b)));
    }
    else {
        stack_.emplace_back(gringo_make_unique<UnaryTheoryTerm>(op.first, std::move(b)));
    }
}

UTheoryTerm TheoryParser::parse(RawTheoryTerm::ElemVec elems, Logger &log) {
    stack_.clear();
    bool unary = true;
    for (auto &elem : elems) {
        for (auto &op : elem.first) {
            if (!def_.hasOp(op, unary)) {
                GRINGO_REPORT(log, Warnings::RuntimeError)
                    << loc_ << ": error: missing definition for operator:" << "\n"
                    << "  " << op << "\n";
            }
            while (!unary && check(op)) {
                reduce();
            }
            stack_.emplace_back(op, unary);
            unary = true;
        }
        stack_.emplace_back(std::move(elem.second));
        unary = false;
    }
    while (stack_.size() > 1) {
        reduce();
    }
    return std::move(stack_.front().term());
}

// {{{1 definition of RawTheoryTerm

RawTheoryTerm::RawTheoryTerm(ElemVec elems)
: elems_(std::move(elems)) { }

void RawTheoryTerm::append(StringVec ops, UTheoryTerm term) {
    assert(elems_.empty() || !ops.empty());
    elems_.emplace_back(std::move(ops), std::move(term));
}

size_t RawTheoryTerm::hash() const {
    return get_value_hash(typeid(RawTheoryTerm).hash_code(), elems_);
}

bool RawTheoryTerm::operator==(TheoryTerm const &other) const {
    auto const *t = dynamic_cast<RawTheoryTerm const *>(&other);
    return t != nullptr && is_value_equal_to(elems_, t->elems_);
}

void RawTheoryTerm::print(std::ostream &out) const {
    out << "(";
    print_comma(out, elems_, ",", [](std::ostream &out, ElemVec::value_type const &elem) {
        print_comma(out, elem.first, " ");
        out << *elem.second;
    });
    out << ")";
}

RawTheoryTerm *RawTheoryTerm::clone() const {
    return gringo_make_unique<RawTheoryTerm>(get_clone(elems_)).release();
}

Potassco::Id_t RawTheoryTerm::eval(TheoryData &data, Logger &log) const {
    static_cast<void>(data);
    static_cast<void>(log);
    throw std::logic_error("RawTheoryTerm::eval must not be called!");
}

void RawTheoryTerm::collect(VarTermBoundVec &vars) {
    for (auto &elem : elems_) {
        elem.second->collect(vars);
    }
}

void RawTheoryTerm::replace(Defines &defs) {
    for (auto &elem : elems_) {
        elem.second->replace(defs);
    }
}

UTheoryTerm RawTheoryTerm::initTheory(TheoryParser &p, Logger &log) {
    for (auto &elem : elems_) {
        Term::replace(elem.second, elem.second->initTheory(p, log));
    }
    return p.parse(std::move(elems_), log);
}

// {{{1 definition of UnaryTheoryTerm

UnaryTheoryTerm::UnaryTheoryTerm(String op, UTheoryTerm arg)
: arg_(std::move(arg))
, op_(op)
{ }

size_t UnaryTheoryTerm::hash() const {
    return get_value_hash(typeid(UnaryTheoryTerm).hash_code(), arg_, op_);
}

bool UnaryTheoryTerm::operator==(TheoryTerm const &other) const {
    auto const *t = dynamic_cast<UnaryTheoryTerm const *>(&other);
    return t != nullptr && is_value_equal_to(arg_, t->arg_) && op_ == t->op_;
}

void UnaryTheoryTerm::print(std::ostream &out) const {
    out << "(" << op_ << *arg_ << ")";
}

UnaryTheoryTerm *UnaryTheoryTerm::clone() const {
    return gringo_make_unique<UnaryTheoryTerm>(op_, get_clone(arg_)).release();
}

Potassco::Id_t UnaryTheoryTerm::eval(TheoryData &data, Logger &log) const {
    auto op = data.addTerm(op_.c_str());
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    Potassco::Id_t args[] = { arg_->eval(data, log) };
    return data.addTermFun(op, Potassco::toSpan(args, 1));
}

void UnaryTheoryTerm::collect(VarTermBoundVec &vars) {
    arg_->collect(vars);
}

void UnaryTheoryTerm::replace(Defines &defs) {
    arg_->replace(defs);
}

UTheoryTerm UnaryTheoryTerm::initTheory(TheoryParser &p, Logger &log) {
    Term::replace(arg_, arg_->initTheory(p, log));
    return nullptr;
}

// {{{1 definition of BinaryTheoryTerm

BinaryTheoryTerm::BinaryTheoryTerm(UTheoryTerm left, String op, UTheoryTerm right)
: left_(std::move(left))
, right_(std::move(right))
, op_(op)
{ }

size_t BinaryTheoryTerm::hash() const {
    return get_value_hash(typeid(BinaryTheoryTerm).hash_code(), left_, right_, op_);
}

bool BinaryTheoryTerm::operator==(TheoryTerm const &other) const {
    auto const *t = dynamic_cast<BinaryTheoryTerm const *>(&other);
    return t != nullptr && is_value_equal_to(left_, t->left_) && is_value_equal_to(right_, t->right_) && op_ == t->op_;
}

void BinaryTheoryTerm::print(std::ostream &out) const {
    out << "(" << *left_ << op_ << *right_ << ")";
}

BinaryTheoryTerm *BinaryTheoryTerm::clone() const {
    return gringo_make_unique<BinaryTheoryTerm>(get_clone(left_), op_, get_clone(right_)).release();
}

Potassco::Id_t BinaryTheoryTerm::eval(TheoryData &data, Logger &log) const {
    auto op = data.addTerm(op_.c_str());
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    Potassco::Id_t args[] = { left_->eval(data, log), right_->eval(data, log) };
    return data.addTermFun(op, Potassco::toSpan(args, 2));
}

void BinaryTheoryTerm::collect(VarTermBoundVec &vars) {
    left_->collect(vars);
    right_->collect(vars);
}

void BinaryTheoryTerm::replace(Defines &defs) {
    left_->replace(defs);
    right_->replace(defs);
}

UTheoryTerm BinaryTheoryTerm::initTheory(TheoryParser &p, Logger &log) {
    Term::replace(left_, left_->initTheory(p, log));
    Term::replace(right_, right_->initTheory(p, log));
    return nullptr;
}

// {{{1 definition of TupleTheoryTerm

TupleTheoryTerm::TupleTheoryTerm(Type type, UTheoryTermVec args)
: args_(std::move(args))
, type_(type)
{ }

size_t TupleTheoryTerm::hash() const {
    return get_value_hash(typeid(TupleTheoryTerm).hash_code(), static_cast<unsigned>(type_), args_);
}

bool TupleTheoryTerm::operator==(TheoryTerm const &other) const {
    auto const *t = dynamic_cast<TupleTheoryTerm const*>(&other);
    return t != nullptr && is_value_equal_to(args_, t->args_) && type_ == t->type_;
}

void TupleTheoryTerm::print(std::ostream &out) const {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto const *parens = Potassco::toString(type_);
    out << parens[0];
    print_comma(out, args_, ",", [](std::ostream &out, UTheoryTerm const &term) { out << *term; });
    if (args_.size() == 1 && type_ == Potassco::Tuple_t::Paren) {
        out << ",";
    }
    out << parens[1];
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

TupleTheoryTerm *TupleTheoryTerm::clone() const {
    return gringo_make_unique<TupleTheoryTerm>(type_, get_clone(args_)).release();
}

Potassco::Id_t TupleTheoryTerm::eval(TheoryData &data, Logger &log) const {
    std::vector<Potassco::Id_t> args;
    for (auto const &arg : args_) {
        args.emplace_back(arg->eval(data, log));
    }
    return data.addTermTup(type_, Potassco::toSpan(args));
}

void TupleTheoryTerm::collect(VarTermBoundVec &vars) {
    for (auto &term : args_) {
        term->collect(vars);
    }
}

void TupleTheoryTerm::replace(Defines &defs) {
    for (auto &term : args_) {
        term->replace(defs);
    }
}

UTheoryTerm TupleTheoryTerm::initTheory(TheoryParser &p, Logger &log) {
    for (auto &term : args_) {
        Term::replace(term, term->initTheory(p, log));
    }
    return nullptr;
}

// {{{1 definition of FunctionTheoryTerm

FunctionTheoryTerm::FunctionTheoryTerm(String name, UTheoryTermVec args)
: args_(std::move(args))
, name_(name)
{ }

size_t FunctionTheoryTerm::hash() const {
    return get_value_hash(typeid(FunctionTheoryTerm).hash_code(), name_, args_);
}

bool FunctionTheoryTerm::operator==(TheoryTerm const &other) const {
    auto const *t = dynamic_cast<FunctionTheoryTerm const*>(&other);
    return t != nullptr && is_value_equal_to(args_, t->args_) && name_ == t->name_;
}

void FunctionTheoryTerm::print(std::ostream &out) const {
    out << name_ << "(";
    print_comma(out, args_, ",", [](std::ostream &out, UTheoryTerm const &term) { out << *term; });
    out << ")";
}

FunctionTheoryTerm *FunctionTheoryTerm::clone() const {
    return gringo_make_unique<FunctionTheoryTerm>(name_, get_clone(args_)).release();
}

Potassco::Id_t FunctionTheoryTerm::eval(TheoryData &data, Logger &log) const {
    auto name = data.addTerm(name_.c_str());
    std::vector<Potassco::Id_t> args;
    for (auto const &arg : args_) {
        args.emplace_back(arg->eval(data, log));
    }
    return data.addTermFun(name, Potassco::toSpan(args));
}

void FunctionTheoryTerm::collect(VarTermBoundVec &vars) {
    for (auto &term : args_) {
        term->collect(vars);
    }
}

void FunctionTheoryTerm::replace(Defines &defs) {
    for (auto &term : args_) {
        term->replace(defs);
    }
}

UTheoryTerm FunctionTheoryTerm::initTheory(TheoryParser &p, Logger &log) {
    for (auto &term : args_) {
        Term::replace(term, term->initTheory(p, log));
    }
    return nullptr;
}

// {{{1 definition of TermTheoryTerm

TermTheoryTerm::TermTheoryTerm(UTerm term)
: term_(std::move(term)) {
    assert(dynamic_cast<VarTerm*>(term_.get()) || dynamic_cast<ValTerm*>(term_.get()));
}

size_t TermTheoryTerm::hash() const {
    return get_value_hash(typeid(TermTheoryTerm).hash_code(), term_);
}

bool TermTheoryTerm::operator==(TheoryTerm const &other) const {
    auto const *t = dynamic_cast<TermTheoryTerm const *>(&other);
    return t != nullptr && is_value_equal_to(term_, t->term_);
}

void TermTheoryTerm::print(std::ostream &out) const {
    out << "(" << *term_ << ")";
}

TermTheoryTerm *TermTheoryTerm::clone() const {
    return gringo_make_unique<TermTheoryTerm>(get_clone(term_)).release();
}

Potassco::Id_t TermTheoryTerm::eval(TheoryData &data, Logger &log) const {
    bool undefined = false;
    return data.addTerm(term_->eval(undefined, log));
}

void TermTheoryTerm::collect(VarTermBoundVec &vars) {
    term_->collect(vars, false);
}

void TermTheoryTerm::replace(Defines &defs) {
    Term::replace(term_, term_->replace(defs, true));
}

UTheoryTerm TermTheoryTerm::initTheory(TheoryParser &p, Logger &log) {
    static_cast<void>(p);
    static_cast<void>(log);
    return nullptr;
}

// {{{1 definition of TheoryData

TheoryData::TheoryData(Potassco::TheoryData &data)
: data_(data)
, terms_{0, TermHash{data_}, TermEqual{data_}}
, elems_{0, ElementHash{*this}, ElementEqual{*this}}
, atoms_{0, AtomHash{data_}, AtomEqual{data_}}
, out_(nullptr)
, aSeen_{0}
{ }

void TheoryData::print(Potassco::Id_t termId, const Potassco::TheoryTerm& term) {
    switch (term.type()) {
        case Potassco::Theory_t::Number  : out_->theoryTerm(termId, term.number()); break;
        case Potassco::Theory_t::Symbol  : out_->theoryTerm(termId, Potassco::toSpan(term.symbol())); break;
        case Potassco::Theory_t::Compound: out_->theoryTerm(termId, term.compound(), term.terms()); break;
    }
}
void TheoryData::print(const Potassco::TheoryAtom& a) {
    if (a.guard() != nullptr) {
        out_->theoryAtom(a.atom(), a.term(), a.elements(), *a.guard(), *a.rhs());
    }
    else {
        out_->theoryAtom(a.atom(), a.term(), a.elements());
    }
}
void TheoryData::visit(Potassco::TheoryData const &data, Potassco::Id_t termId, Potassco::TheoryTerm const &t) {
    if (addSeen(tSeen_, termId)) { // only visit once
        // visit any subterms then print
        data.accept(t, *this);
        print(termId, t);
    }
}
void TheoryData::visit(Potassco::TheoryData const &data, Potassco::Id_t elemId, Potassco::TheoryElement const &e) {
    if (addSeen(eSeen_, elemId)) { // only visit once
        // visit terms then print element
        data.accept(e, *this);
        out_->theoryElement(elemId, e.terms(), getCondition(elemId));
    }
}
void TheoryData::visit(Potassco::TheoryData const &data, Potassco::TheoryAtom const &a) {
    // visit elements then print atom
    data.accept(a, *this);
    print(a);
}

void TheoryData::output(TheoryOutput &out) {
    // NOTE: a friend class would probably have been nicer
    out_ = &out;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (auto const *it = data_.begin() + aSeen_; it != data_.end(); ++it) {
        visit(data_, **it);
    }
    aSeen_ = data_.numAtoms();
}

template <typename ...Args>
Potassco::Id_t TheoryData::addTerm_(Args ...args) {
    auto it = terms_.find(std::make_tuple(args...));
    if (it != terms_.end()) {
        return *it;
    }
    auto size = numeric_cast<Id_t>(terms_.size());
    data_.addTerm(size, args...);
    terms_.insert(size);
    return size;
}

Potassco::Id_t TheoryData::addTerm(int number) {
    return addTerm_(number);
}

Potassco::Id_t TheoryData::addTerm(char const *name) {
    return addTerm_(name);
}

Potassco::Id_t TheoryData::addTermFun(Potassco::Id_t funcSym, Potassco::IdSpan const& terms) {
    return addTerm_(funcSym, terms);
}

Potassco::Id_t TheoryData::addTermTup(Potassco::Tuple_t type, Potassco::IdSpan const& terms) {
    return addTerm_(type, terms);
}

Potassco::Id_t TheoryData::addTerm(Symbol value) {
    switch (value.type()) {
        case SymbolType::Num: {
            auto num = value.num();
            if (num < 0) {
                auto f = addTerm("-");
                auto ret = addTerm(-num);
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
                Potassco::Id_t args[] = { ret };
                return addTermFun(f, Potassco::toSpan(args, 1));
            }
            return addTerm(num);
        }
        case SymbolType::Str: {
            std::string s;
            s.push_back('"');
            s.append(quote(value.string().c_str()));
            s.push_back('"');
            return addTerm(s.c_str());
        }
        case SymbolType::Sup: {
            return addTerm("#sup");
        }
        case SymbolType::Inf: {
            return addTerm("#inf");
        }
        case SymbolType::Fun: {
            std::vector<Potassco::Id_t> args;
            for (auto const &arg : value.args()) {
                args.emplace_back(addTerm(arg));
            }
            if (value.name().empty()) {
                return addTermTup(Potassco::Tuple_t::Paren, Potassco::toSpan(args));
            }
            Potassco::Id_t name = addTerm(value.name().c_str());
            auto ret = args.empty()
                ? addTerm(value.name().c_str())
                : addTermFun(name, Potassco::toSpan(args));
            if (value.sign()) {
                Potassco::Id_t f = addTerm("-");
                ret = addTermFun(f, Potassco::toSpan(&ret, 1));
            }
            return ret;
        }
        case SymbolType::Special: {
            assert(false);
            break;
        }
    }
    assert(false);
    return 0;
}

Potassco::Id_t TheoryData::addElem(Potassco::IdSpan const &tuple, Potassco::LitSpan const &cond) {
    LitVec lits;
    for (auto uid : cond) {
        auto offset = static_cast<Potassco::Id_t>(uid > 0 ? uid : -uid);
        lits.emplace_back(LiteralId{uid > 0 ? NAF::POS : NAF::NOT, Gringo::Output::AtomType::Aux, offset, 0});
    }
    return addElem(tuple, std::move(lits));
}

Potassco::Id_t TheoryData::addElem(Potassco::IdSpan const &tuple, LitVec cond) {
    auto it = elems_.find(std::make_pair(tuple, make_span(cond)));
    if (it != elems_.end()) {
        return *it;
    }
    auto size = numeric_cast<Id_t>(elems_.size());
    data_.addElement(size, tuple, cond.empty() ? 0 : Potassco::TheoryData::COND_DEFERRED);
    conditions_.emplace_back(std::move(cond));
    elems_.insert(size);
    return size;
}

template <typename ...Args>
std::pair<Potassco::TheoryAtom const&, bool> TheoryData::addAtom_(std::function<Potassco::Id_t()> const &newAtom, Args ...args) {
    auto it = atoms_.find(std::make_tuple(args...));
    if (it != atoms_.end()) {
        return {**(data_.begin() + *it), false};
    }
    auto size = atoms_.size();
    auto &atom = data_.addAtom(newAtom(), args...);
    atoms_.insert(size);
    return {atom, true};
}

std::pair<Potassco::TheoryAtom const &, bool> TheoryData::addAtom(std::function<Potassco::Id_t()> const &newAtom, Potassco::Id_t termId, Potassco::IdSpan const &elems) {
    return addAtom_(newAtom, termId, elems);
}

std::pair<Potassco::TheoryAtom const &, bool> TheoryData::addAtom(std::function<Potassco::Id_t()> const &newAtom, Potassco::Id_t termId, Potassco::IdSpan const &elems, Potassco::Id_t op, Potassco::Id_t rhs) {
    return addAtom_(newAtom, termId, elems, op, rhs);
}

void TheoryData::printTerm(std::ostream &out, Potassco::Id_t termId) const {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    auto const &term = data_.getTerm(termId);
    switch (term.type()) {
        case Potassco::Theory_t::Number: {
            if (term.number() < 0) { out << "("; }
            out << term.number();
            if (term.number() < 0) { out << ")"; }
            break;
        }
        case Potassco::Theory_t::Symbol: { out << term.symbol(); break; }
        case Potassco::Theory_t::Compound: {
            auto const *parens = Potassco::toString(term.isTuple() ? term.tuple() : Potassco::Tuple_t::Paren);
            bool isOp = false;
            char const *op = ",";
            if (term.isFunction()) {
                if (term.size() <= 2) {
                    auto const &name = data_.getTerm(term.function());
                    char buf[2] = { *name.symbol(), 0 };
                    if ((isOp = std::strpbrk(buf, "/!<=>+-*\\?&@|:;~^.") != nullptr)) {
                        op = name.symbol();
                    }
                    else if ((isOp = strcmp(name.symbol(), "not") == 0)) {
                        op = term.size() == 1 ? "not " : " not ";
                    }
                }
                if (!isOp) {
                    printTerm(out, term.function());
                }
            }
            out << parens[0];
            if (isOp && term.size() <= 1) {
                out << op;
            }
            print_comma(out, term, op, [this](std::ostream &out, Potassco::Id_t termId){ printTerm(out, termId); });
            if (term.isTuple() && term.tuple() == Potassco::Tuple_t::Paren && term.size() == 1) { out << ","; }
            out << parens[1];
            break;
        }
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
}

void TheoryData::printElem(std::ostream &out, Potassco::Id_t elemId, PrintLit printLit) const {
    auto const &elem = data_.getElement(elemId);
    auto const &cond = getCondition(elemId);
    print_comma(out, elem, ",", [this](std::ostream &out, Potassco::Id_t termId){ printTerm(out, termId); });
    if (elem.size() == 0 && cond.empty()) {
        out << ": ";
    }
    else if (!cond.empty()) {
        out << ": ";
        print_comma(out, cond, ",", [&printLit](std::ostream &out, LiteralId const &lit){ printLit(out, lit); });
    }
}

bool TheoryData::empty() const {
    return data_.numAtoms() == 0;
}

Potassco::TheoryData const &TheoryData::data() const {
    return data_;
}

LitVec const &TheoryData::getCondition(Potassco::Id_t elemId) const {
    assert(elemId < elems_.size());
    return conditions_[elemId];
}

void TheoryData::setCondition(Potassco::Id_t elementId, Potassco::Id_t newCond) {
    data_.setCondition(elementId, newCond);
}

void TheoryData::reset(bool resetData) {
    aSeen_ = 0;
    tSeen_.clear();
    eSeen_.clear();
    TermSet(0, TermHash{data_}, TermEqual{data_}).swap(terms_);
    ElementSet(0, ElementHash{*this}, ElementEqual{*this}).swap(elems_);
    AtomSet(0, AtomHash{data_}, AtomEqual{data_}).swap(atoms_);
    ConditionVec().swap(conditions_);
    if (resetData) {
        data_.reset();
    }
}

// }}}1

} } // namspace Gringo
