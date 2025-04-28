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

#ifndef GRINGO_GROUND_STATEMENT_HH
#define GRINGO_GROUND_STATEMENT_HH

#include <gringo/ground/literal.hh>
#include <gringo/ground/dependency.hh>

namespace Gringo { namespace Ground {

// {{{ declaration of Statement

class Statement;
using UStm = std::unique_ptr<Statement>;
using UStmVec = std::vector<UStm>;

class Statement : public Printable {
public:
    using Dep = Dependency<UStm, HeadOccurrence>;
    virtual bool isNormal() const = 0;
    virtual void analyze(Dep::Node &node, Dep &dep) = 0;
    virtual void startLinearize(bool active) = 0;
    virtual void linearize(Context &context, bool positive, Logger &log) = 0;
    virtual void enqueue(Queue &q) = 0;
};

// }}}

} } // namespace Ground Gringo

#endif // GRINGO_GROUND_STATEMENT_HH
