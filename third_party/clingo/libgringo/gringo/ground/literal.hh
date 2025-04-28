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

#ifndef GRINGO_GROUND_LITERAL_HH
#define GRINGO_GROUND_LITERAL_HH

#include <gringo/ground/types.hh>
#include <gringo/ground/dependency.hh>
#include <gringo/ground/instantiation.hh>
#include <gringo/output/types.hh>

namespace Gringo { namespace Ground {

using Output::DomainData;
using Output::PredicateDomain;

// {{{ declaration of HeadOccurrence

class HeadOccurrence {
public:
    HeadOccurrence() = default;
    HeadOccurrence(HeadOccurrence const &other) = default;
    HeadOccurrence(HeadOccurrence &&other) noexcept = default;
    HeadOccurrence &operator=(HeadOccurrence const &other) = default;
    HeadOccurrence &operator=(HeadOccurrence &&other) noexcept = default;
    virtual ~HeadOccurrence() noexcept = default;

    virtual void defines(IndexUpdater &update, Instantiator *inst) = 0;
};
using BodyOcc = BodyOccurrence<HeadOccurrence>;

// }}}

// {{{ declaration of Literal

class Literal;
using ULit = std::unique_ptr<Literal>;
using ULitVec = std::vector<ULit>;

class Literal : public Printable {
public:
    using Score   = double;
    virtual bool auxiliary() const = 0;
    virtual bool isRecursive() const = 0;
    virtual UIdx index(Context &context, BinderType type, Term::VarSet &bound) = 0;
    virtual BodyOcc *occurrence() = 0;
    virtual void collect(VarTermBoundVec &vars) const = 0;
    virtual void collectImportant(Term::VarSet &vars);
    virtual std::pair<Output::LiteralId,bool> toOutput(Logger &log) = 0;
    virtual Score score(Term::VarSet const &bound, Logger &log) = 0;
};

// }}}

} } // namespace Ground Gringo

#endif // GRINGO_GROUND_LITERAL_HH

