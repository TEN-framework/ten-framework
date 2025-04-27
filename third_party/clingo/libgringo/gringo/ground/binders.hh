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

#ifndef GRINGO_GROUND_BINDERS_HH
#define GRINGO_GROUND_BINDERS_HH

#include <gringo/domain.hh>
#include <gringo/ground/instantiation.hh>

namespace Gringo { namespace Ground {

// {{{ definition of PosBinder

template <class Index, class... LookupArgs>
struct PosBinder : Binder {
    using IndexType = typename std::remove_reference<Index>::type;
    using Match     = typename IndexType::SizeType;
    using MatchRng  = typename IndexType::OffsetRange;
    using Lookup    = std::tuple<Index, LookupArgs...>;

    PosBinder(UTerm &&repr, Match &result, Index &&index, BinderType type, LookupArgs&&... args)
        : repr(std::move(repr))
        , result(result)
        , index(std::forward<Index>(index), std::forward<LookupArgs>(args)...)
        , type(type) { }

    template <       int... I> struct lookup;
    template <int N, int... I> struct lookup<N, I...> : lookup<N-1, N, I...> { };
    template <       int... I> struct lookup<0, I...> {
        MatchRng operator()(std::tuple<Index, LookupArgs...> &index, BinderType type, Logger &log) {
            return std::get<0>(index).lookup(std::get<I>(index)..., type, log);
        }
    };

    IndexUpdater *getUpdater() override {
        return &std::get<0>(index);
    }

    void match(Logger &log) override {
        current = lookup<sizeof...(LookupArgs)>()(index, type, log);
    }

    bool next() override {
        return current.next(result, *repr, std::get<0>(index));
    }

    void print(std::ostream &out) const override {
        out << *repr << "@" << type;
    }

    UTerm      repr; // problematic
    Match     &result;
    Lookup     index;
    MatchRng   current;
    BinderType type;
};

// }}}
// {{{ definition of Matcher

template <class Atom>
struct Matcher : Binder {
    using DomainType = AbstractDomain<Atom>;
    using Match      = typename DomainType::SizeType;

    Matcher(Match &result, DomainType &domain, Term const &repr, RECNAF naf)
        : result(result)
        , domain(domain)
        , repr(repr)
        , naf(naf) { }

    IndexUpdater *getUpdater() override {
        return nullptr;
    }

    void match(Logger &log) override {
        firstMatch = domain.lookup(result, repr, naf, log);
    }

    bool next() override {
        bool ret = firstMatch;
        firstMatch = false;
        return ret;
    }

    void print(std::ostream &out) const override {
        out << naf << repr << "[" << domain.generation() << "/" << domain.size() << "]" << "@ALL";
    }

    Match      &result;
    DomainType &domain;
    Term const &repr;
    RECNAF      naf;
    bool        firstMatch = false;
};

// }}}
// {{{ definition of PosMatcher

template <class Atom>
struct PosMatcher : Binder, IndexUpdater {
    using DomainType = AbstractDomain<Atom>;
    using Match = typename DomainType::SizeType;

    PosMatcher(Match &result, DomainType &domain, UTerm &&repr, BinderType type)
        : result(result)
        , domain(domain)
        , repr(std::move(repr))
        , type(type) { }

    IndexUpdater *getUpdater() override {
        return type == BinderType::NEW ? this : nullptr;
    }

    void match(Logger &log) override {
        firstMatch = domain.lookup(result, *repr, type, log);
    }

    bool next() override {
        bool ret = firstMatch;
        firstMatch = false;
        return ret;
    }

    bool update() override {
        return domain.update([](unsigned) { }, *repr, imported, importedDelayed);
    }

    void print(std::ostream &out) const override {
        out << *repr << "[" << domain.generation() << "/" << domain.size() << "]" << "@" << type;
    }

    Match      &result;
    DomainType &domain;
    UTerm       repr;
    BinderType  type;
    unsigned    imported = 0;
    unsigned    importedDelayed = 0;
    bool        firstMatch = false;
};

// }}}
// {{{ definition of make_binder

// Note: does not (quite) work this way
//       but it should be enough to make imported part of the key of full_indices

template <class Atom>
inline UIdx make_binder(AbstractDomain<Atom> &domain, NAF naf, Term const &repr, typename AbstractDomain<Atom>::SizeType &elem, BinderType type, bool recursive, Term::VarSet &bound, int imported) {
    using DomainType          = AbstractDomain<Atom>;
    using PredicateMatcher    = Matcher<Atom>;
    using PosPredicateMatcher = PosMatcher<Atom>;
    using PosPredicateBinder  = PosBinder<typename DomainType::BindIndex&, SValVec>;
    using FullPredicateBinder = PosBinder<typename DomainType::FullIndex&>;
    if (naf == NAF::POS) {
        UTerm predClone(repr.clone());
        VarTermBoundVec occs;
        predClone->collect(occs, false);
        bool hasUnbound = false;
        for (auto &x : occs) {
            if ((x.first->bindRef = bound.find(x.first->name) == bound.end())) {
                hasUnbound = true;
            }
        }
        if (hasUnbound) {
            Term::VarSet occBoundSet;
            VarTermVec occBound;
            for (auto &x : occs) {
                if (x.first->bindRef)                               { x.first->bindRef = bound.emplace(x.first->name).second; }
                else if (occBoundSet.emplace(x.first->name).second) { occBound.emplace_back(*x.first); }
            }
            Term::RenameMap rename;
            UTerm idxClone(predClone->renameVars(rename));
            SValVec predBound;
            SValVec idxBound;
            for (VarTerm &x : occBound) {
                auto it(rename.find(x.name));
                predBound.emplace_back(x.ref);
                idxBound.emplace_back(it->second.second);
            }
            Term::VarSet empty;
            idxClone->bind(empty);
            if (occBound.empty()) {
                auto &idx(domain.add(std::move(idxClone), imported));
                return gringo_make_unique<FullPredicateBinder>(std::move(predClone), elem, idx, type);
            }
            assert(imported == 0);
            auto &idx(domain.add(std::move(idxBound), std::move(idxClone)));
            return gringo_make_unique<PosPredicateBinder>(std::move(predClone), elem, idx, type, std::move(predBound));
        }
        if (recursive) {
            assert(imported == 0);
            Term::VarSet empty;
            predClone->bind(empty);
            return gringo_make_unique<PosPredicateMatcher>(elem, domain, std::move(predClone), type);
        }
        assert(imported == 0);
        return gringo_make_unique<PredicateMatcher>(elem, domain, repr, RECNAF::POS);
    }
    assert(imported == 0);
    return gringo_make_unique<PredicateMatcher>(elem, domain, repr, recnaf(naf, recursive));
}

// }}}

} } // namespace Ground Gringo

#endif // GRINGO_GROUND_BINDERS_HH
