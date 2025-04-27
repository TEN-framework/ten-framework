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

#ifndef GRINGO_GROUND_PROGRAM_HH
#define GRINGO_GROUND_PROGRAM_HH

#include <gringo/ground/statement.hh>

namespace Gringo { namespace Ground {

// {{{ declaration of Program

using IdVec     = std::vector<std::pair<Location, String>>;
using SEdb      = std::shared_ptr<std::pair<UTerm, SymVec>>;
using SEdbVec   = std::vector<SEdb>;
using SymVecSet = std::set<SymVec>;
using ParamSet  = std::map<Sig, SymVecSet>;

class Parameters {
public:
    void add(String name, SymVec &&args);
    bool find(Sig sig) const;
    ParamSet::const_iterator begin() const;
    ParamSet::const_iterator end() const;
    bool empty() const;
    void clear();

private:
    ParamSet params_;
};

class Program {
public:
    using const_iterator = Statement::Dep::ComponentVec::const_iterator;

    Program(SEdbVec &&edb, Statement::Dep::ComponentVec &&stms);
    void linearize(Context &context, Logger &log);
    //! Prepare the ground program before grounding.
    void prepare(Parameters const &params, Output::OutputBase &out, Logger &log);
    //! Ground a prepared program.
    void ground(Context &context, Output::OutputBase &out, Logger &log);
    const_iterator begin() const {
        return stms_.begin();
    }
    const_iterator end() const {
        return stms_.end();
    }

private:
    SEdbVec                      edb_;
    Statement::Dep::ComponentVec stms_;
    bool                         linearized_ = false;
};

std::ostream &operator<<(std::ostream &out, Program const &prg);

// }}}

} } // namespace Ground Gringo

#endif // GRINGO_GROUND_PROGRAM_HH
