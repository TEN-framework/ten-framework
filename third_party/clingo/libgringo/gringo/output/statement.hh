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

#ifndef GRINGO_OUTPUT_STATEMENT_HH
#define GRINGO_OUTPUT_STATEMENT_HH

#include <gringo/output/literal.hh>
#include <gringo/backend.hh>
#include <gringo/locatable.hh>
#include <gringo/symbol.hh>
#include <gringo/domain.hh>

namespace Gringo { namespace Output {

class Statement;

// {{{1 declaration of AbstractOutput

class AbstractOutput {
public:
    AbstractOutput() = default;
    AbstractOutput(AbstractOutput const &other) = default;
    AbstractOutput(AbstractOutput &&other) noexcept = default;
    AbstractOutput &operator=(AbstractOutput const &other) = default;
    AbstractOutput &operator=(AbstractOutput &&other) noexcept = default;
    virtual ~AbstractOutput() noexcept = default;

    virtual void output(DomainData &data, Statement &stm) = 0;
};
using UAbstractOutput = std::unique_ptr<AbstractOutput>;

// {{{1 declaration of Statement

void replaceDelayed(DomainData &data, LiteralId &lit, LitVec &delayed);
void replaceDelayed(DomainData &data, LitVec &lits, LitVec &delayed);
void translate(DomainData &data, Translator &x, LiteralId &lit);
void translate(DomainData &data, Translator &x, LitVec &lits);

class Statement {
public:
    Statement() = default;
    Statement(Statement const &other) = default;
    Statement(Statement &&other) noexcept = default;
    Statement &operator=(Statement const &other) = default;
    Statement &operator=(Statement &&other) noexcept = default;
    virtual ~Statement() noexcept = default;

    virtual void output(DomainData &data, UBackend &out) const = 0;
    virtual void print(PrintPlain out, char const *prefix = "") const = 0;
    virtual void translate(DomainData &data, Translator &trans) = 0;
    virtual void replaceDelayed(DomainData &data, LitVec &delayed) = 0;
    // convenience function
    void passTo(DomainData &data, AbstractOutput &out) { out.output(data, *this); }
};

// }}}1

} } // namespace Output Gringo

#endif // GRINGO_OUTPUT_STATEMENT_HH

