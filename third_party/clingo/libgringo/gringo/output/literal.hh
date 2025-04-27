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

#ifndef GRINGO_OUTPUT_LITERAL_HH
#define GRINGO_OUTPUT_LITERAL_HH

#include "potassco/basic_types.h"
#include <algorithm>
#include <gringo/domain.hh>
#include <gringo/output/types.hh>
#include <potassco/theory_data.h>
#include <unordered_map>

namespace Gringo { namespace Output {

// TODO: There should not be any forward declarations here.
class DomainData;
class Translator;

// {{{1 declaration of Mapping

class Mapping {
public:
    void add(Id_t oldOffset, Id_t newOffset) {
        // Note: this assumes that identical indices are added in order.
        // (This is particular to the unordered_delete of the ordered_set being used for remapping.)
        if (oldOffset == newOffset) {
            if (id_.empty() || id_.back().second < oldOffset - 1) {
                id_.emplace_back(oldOffset, oldOffset);
            }
            else {
                assert(id_.back().second == oldOffset - 1);
                ++id_.back().second;
            }
        }
        else {
            assert(map_.find(oldOffset) == map_.end());
            map_.emplace(oldOffset, newOffset);
        }
    }
    Id_t get(Id_t oldOffset) const {
        auto it = map_.find(oldOffset);
        if (it != map_.end()) {
            return it->second;
        }
        auto jt = std::lower_bound(
            id_.begin(),
            id_.end(),
            oldOffset,
            [](std::pair<Id_t, Id_t> const &a, Id_t const &b) { return a.second < b; });
        if (jt != id_.end() && jt->first <= oldOffset) {
            assert(oldOffset <= jt->second);
            return oldOffset;
        }
        return InvalidId;
    }
private:
    hash_map<Id_t, Id_t> map_;
    std::vector<std::pair<Id_t, Id_t>> id_;
};
using Mappings = std::vector<Mapping>;

// {{{1 declaration of PredicateAtom

struct PredicateAtom {
public:
    // {{{2 Atom interface

    // Constructs a valid atom without uid and generation.
    PredicateAtom(Symbol value)
    : value_(value)
    , uid_(0)
    , fact_(false)
    , generation_(0)
    , external_(false)
    , delayed_(false) { }

    // Functions that have to be implemented by all atoms.

    // Atoms can be marked as facts.
    // Atoms that are not monotone (like some aggregate atoms) are not considered
    // monotone in the recursive case because their fact status can still change
    // during grounding
    bool fact() const {
        return fact_;
    }
    // Defined and undefined atoms are distinguished.
    // Only recursion through negative literals can lead to undefined atoms.
    // Such atoms must not be imported in indices.
    // They can be defined later though, in which case they have to be imported.
    // Example: a :- not b.  b :- not a.
    bool defined() const {
        return generation_ > 0;
    }
    // The generation of the atom. This value is used by indices to determine
    // what is new and old.
    Id_t generation() const {
        assert(defined());
        return generation_ - 1;
    }
    void setGeneration(unsigned x) {
        generation_ = x + 1;
    }
    void markDelayed() {
        delayed_ = 1;
    }
    bool delayed() const {
        return delayed_;
    }
    // Returns the value associated with the atom.
    operator Symbol const &() const {
        return value_;
    }
    // }}}2

    // The functions below are not necessary for domain elements.
    // This are additional functions in symbolic atoms.

    void setFact(bool x) {
        fact_ = x;
    }
    // These functions are used to handle external atoms.
    // The sole reason for this function is to workaround a problem with libclasp,
    // which might find equivalences between external and non-external atoms.
    // In such a case it is not possible to determine which atom is the external
    // one using the clasp API. Note that the external status is never unset;
    // only the solver knows about this.
    bool isExternal() const {
        return external_;
    }
    void setExternal(bool x) {
        external_ = x;
    }
    // The uid of an atom is used in various outputs.
    bool hasUid() const {
        return uid_ > 0;
    }
    void setUid(unsigned x) {
        assert(!hasUid());
        uid_ = x + 1;
    }
    void resetUid(unsigned x) {
        assert(hasUid());
        uid_ = x + 1;
    }
    Id_t uid() const {
        assert(hasUid());
        return uid_ - 1;
    }
    void unmarkDelayed() {
        delayed_ = 0;
    }
private:
    Symbol value_;
    uint32_t uid_ : 31;
    uint32_t fact_ : 1;
    uint32_t generation_ : 30;
    uint32_t external_ : 1;
    uint32_t delayed_ : 1;
};

// {{{1 declaration of PredicateDomain

class PredicateDomain : public AbstractDomain<PredicateAtom> {
public:
    explicit PredicateDomain(Sig sig)
    : sig_(sig) { }

    using AbstractDomain<PredicateAtom>::define;
    // Defines (adds) an atom setting its generation and fact status.
    // Returns a tuple indicating its postion, wheather it was inserted, and wheather it was a fact before.
    std::tuple<Iterator, bool, bool> define(Symbol x, bool fact) {
        auto ret = define(x);
        bool wasfact = !ret.second && ret.first->fact();
        if (fact) { ret.first->setFact(true); }
        return std::forward_as_tuple(ret.first, ret.second, wasfact);
    }

    // This offset divides elements added at the current and previous incremental steps.
    // It is used to only output newly inserted atoms, for projection, and classical negation.
    SizeType incOffset() const {
        return incOffset_;
    }

    void incNext() {
        incOffset_ = size();
    }

    // This offset keeps track of atoms already added to the output table.
    // TODO: maybe this can be merged with incNext.
    SizeType showOffset() const {
        return showOffset_;
    }

    void showNext() {
        showOffset_ = size();
    }

    void clear() {
        AbstractDomain<PredicateAtom>::clear();
        incOffset_  = 0;
        showOffset_ = 0;
    }

    Sig const &sig() const {
        return sig_;
    }

    operator Sig const &() const {
        return sig_;
    }

    std::pair<Id_t, Id_t> cleanup(AssignmentLookup assignment, Mapping &map);
private:
    Sig sig_;
    SizeType incOffset_ = 0;
    SizeType showOffset_ = 0;
};
using UPredDom = std::unique_ptr<PredicateDomain>;

struct UPredDomHash : std::hash<Sig> {
    using std::hash<Sig>::operator();
    size_t operator()(UPredDom const &dom) const {
        return std::hash<Sig>::operator()(*dom);
    }
};

// NOLINTNEXTLINE(modernize-use-transparent-functors)
struct UPredDomEqualTo : private std::equal_to<Sig> {
    using is_transparent = void;
    bool operator()(UPredDom const &a, Sig const &b) const {
        return std::equal_to<Sig>::operator()(*a, b);
    }
    bool operator()(Sig const &a, UPredDom const &b) const {
        return std::equal_to<Sig>::operator()(a, *b);
    }
    bool operator()(UPredDom const &a, UPredDom const &b) const {
        return std::equal_to<Sig>::operator()(*a, *b);
    }
};

// TODO: using a map could avoid some akwardness in usage
using PredDomMap = ordered_set<UPredDom, mix_hash<UPredDom, UPredDomHash>, UPredDomEqualTo>;

// {{{1 declaration of PrintPlain

struct PrintPlain {
    void printTerm(Potassco::Id_t x);
    void printElem(Potassco::Id_t x);
    DomainData   &domain;
    std::ostream &stream;
};

template <typename T>
PrintPlain &operator<<(PrintPlain &out, T const &x) {
    out.stream << x;
    return out;
}

// {{{1 declaration of LiteralId

enum class AtomType : uint32_t {
    BodyAggregate,
    AssignmentAggregate,
    HeadAggregate,
    Disjunction,
    Conjunction,
    Theory,
    Predicate,
    Aux
    // TODO: consider a hidden predicate domain for internal purposes
};

class LiteralId;
using LitVec = std::vector<LiteralId>;
using LitSpan = Potassco::Span<LiteralId>;

class LiteralId {
public:
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    explicit LiteralId(uint64_t repr)
    : repr_(repr) { }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    LiteralId()
    : repr_(std::numeric_limits<uint64_t>::max()) { }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    LiteralId(NAF sign, AtomType type, Potassco::Id_t offset, Potassco::Id_t domain)
    : data_{static_cast<uint32_t>(sign), static_cast<uint32_t>(type), domain, offset} { }
    Potassco::Id_t offset() const {
        return data_.offset;
    }
    Potassco::Id_t domain() const {
        return data_.domain;
    }
    AtomType type() const {
        return static_cast<AtomType>(data_.type);
    }
    NAF sign() const {
        return static_cast<NAF>(data_.sign);
    }
    bool invertible() const {
        return sign() != NAF::POS;
    }
    LiteralId negate(bool recursive=true) const {
        return {inv(sign(), recursive), type(), offset(), domain()};
    }
    bool operator==(LiteralId const &other) const {
        return repr_ == other.repr_;
    }
    bool operator<(LiteralId const &other) const {
        return repr_ < other.repr_;
    }
    uint64_t repr() const {
        return repr_;
    }
    bool valid() const {
        return repr_ != std::numeric_limits<uint64_t>::max();
    }
    LiteralId withSign(NAF naf) const {
        return {naf, type(), offset(), domain()};
    }
    LiteralId withOffset(Id_t offset) const {
        return {sign(), type(), offset, domain()};
    }
    operator bool() const {
        return valid();
    }
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)

private:
    struct Data {
        uint32_t sign   : 2;
        uint32_t type   : 6;
        uint32_t domain : 24;
        uint32_t offset;
    };
    union {
        Data data_;
        uint64_t repr_;
    };
};
using LitVec  = std::vector<LiteralId>;

// {{{1 declaration of Literal

class Literal {
public:
    Literal() = default;
    Literal(Literal const &other) = default;
    Literal(Literal &&other) noexcept = default;
    Literal &operator=(Literal const &other) = default;
    Literal &operator=(Literal &&other) noexcept = default;
    virtual ~Literal() noexcept = default;

    // Shall return true if the literal was defined in a previous step.
    // Modularity guarantees that there is no cycle involing atoms from
    // the current step through such a literal.
    virtual bool isAtomFromPreviousStep() const;
    // Shall return true for all literals that can be used in the head of a rule.
    virtual bool isHeadAtom() const;
    // Translates the literal into a literal suitable to be output by a Backend.
    virtual LiteralId translate(Translator &x) = 0;
    // Compact representation for the literal.
    // A literal can be restored from this representation using the DomainData class.
    virtual LiteralId toId() const = 0;
    // Prints the literal in plain text format.
    virtual void printPlain(PrintPlain out) const = 0;
    // Shall return if the literal cannot be output at this point of time.
    // Such literals are stored in a list and output once their definition is complete.
    virtual bool isIncomplete() const;
    // Associates an incomplete literals with a delayd literal.
    // The return flag indicates that a fresh delayed literal has been added.
    // The literal might copy its sign into the delayed literal.
    // In this case the sign is stripped from the original literal.
    virtual std::pair<LiteralId,bool> delayedLit();
    virtual bool needsSemicolon() const;
    virtual bool isPositive() const;
    // Maps true and false literals to a unique literal and remaps the offsets of predicate literals.
    virtual LiteralId simplify(Mappings &mappings, AssignmentLookup const &lookup) const;
    virtual bool isTrue(IsTrueLookup const &lookup) const;
    virtual Lit_t uid() const = 0;
};

// returns true if all literals in lits are true
bool isTrueClause(DomainData &data, LitVec &lits, IsTrueLookup const &lookup);

// {{{1 declaration of TupleId

struct TupleId {
    Id_t offset;
    Id_t size;
};
inline bool operator==(TupleId a, TupleId b) {
    return a.offset == b.offset && a.size == b.size;
}
inline bool operator<(TupleId a, TupleId b) {
    if (a.offset != b.offset) { return a.offset < b.offset; }
    return a.size < b.size;
}

// }}}1

} } // namespace Output Gringo

namespace std {

template <>
struct hash<Gringo::Output::LiteralId> : private std::hash<uint64_t> {
    size_t operator()(Gringo::Output::LiteralId const &lit) const {
        return std::hash<uint64_t>::operator()(lit.repr());
    }
};

template <>
struct hash<Gringo::Output::TupleId> {
    size_t operator()(Gringo::Output::TupleId t) const {
        return Gringo::get_value_hash(t.offset, t.size);
    }
};

} // namespace std

#endif // GRINGO_OUTPUT_LITERAL_HH
