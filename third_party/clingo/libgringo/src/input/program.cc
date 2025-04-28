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

#include "gringo/input/program.hh"
#include "gringo/input/literals.hh"
#include "gringo/input/aggregates.hh"
#include "gringo/ground/literal.hh"
#include "gringo/ground/statements.hh"
#include "gringo/term.hh"
#include "gringo/logger.hh"
#include "gringo/graph.hh"
#include "gringo/safetycheck.hh"

namespace Gringo { namespace Input {

// {{{ definition of Block

Ground::SEdb Block::make_sig() const {
    auto edb = std::make_shared<Ground::SEdb::element_type>(nullptr, SymVec());
    UTermVec args;
    for (auto const &param : params) {
        args.emplace_back(make_locatable<ValTerm>(param.first, Symbol::createId(param.second)));
    }
    if (args.empty()) {
        std::get<0>(*edb) = make_locatable<ValTerm>(loc, Symbol::createId(name));
    }
    else {
        std::get<0>(*edb) = make_locatable<FunctionTerm>(loc, name, std::move(args));
    }
    return edb;
}

// }}}
// {{{ definition of Program

Program::Program() {
    begin(Location("<internal>", 1, 1, "<internal>", 1, 1), "base", IdVec({}));
}

bool Program::empty() const {
    return blocks_.empty() && sigs_.empty() && theoryDefs_.empty();
}

void Program::begin(Location const &loc, String name, IdVec &&params) {
    auto block = Block{loc, (std::string("#inc_") + name.c_str()).c_str(), std::move(params)};
    auto sig = block.make_sig();
    current_ = &blocks_.try_emplace(std::move(sig), std::move(block)).first.value();
}

void Program::add(UStm &&stm) {
    current_->addedEdb.emplace_back(stm->isEDB());
    if (current_->addedEdb.back().type() == SymbolType::Special) {
        current_->addedStms.emplace_back(std::move(stm));
        current_->addedEdb.pop_back();
    }
}

void Program::addInput(Sig sig) {
    sigs_.insert(sig);
}

void Program::add(TheoryDef &&def, Logger &log) {
    auto it = theoryDefs_.find(def.name());
    if (it == theoryDefs_.end()) {
        theoryDefs_.insert(std::move(def));
    }
    else {
        GRINGO_REPORT(log, Warnings::RuntimeError)
            << def.loc() << ": error: redefinition of theory:" << "\n"
            << "  " << def.name() << "\n"
            << it->loc() << ": note: theory first defined here\n";
    }

}

void Program::rewrite(Defines &defs, Logger &log) {
    for (auto it = blocks_.begin(), ie = blocks_.end(); it != ie; ++it) {
        auto &block  = it.value();
        auto const &sig = it.key();
        // replacing definitions
        Defines incDefs;

        UTermVec args;
        AuxGen gen;
        for (auto const &param : block.params) {
            args.emplace_back(gen.uniqueVar(param.first, 0, "#Inc"));
            incDefs.add(param.first, param.second, get_clone(args.back()), false, log);
        }
        sigs_.insert(Sig(block.name, static_cast<uint32_t>(args.size()), false));
        UTerm blockTerm(args.empty()
            ? (UTerm)make_locatable<ValTerm>(block.loc, Symbol::createId(block.name))
            : make_locatable<FunctionTerm>(block.loc, block.name, get_clone(args)));
        VarTermBoundVec blockBound;
        blockTerm->collect(blockBound, true);
        incDefs.init(log);

        for (auto &fact : block.addedEdb) {
            sigs_.insert(fact.sig());
        }
        auto replace = [&](Defines &defs, Symbol fact) -> Symbol {
            if (defs.empty() || fact.type() == SymbolType::Special) { return fact; }
            UTerm rt;
            Symbol rv;
            defs.apply(fact, rv, rt, false);
            if (rt) {
                Location loc{rt->loc()};
                block.addedStms.emplace_back(make_locatable<Statement>(loc, gringo_make_unique<SimpleHeadLiteral>(make_locatable<PredicateLiteral>(loc, NAF::POS, std::move(rt))), UBodyAggrVec{}));
                return {};
            }
            if (rv.type() != SymbolType::Special) {
                return rv;
            }
            return fact;
        };
        if (!defs.empty() || !incDefs.empty()) {
            for (auto &fact : block.addedEdb) {
                Symbol rv = replace(incDefs, replace(defs, fact));
                if (rv.type() != SymbolType::Special) {
                    std::get<1>(*sig).emplace_back(rv);
                }
            }
            block.addedEdb.clear();
        }
        else if (std::get<1>(*sig).empty()) {
            std::swap(std::get<1>(*sig), block.addedEdb);
        }
        else {
            std::copy(block.addedEdb.begin(), block.addedEdb.end(), std::back_inserter(std::get<1>(*sig)));
        }
        // rewriting
        // steps:
        // 1. unpool
        // 2. initialize theory
        // 3. simplify
        // 4. shift
        // 5. unpool comparisions
        // 6. rewrite aggregates
        auto rewrite2 = [&](UStm &x) -> void {
            std::get<1>(*sig).emplace_back(x->isEDB());
            if (std::get<1>(*sig).back().type() == SymbolType::Special) {
                x->add(make_locatable<PredicateLiteral>(block.loc, NAF::POS, get_clone(blockTerm), true));
                x->rewrite();
                block.stms.emplace_back(std::move(x));
                std::get<1>(*sig).pop_back();
            }
            else {
                sigs_.insert(std::get<1>(*sig).back().sig());
            }
        };
        auto rewrite1 = [&](UStm &x) -> void {
            x->initTheory(theoryDefs_, log);
            if (x->simplify(project_, log)) {
                for (auto &y : x->unpoolComparison()) {
                    rewrite2(y);
                }
            }
        };
        for (auto &x : block.addedStms) {
            x->replace(defs);
            x->replace(incDefs);
            x->assignLevels(blockBound);
            if (x->hasPool()) {
                for (auto &y : x->unpool()) {
                    rewrite1(y);
                }
            }
            else {
                rewrite1(x);
            }
        }
        block.addedStms.clear();
    }
    // projection
    for (auto it = project_.begin(), ie = project_.end(); it != ie; ++it) {
        if (!it.value().second) {
            Location loc(it.value().first->loc());
            UBodyAggrVec body;
            body.emplace_back(gringo_make_unique<SimpleBodyLiteral>(make_locatable<ProjectionLiteral>(loc, get_clone(it.value().first))));
            stms_.emplace_back(make_locatable<Statement>(
                loc,
                gringo_make_unique<SimpleHeadLiteral>(make_locatable<PredicateLiteral>(loc, NAF::POS, get_clone(it.key()))),
                std::move(body)));
            it.value().second = true;
        }
    }
    // }}}3
}

void Program::check(Logger &log) {
    for (auto const &block : blocks_) {
        for (auto const &stm : block.second.stms) {
            stm->check(log);
        }
    }
    std::unordered_map<Sig, Location> seenSigs;
    for (auto &def : theoryDefs_) {
        for (auto const &atomDef : def.atomDefs()) {
            auto seenSig = seenSigs.emplace(atomDef.sig(), atomDef.loc());
            if (!seenSig.second) {
                GRINGO_REPORT(log, Warnings::RuntimeError)
                    << atomDef.loc() << ": error: multiple definitions for theory atom:" << "\n"
                    << "  " << atomDef.sig() << "\n"
                    << seenSig.first->second << ": note: first defined here\n";
            }
        }
    }
}

void Program::print(std::ostream &out) const {
    for (auto const &def : theoryDefs_) {
        out << def << "\n";
    }
    for (auto const &block : blocks_) {
        for (auto const &x : block.second.addedEdb)     { out << x << "." << "\n"; }
        for (auto const &x : std::get<1>(*block.first)) { out << x << "." << "\n"; }
        for (auto const &x : block.second.addedStms)    { out << *x << "\n"; }
        for (auto const &x : block.second.stms)         { out << *x << "\n"; }
    }
    for (auto const &x : stms_) { out << *x << "\n"; }
}

// Defines atoms that have been seen in earlier steps
class DummyStatement : public Ground::Statement, private Ground::HeadOccurrence {
public:
    DummyStatement(UGTermVec terms, bool normal)
    : terms_{std::move(terms)}
    , normal_{normal}  {}

    bool isNormal() const override {
        return normal_;
    }

    void analyze(Dep::Node &node, Dep &dep) override {
        for (auto &term : terms_) {
            dep.provides(node, *this, get_clone(term));
        }
    }

    void startLinearize(bool active) override {
        static_cast<void>(active);
    }

    void linearize(Context &context, bool positive, Logger &log) override {
        static_cast<void>(context);
        static_cast<void>(positive);
        static_cast<void>(log);
    }

    void enqueue(Ground::Queue &q) override {
        static_cast<void>(q);
    }

    void print(std::ostream &out) const override {
        print_comma(out, terms_, ";", [](std::ostream &out, UGTerm const &term) { out << *term; });
        out << ".";
    }

private:
    void defines(IndexUpdater &update, Ground::Instantiator *inst) override {
        static_cast<void>(update);
        static_cast<void>(inst);
    }

    UGTermVec terms_;
    bool normal_;
};

Ground::Program Program::toGround(std::set<Sig> const &sigs, DomainData &domains, Logger &log) {
    Ground::UStmVec stms;
    if (!pheads.empty()) {
        stms.emplace_back(gringo_make_unique<DummyStatement>(std::move(pheads), true));
    }
    if (!nheads.empty()) {
        stms.emplace_back(gringo_make_unique<DummyStatement>(std::move(nheads), false));
    }
    stms.emplace_back(make_locatable<Ground::ExternalRule>(Location("#external", 1, 1, "#external", 1, 1)));
    ToGroundArg arg(auxNames_, domains);
    Ground::SEdbVec edb;
    for (auto const &block : blocks_) {
        if (sigs.find(Sig{block.second.name.c_str() + 5, numeric_cast<uint32_t>(block.second.params.size()), false}) != sigs.end()) { // NOLINT
            edb.emplace_back(block.first);
            for (auto const &x : block.second.stms) {
                x->toGround(arg, stms);
            }
        }
    }
    for (auto &x : stms_) {
        x->toGround(arg, stms);
    }
    Ground::Statement::Dep dep;
    for (auto &x : stms) {
        bool normal(x->isNormal());
        auto &node(dep.add(std::move(x), normal));
        node.stm->analyze(node, dep);
    }
    auto ret = dep.analyze();
    Ground::Program prg(std::move(edb), std::move(std::get<0>(ret)));
    pheads = std::move(std::get<1>(ret));
    nheads = std::move(std::get<2>(ret));
    for (auto const &sig : sigs_) {
        domains.add(sig);
    }
    Ground::UndefVec undef;
    for (auto const &x : dep.depend) {
        for (auto const &y : x.second.first->depend) {
            (*std::get<0>(y)).checkDefined(locs_, sigs_, undef);
        }
    }
    std::sort(undef.begin(), undef.end(), [](Ground::UndefVec::value_type const &a, Ground::UndefVec::value_type const &b) { return a.first < b.first; });
    for (auto &x : undef) {
        GRINGO_REPORT(log, Warnings::AtomUndefined)
            << x.first << ": info: atom does not occur in any rule head:\n"
            << "  " << *x.second << "\n";
    }
    return prg;
}

std::ostream &operator<<(std::ostream &out, Program const &p) {
    p.print(out);
    return out;
}

// }}}

// }}}

} } // namespace Input Gringo
