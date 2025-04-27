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

#ifndef GRINGO_OUTPUT_THEORY_HH
#define GRINGO_OUTPUT_THEORY_HH

#include <potassco/basic_types.h>
#include <gringo/utility.hh>
#include <gringo/base.hh>
#include <gringo/terms.hh>
#include <gringo/types.hh>
#include <gringo/hash_set.hh>
#include <potassco/theory_data.h>
#include <gringo/output/literal.hh>
#include <functional>

namespace Gringo { namespace Output {

using Gringo::StringVec;
using Gringo::GetName;

// {{{1 declaration of TheoryData

class TheoryOutput {
public:
    TheoryOutput() = default;
    TheoryOutput(TheoryOutput const &other) = default;
    TheoryOutput(TheoryOutput &&other) noexcept = default;
    TheoryOutput &operator=(TheoryOutput const &other) = default;
    TheoryOutput &operator=(TheoryOutput &&other) noexcept = default;
    virtual ~TheoryOutput() = default;

    virtual void theoryTerm(Id_t termId, int number) = 0;
    virtual void theoryTerm(Id_t termId, const StringSpan& name) = 0;
    virtual void theoryTerm(Id_t termId, int cId, const IdSpan& args) = 0;
    virtual void theoryElement(Id_t elementId, IdSpan const & terms, LitVec const &cond) = 0;
    virtual void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements) = 0;
    virtual void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements, Id_t op, Id_t rhs) = 0;

};

class TheoryData : private Potassco::TheoryData::Visitor {
    struct TermHash {
        TermHash(Potassco::TheoryData const &data)
        : data{&data} { }

        size_t operator()(Id_t x) const {
            return operator()(data->getTerm(x));
        }

        size_t operator()(std::tuple<int> const &x) const {
            return hash_mix(get_value_hash(static_cast<unsigned>(Potassco::Theory_t::Number), std::get<0>(x)));
        }

        size_t operator()(std::tuple<char const *> const &x) const {
            return hash_mix(get_value_hash(static_cast<unsigned>(Potassco::Theory_t::Symbol), strhash(std::get<0>(x))));
        }

        size_t operator()(std::tuple<Id_t, IdSpan> const &x) const {
            size_t seed = get_value_hash(static_cast<unsigned>(Potassco::Theory_t::Compound), std::get<0>(x));
            for (auto const &t : std::get<1>(x)) {
                hash_combine(seed, t);
            }
            return hash_mix(seed);
        }

        size_t operator()(std::tuple<Potassco::Tuple_t, IdSpan> const &x) const {
            size_t seed = get_value_hash(static_cast<unsigned>(Potassco::Theory_t::Compound), static_cast<unsigned>(std::get<0>(x)));
            for (auto const &t : std::get<1>(x)) {
                hash_combine(seed, t);
            }
            return hash_mix(seed);
        }

        size_t operator()(Potassco::TheoryTerm const &term) const {
            switch (term.type()) {
                case Potassco::Theory_t::Number: {
                    return operator()(std::make_tuple(term.number()));
                }
                case Potassco::Theory_t::Symbol: {
                    return operator()(std::make_tuple(term.symbol()));
                }
                case Potassco::Theory_t::Compound: {
                    break;
                }
            }
            return term.isTuple() ?
                operator()(std::make_tuple(term.tuple(), term.terms())) :
                operator()(std::make_tuple(term.function(), term.terms()));
        }

        Potassco::TheoryData const *data;
    };
    struct TermEqual {
        using is_transparent = void;

        TermEqual(Potassco::TheoryData const &data)
        : data{&data} { }

        bool operator()(Id_t x, Id_t y) const {
            return x == y;
        }

        template <class T>
        bool operator()(Id_t x, T const &y) const {
            return operator()(data->getTerm(x), y);
        }

        template <class T>
        bool operator()(T const &x, Id_t y) const {
            return operator()(data->getTerm(y), x);
        }

        bool operator()(Potassco::TheoryTerm const &x, std::tuple<int> const &y) const {
            return x.type() == Potassco::Theory_t::Number &&
                   x.number() == std::get<0>(y);
        }

        bool operator()(Potassco::TheoryTerm const &x, std::tuple<char const *> const &y) const {
            return x.type() == Potassco::Theory_t::Symbol &&
                   strcmp(x.symbol(), std::get<0>(y)) == 0;
        }

        bool operator()(Potassco::TheoryTerm const &x, std::tuple<Potassco::Tuple_t, IdSpan> const &y) const {
            return x.type() == Potassco::Theory_t::Compound &&
                   x.isTuple() &&
                   x.tuple() == std::get<0>(y) &&
                   x.size() == std::get<1>(y).size &&
                   std::equal(x.begin(), x.end(), Potassco::begin(std::get<1>(y)));
        }

        bool operator()(Potassco::TheoryTerm const &x, std::tuple<Id_t, IdSpan> const &y) const {
            return x.type() == Potassco::Theory_t::Compound &&
                   x.isFunction() &&
                   x.function() == std::get<0>(y) &&
                   x.size() == std::get<1>(y).size &&
                   std::equal(x.begin(), x.end(), Potassco::begin(std::get<1>(y)));
        }

        Potassco::TheoryData const *data;
    };

    struct ElementHash {
        ElementHash(TheoryData const &self)
        : self{&self} { }

        size_t operator()(std::pair<IdSpan, LitSpan> const &x) const {
            size_t seed = hash_range(begin(x.first), end(x.first));
            hash_combine(seed, hash_range(begin(x.second), end(x.second)));
            return hash_mix(seed);
        }

        size_t operator()(Id_t const &x) const {
            return operator()({self->data_.getElement(x).terms(), make_span(self->conditions_[x])});
        }

        TheoryData const *self;
    };

    struct ElementEqual {
        using is_transparent = void;

        ElementEqual(TheoryData const &self)
        : self{&self} { }

        bool operator()(Id_t const &x, Id_t const &y) const {
            return x == y;
        }

        bool operator()(Id_t const &x, std::pair<IdSpan, LitSpan> const &y) const {
            return operator()({self->data_.getElement(x).terms(), make_span(self->conditions_[x])}, y);
        }

        bool operator()(std::pair<IdSpan, LitSpan> const &x, Id_t const &y) const {
            return operator()(y, x);
        }

        bool operator()(std::pair<IdSpan, LitSpan> const &x, std::pair<IdSpan, LitSpan> const &y) const {
            return x.first.size == y.first.size &&
                   x.second.size == y.second.size &&
                   std::equal(begin(x.first), end(x.first), begin(y.first)) &&
                   std::equal(begin(x.second), end(x.second), begin(y.second));
        }

        TheoryData const *self;
    };

    struct AtomHash {
        AtomHash(Potassco::TheoryData const &data)
        : data{&data} { }

        size_t operator()(std::tuple<Id_t, Potassco::IdSpan> const &x) const {
            size_t seed = std::get<0>(x);
            Gringo::hash_combine(seed, get_value_hash(std::get<1>(x)));
            return hash_mix(seed);
        }

        size_t operator()(std::tuple<Id_t, Potassco::IdSpan, Id_t, Id_t> const &x) const {
            size_t seed = std::get<0>(x);
            Gringo::hash_combine(seed, get_value_hash(std::get<1>(x)));
            Gringo::hash_combine(seed, std::get<2>(x));
            Gringo::hash_combine(seed, std::get<3>(x));
            return hash_mix(seed);
        }

        size_t operator()(Id_t const &x) const {
            auto const &atom = **(data->begin() + x);
            return atom.guard() != nullptr
                ? operator()({atom.term(), atom.elements(), *atom.guard(), *atom.rhs()})
                : operator()({atom.term(), atom.elements()});
        }

        Potassco::TheoryData const *data;
    };

    struct AtomEqual {
        AtomEqual(Potassco::TheoryData const &data)
        : data{&data} { }

        using is_transparent = void;
        using TAtom = std::tuple<Id_t, Potassco::IdSpan>;
        using TAtomG = std::tuple<Id_t, Potassco::IdSpan, Id_t, Id_t>;

        bool operator()(Id_t const &x, Id_t const &y) const {
            return x == y;
        }

        bool operator()(Id_t const &x, TAtom const &y) const {
            auto const &atom = **(data->begin() + x);
            return atom.guard() == nullptr &&
                   atom.term() == std::get<0>(y) &&
                   atom.size() == std::get<1>(y).size &&
                   std::equal(atom.begin(), atom.end(), begin(std::get<1>(y)));
        }

        bool operator()(TAtom const &x, Id_t const &y) const {
            return operator()(y, x);
        }

        bool operator()(Id_t const &x, TAtomG const &y) const {
            auto const &atom = **(data->begin() + x);
            return atom.guard() != nullptr &&
                   *atom.guard() == std::get<2>(y) &&
                   *atom.rhs() == std::get<3>(y) &&
                   atom.term() == std::get<0>(y) &&
                   atom.size() == std::get<1>(y).size &&
                   std::equal(atom.begin(), atom.end(), begin(std::get<1>(y)));
        }

        bool operator()(TAtomG const &x, Id_t const &y) const {
            return operator()(y, x);
        }

        Potassco::TheoryData const *data;
    };

    using TermSet = hash_set<Id_t, TermHash, TermEqual>;
    using ElementSet = hash_set<Id_t, ElementHash, ElementEqual>;
    using AtomSet = hash_set<Id_t, AtomHash, AtomEqual>;
    using ConditionVec = std::vector<LitVec>;
    using PrintLit = std::function<void (std::ostream &out, LiteralId const &)>;

public:
    TheoryData(Potassco::TheoryData &data);

    Id_t addTerm(int number);
    Id_t addTerm(char const *name);
    Id_t addTermFun(Id_t funcSym, IdSpan const& terms);
    Id_t addTermTup(Potassco::Tuple_t type, IdSpan const& terms);
    Id_t addTerm(Symbol value);
    Id_t addElem(IdSpan const &tuple, Potassco::LitSpan const &cond);
    Id_t addElem(IdSpan const &tuple, LitVec cond);
    std::pair<Potassco::TheoryAtom const &, bool> addAtom(std::function<Id_t()> const &newAtom, Id_t termId, IdSpan const &elems);
    std::pair<Potassco::TheoryAtom const &, bool> addAtom(std::function<Id_t()> const &newAtom, Id_t termId, IdSpan const &elems, Id_t op, Id_t rhs);
    void printTerm(std::ostream &out, Id_t termId) const;
    void printElem(std::ostream &out, Id_t elemId, PrintLit printLit) const;
    bool empty() const;
    LitVec const &getCondition(Id_t elemId) const;
    Potassco::TheoryData const &data() const;
    void setCondition(Id_t elementId, Id_t newCond);
    void reset(bool resetData);
    void output(TheoryOutput &tout);
    Potassco::TheoryAtom const &getAtom(Id_t offset) const {
        return **(data_.begin() + offset);
    }
    template <class F>
    void updateCondition(Id_t elemId, F update) {
        elems_.erase(elemId);
        update(conditions_[elemId]);
        elems_.insert(elemId);
    }

private:
    void print(Id_t termId, const Potassco::TheoryTerm& term);
    void print(const Potassco::TheoryAtom& a);
    void visit(Potassco::TheoryData const &data, Id_t termId, Potassco::TheoryTerm const &t) override;
    void visit(Potassco::TheoryData const &data, Id_t elemId, Potassco::TheoryElement const &e) override;
    void visit(Potassco::TheoryData const &data, Potassco::TheoryAtom const &a) override;

    template <typename ...Args>
    Id_t addTerm_(Args ...args);
    template <typename ...Args>
    std::pair<Potassco::TheoryAtom const &, bool> addAtom_(std::function<Id_t()> const &newAtom, Args ...args);

    Potassco::TheoryData &data_;
    TermSet terms_;
    ElementSet elems_;
    ConditionVec conditions_;
    AtomSet atoms_;
    std::vector<bool> tSeen_;
    std::vector<bool> eSeen_;
    TheoryOutput *out_;
    uint32_t aSeen_;
};

// {{{1 declaration of TheoryTerm

class TheoryParser;
class TheoryTerm;
using UTheoryTerm = std::unique_ptr<TheoryTerm>;
using UTheoryTermVec = std::vector<UTheoryTerm>;
class TheoryTerm : public Hashable, public Comparable<TheoryTerm>, public Printable, public Clonable<TheoryTerm> {
public:
    virtual Id_t eval(TheoryData &data, Logger &log) const = 0;
    virtual void collect(VarTermBoundVec &vars) = 0;
    virtual void replace(Defines &defs) = 0;
    virtual UTheoryTerm initTheory(TheoryParser &p, Logger &log) = 0;
};

// {{{1 declaration of RawTheoryTerm

class RawTheoryTerm : public TheoryTerm {
public:
    using ElemVec = std::vector<std::pair<StringVec, UTheoryTerm>>;

    RawTheoryTerm() = default;
    RawTheoryTerm(ElemVec elems);

    void append(StringVec ops, UTheoryTerm term);
    // {{{2 TheoryTerm interface
    Id_t eval(TheoryData &data, Logger &log) const override;
    void collect(VarTermBoundVec &vars) override;
    void replace(Defines &defs) override;
    UTheoryTerm initTheory(TheoryParser &p, Logger &log) override;
    // {{{2 Hashable interface
    size_t hash() const override;
    // {{{2 Comparable interface
    bool operator==(TheoryTerm const &other) const override;
    // {{{2 Printable interface
    void print(std::ostream &out) const override;
    // {{{2 Clonable interface
    RawTheoryTerm *clone() const override;
    // }}}2
private:
    ElemVec elems_;
};

class TheoryParser {
    enum TokenType { Op, Id };
    class Elem {
    public:
        Elem(String op, bool unary);
        Elem(UTheoryTerm term);
        Elem(Elem const &elem) = delete;
        Elem(Elem &&elem) noexcept;
        Elem &operator=(Elem const &elem) = delete;
        Elem &operator=(Elem &&elem) noexcept = delete;
        ~Elem() noexcept;

        TokenType type();
        std::pair<String,bool> &op();
        UTheoryTerm &term();

    private:
        TokenType tokenType_;
        union {
            std::pair<String,bool> op_;
            UTheoryTerm term_;
        };
    };
    using Stack = std::vector<Elem>;
public:
    TheoryParser(Location const &loc, TheoryTermDef const &def);
    UTheoryTerm parse(RawTheoryTerm::ElemVec elems, Logger &log);

private:
    void reduce();
    bool check(String op);

    Location loc_;
    TheoryTermDef const &def_;
    Stack stack_;
};

// {{{1 declaration of UnaryTheoryTerm

class UnaryTheoryTerm : public TheoryTerm {
public:
    UnaryTheoryTerm(String op, UTheoryTerm arg);

    // {{{2 TheoryTerm interface
    Id_t eval(TheoryData &data, Logger &log) const override;
    void collect(VarTermBoundVec &vars) override;
    void replace(Defines &defs) override;
    UTheoryTerm initTheory(TheoryParser &p, Logger &log) override;
    // {{{2 Hashable interface
    size_t hash() const override;
    // {{{2 Comparable interface
    bool operator==(TheoryTerm const &other) const override;
    // {{{2 Printable interface
    void print(std::ostream &out) const override;
    // {{{2 Clonable interface
    UnaryTheoryTerm *clone() const override;
    // }}}2

private:
    UTheoryTerm arg_;
    String op_;
};

// {{{1 declaration of BinaryTheoryTerm

class BinaryTheoryTerm : public TheoryTerm {
public:
    BinaryTheoryTerm(UTheoryTerm left, String op, UTheoryTerm right);

    // {{{2 TheoryTerm interface
    Id_t eval(TheoryData &data, Logger &log) const override;
    void collect(VarTermBoundVec &vars) override;
    void replace(Defines &defs) override;
    UTheoryTerm initTheory(TheoryParser &p, Logger &log) override;
    // {{{2 Hashable interface
    size_t hash() const override;
    // {{{2 Comparable interface
    bool operator==(TheoryTerm const &other) const override;
    // {{{2 Printable interface
    void print(std::ostream &out) const override;
    // {{{2 Clonable interface
    BinaryTheoryTerm *clone() const override;
    // }}}2

private:
    UTheoryTerm left_;
    UTheoryTerm right_;
    String op_;
};

// {{{1 declaration of TupleTheoryTerm

class TupleTheoryTerm : public TheoryTerm {
public:
    using Type = Potassco::Tuple_t;

    TupleTheoryTerm(Type type, UTheoryTermVec args);

    // {{{2 TheoryTerm interface
    Id_t eval(TheoryData &data, Logger &log) const override;
    void collect(VarTermBoundVec &vars) override;
    void replace(Defines &defs) override;
    UTheoryTerm initTheory(TheoryParser &p, Logger &log) override;
    // {{{2 Hashable interface
    size_t hash() const override;
    // {{{2 Comparable interface
    bool operator==(TheoryTerm const &other) const override;
    // {{{2 Printable interface
    void print(std::ostream &out) const override;
    // {{{2 Clonable interface
    TupleTheoryTerm *clone() const override;
    // }}}2

private:
    UTheoryTermVec args_;
    Type type_;
};

// {{{1 declaration of FunctionTheoryTerm

class FunctionTheoryTerm : public TheoryTerm {
public:
    FunctionTheoryTerm(String name, UTheoryTermVec args);

    // {{{2 TheoryTerm interface
    Id_t eval(TheoryData &data, Logger &log) const override;
    void collect(VarTermBoundVec &vars) override;
    void replace(Defines &defs) override;
    UTheoryTerm initTheory(TheoryParser &p, Logger &log) override;
    // {{{2 Hashable interface
    size_t hash() const override;
    // {{{2 Comparable interface
    bool operator==(TheoryTerm const &other) const override;
    // {{{2 Printable interface
    void print(std::ostream &out) const override;
    // {{{2 Clonable interface
    FunctionTheoryTerm *clone() const override;
    // }}}2

private:
    UTheoryTermVec args_;
    String name_;
};

// }}}1
// {{{1 declaration of TermTheoryTerm

class TermTheoryTerm : public TheoryTerm {
public:
    TermTheoryTerm(UTerm term);

    // {{{2 TheoryTerm interface
    Id_t eval(TheoryData &data, Logger &log) const override;
    void collect(VarTermBoundVec &vars) override;
    void replace(Defines &defs) override;
    UTheoryTerm initTheory(TheoryParser &p, Logger &log) override;
    // {{{2 Hashable interface
    size_t hash() const override;
    // {{{2 Comparable interface
    bool operator==(TheoryTerm const &other) const override;
    // {{{2 Printable interface
    void print(std::ostream &out) const override;
    // {{{2 Clonable interface
    TermTheoryTerm *clone() const override;
    // }}}2

private:
    UTerm term_;
};

// }}}1

} } // namespace Output Gringo

GRINGO_CALL_HASH(Gringo::Output::TheoryTerm)

#endif // GRINGO_OUTPUT_THEORY_HH
