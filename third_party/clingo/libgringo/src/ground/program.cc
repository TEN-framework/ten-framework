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

#include "gringo/ground/program.hh"
#include "gringo/output/output.hh"

#define DEBUG_INSTANTIATION 0

namespace Gringo { namespace Ground {

// {{{ definition of Parameters

void Parameters::add(String name, SymVec &&args) {
    params_[Sig((std::string("#inc_") + name.c_str()).c_str(), static_cast<uint32_t>(args.size()), false)].emplace(std::move(args));
}

bool Parameters::find(Sig sig) const {
    auto it = params_.find(sig);
    return it != params_.end() && !it->second.empty();
}

ParamSet::const_iterator Parameters::begin() const {
    return params_.begin();
}

ParamSet::const_iterator Parameters::end() const {
    return params_.end();
}

bool Parameters::empty() const {
    return params_.empty();
}

void Parameters::clear() {
    params_.clear();
}

// }}}
// {{{ definition of Program

Program::Program(SEdbVec &&edb, Statement::Dep::ComponentVec &&stms)
: edb_(std::move(edb))
, stms_(std::move(stms)) { }

std::ostream &operator<<(std::ostream &out, Program const &prg) {
    bool comma = false;
    for (auto const &component : prg) {
        if (comma) {
            out << "\n";
        }
        else {
            comma = true;
        }
        out << "%" << (component.second ? " positive" : "") <<  " component";
        for (auto const &stm : component.first) {
            out << "\n" << *stm;
        }
    }
    return out;
}

void Program::linearize(Context &context, Logger &log) {
    for (auto &x : stms_) {
        for (auto &y : x.first) {
            y->startLinearize(true);
        }
        for (auto &y : x.first) {
            y->linearize(context, x.second, log);
        }
        for (auto &y : x.first) {
            y->startLinearize(false);
        }
    }
    linearized_ = true;
}

void Program::prepare(Parameters const &params, Output::OutputBase &out, Logger &log) {
    auto &doms = out.predDoms();
    for (auto &dom : doms) {
        auto name = dom->sig().name();
        if (name.startsWith("#p_")) {
            // The idea here is to assign a fresh uid to each projection atom.
            // Furthermore, the fresh atom is derived by the old atom.
            // This prevents redefinition errors from projections.
            for (auto &atom : *dom) {
                if (!atom.fact() && atom.hasUid() && atom.defined()) {
                    Output::Rule &rule = out.tempRule(false);;
                    Atom_t oldUid = atom.uid();
                    Atom_t newUid = out.data.newAtom();
                    rule.addHead(Output::LiteralId{NAF::POS, Output::AtomType::Aux, newUid, dom->domainOffset()});
                    rule.addBody(Output::LiteralId{NAF::POS, Output::AtomType::Aux, oldUid, dom->domainOffset()});
                    out.output(rule);
                    atom.resetUid(newUid);
                }
            }
        }
        else if (name.startsWith("#inc_")) {
            // clear incremental domains
            dom->clear();
        }
        dom->incNext();
    }
    out.checkOutPreds(log);
    for (auto &x : edb_) {
        if (params.find(std::get<0>(*x)->getSig())) {
            for (auto &z : std::get<1>(*x)) {
                auto it(doms.find(z.sig()));
                assert(it != doms.end());
                auto ret((*it)->define(z, true));
                if (!std::get<2>(ret)) {
                    Potassco::Id_t offset = static_cast<Id_t>(std::get<0>(ret) - (*it)->begin());
                    Potassco::Id_t domain = static_cast<Id_t>(it - doms.begin());
                    out.output(out.tempRule(false).addHead({NAF::POS, Output::AtomType::Predicate, offset, domain}));
                }
            }
        }
    }
    for (auto const &p : params) {
        auto base = doms.find(p.first);
        if (base != doms.end()) {
            for (auto const &args : p.second) {
                if (args.empty()) {
                    (*base)->define(Symbol::createId(p.first.name()), true);
                }
                else {
                    (*base)->define(Symbol::createFun(p.first.name(), Potassco::toSpan(args)), true);
                }
            }
        }
    }
    for (auto &x : doms) {
        x->nextGeneration();
    }
}

void Program::ground(Context &context, Output::OutputBase &out, Logger &log) {
    Queue q;
    for (auto &x : stms_) {
        if (!linearized_) {
            for (auto &y : x.first) {
                y->startLinearize(true);
            }
            for (auto &y : x.first) {
                y->linearize(context, x.second, log);
            }
            for (auto &y : x.first) {
                y->startLinearize(false);
            }
        }
#if DEBUG_INSTANTIATION > 0
        std::cerr << "============= component ===========" << std::endl;
#endif
        for (auto &y : x.first) {
#if DEBUG_INSTANTIATION > 0
            std::cerr << "  enqueue: " << *y << std::endl;
#endif
            y->enqueue(q);
        }
        q.process(out, log);
    }
    out.endGround(log);
    linearized_ = true;
}

// }}}

} } // namespace Ground Gringo
