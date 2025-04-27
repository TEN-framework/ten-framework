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

#include "gringo/terms.hh"
#include "gringo/logger.hh"
#include <cstring>

namespace Gringo {

// {{{1 TheoryOpDef

TheoryOpDef::TheoryOpDef(Location const &loc, String op, unsigned priority, TheoryOperatorType type)
: loc_(loc)
, op_(op)
, priority_(priority)
, type_(type) { }

String TheoryOpDef::op() const {
    return op_;
}

TheoryOpDef::Key TheoryOpDef::key() const noexcept {
    return {op_, type_ == TheoryOperatorType::Unary};
}

Location const &TheoryOpDef::loc() const {
    return loc_;
}

void TheoryOpDef::print(std::ostream &out) const {
    out << op_ << " :" << priority_ << "," << type_;
}

unsigned TheoryOpDef::priority() const {
    return priority_;
}

TheoryOperatorType TheoryOpDef::type() const {
    return type_;
}

// {{{1 TheoryTermDef

TheoryTermDef::TheoryTermDef(Location const &loc, String name)
: loc_(loc)
, name_(name) { }

void TheoryTermDef::addOpDef(TheoryOpDef &&def, Logger &log) const {
    auto it = opDefs_.find(def.key());
    if (it == opDefs_.end()) {
        opDefs_.insert(std::move(def));
    }
    else {
        GRINGO_REPORT(log, Warnings::RuntimeError)
            << def.loc() << ": error: redefinition of theory operator:" << "\n"
            << "  " << def.op() << "\n"
            << it->loc() << ": note: operator first defined here\n";
    }
}

String const &TheoryTermDef::name() const noexcept {
    return name_;
}

Location const &TheoryTermDef::loc() const {
    return loc_;
}

void TheoryTermDef::print(std::ostream &out) const {
    out << name_ << "{";
    print_comma(out, opDefs_, ",");
    out << "}";
}

std::pair<unsigned, bool> TheoryTermDef::getPrioAndAssoc(String op) const {
    auto ret = opDefs_.find(TheoryOpDef::Key{op, false});
    if (ret != opDefs_.end()) {
        return {ret->priority(), ret->type() == TheoryOperatorType::BinaryLeft};
    }
    return {0, true};
}

bool TheoryTermDef::hasOp(String op, bool unary) const {
    return opDefs_.find(TheoryOpDef::Key{op, unary}) != opDefs_.end();
}

unsigned TheoryTermDef::getPrio(String op, bool unary) const {
    auto ret = opDefs_.find(TheoryOpDef::Key{op, unary});
    if (ret != opDefs_.end()) {
        return ret->priority();
    }
    return 0;
}

// {{{1 TheoryAtomDef

TheoryAtomDef::TheoryAtomDef(Location const &loc, String name, unsigned arity, String elemDef, TheoryAtomType type)
: TheoryAtomDef(loc, name, arity, elemDef, type, {}, "") { }

TheoryAtomDef::TheoryAtomDef(Location const &loc, String name, unsigned arity, String elemDef, TheoryAtomType type, StringVec &&ops, String guardDef)
: loc_(loc)
, sig_(name, arity, false)
, elemDef_(elemDef)
, guardDef_(guardDef)
, ops_(std::move(ops))
, type_(type) { }

Sig TheoryAtomDef::sig() const {
    return sig_;
}

bool TheoryAtomDef::hasGuard() const {
    return !ops_.empty();
}

TheoryAtomType TheoryAtomDef::type() const {
    return type_;
}

Location const &TheoryAtomDef::loc() const {
    return loc_;
}

StringVec const &TheoryAtomDef::ops() const {
    return ops_;
}

String TheoryAtomDef::elemDef() const {
    return elemDef_;
}

String TheoryAtomDef::guardDef() const {
    assert(hasGuard());
    return guardDef_;
}

void TheoryAtomDef::print(std::ostream &out) const {
    out << "&" << sig_.name() << "/" << sig_.arity() << ":" << elemDef_;
    if (hasGuard()) {
        out << ",{";
        print_comma(out, ops_, ",");
        out << "}," << guardDef_;
    }
    out << "," << type_;
}

// {{{1 TheoryDef

TheoryDef::TheoryDef(Location const &loc, String name)
: loc_(loc)
, name_(name) { }

String const &TheoryDef::name() const noexcept {
    return name_;
}

Location const &TheoryDef::loc() const {
    return loc_;
}

void TheoryDef::addAtomDef(TheoryAtomDef &&def, Logger &log) const {
    auto it = atomDefs_.find(def.sig());
    if (it == atomDefs_.end()) {
        atomDefs_.insert(std::move(def));
    }
    else {
        GRINGO_REPORT(log, Warnings::RuntimeError)
            << def.loc() << ": error: redefinition of theory atom:" << "\n"
            << "  " << def.sig() << "\n"
            << it->loc() << ": note: atom first defined here\n";
    }
}

void TheoryDef::addTermDef(TheoryTermDef &&def, Logger &log) const {
    auto it = termDefs_.find(def.name());
    if (it == termDefs_.end()) {
        termDefs_.insert(std::move(def));
    }
    else {
        GRINGO_REPORT(log, Warnings::RuntimeError)
            << def.loc() << ": error: redefinition of theory term:" << "\n"
            << "  " << def.name() << "\n"
            << it->loc() << ": note: term first defined term\n";
    }
}

void TheoryDef::print(std::ostream &out) const {
    out << "#theory " << name_ << "{";
    if (!atomDefs_.empty() || !termDefs_.empty()) {
        out << "\n";
    }
    bool sep = false;
    for (auto const &def : termDefs_) {
        if (sep) { out << ";\n"; }
        else     { sep = true; }
        out << "  " << def;
    }
    for (auto const &def : atomDefs_) {
        if (sep) { out << ";\n"; }
        else     { sep = true; }
        out << "  " << def;
    }
    if (sep) {
        out << "\n";
    }
    out << "}.";
}

TheoryAtomDef const *TheoryDef::getAtomDef(Sig sig) const {
    auto ret = atomDefs_.find(sig);
    return ret != atomDefs_.end() ? &*ret : nullptr;
}

TheoryTermDef const *TheoryDef::getTermDef(String name) const {
    auto ret = termDefs_.find(name);
    return ret != termDefs_.end() ? &*ret : nullptr;
}

TheoryAtomDefs const &TheoryDef::atomDefs() const {
    return atomDefs_;
}

// }}}1

} // namspace Gringo

