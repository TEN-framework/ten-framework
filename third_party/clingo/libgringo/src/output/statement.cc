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

#include "gringo/output/statement.hh"
#include "gringo/output/literals.hh"

namespace Gringo { namespace Output {

void replaceDelayed(DomainData &data, LiteralId &lit, LitVec &delayed) {
    if (call(data, lit, &Literal::isIncomplete)) {
        auto ret = call(data, lit, &Literal::delayedLit);
        assert(ret.first.type() == AtomType::Aux);
        if (ret.second) {
            if (ret.first.sign() != NAF::POS) {
                assert(ret.first.sign() == lit.sign());
                delayed.emplace_back(lit.withSign(NAF::POS));
            }
            else {
                delayed.emplace_back(lit);
            }
        }
        lit = ret.first;
    }
}
void replaceDelayed(DomainData &data, LitVec &lits, LitVec &delayed) {
    for (auto &lit : lits) {
        replaceDelayed(data, lit, delayed);
    }
}

void translate(DomainData &data, Translator &x, LiteralId &lit) {
    lit = call(data, lit, &Literal::translate, x);
}

void translate(DomainData &data, Translator &x, LitVec &lits) {
    for (auto &lit : lits) {
        translate(data, x, lit);
    }
}

} } // namespace Output Gringo

