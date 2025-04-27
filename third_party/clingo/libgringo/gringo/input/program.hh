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

#ifndef GRINGO_INPUT_PROGRAM_HH
#define GRINGO_INPUT_PROGRAM_HH

#include <gringo/terms.hh>
#include <gringo/input/literal.hh>
#include <gringo/input/statement.hh>
#include <gringo/ground/program.hh>

namespace Gringo { namespace Input {

// {{{ declaration of Program

using IdVec = Ground::IdVec;

struct Block {
    struct Hash {
        size_t operator()(Ground::SEdb const &edb) const {
            return hash_mix(edb->first->hash());
        }
    };

    struct Equal {
        bool operator()(Ground::SEdb const &a, Ground::SEdb const &b) const {
            return *a->first == *b->first;
        }
    };

    Block(Location const &loc, String name, IdVec &&params)
    : loc(loc)
    , name(name)
    , params(std::move(params)) { }

    Ground::SEdb make_sig() const;

    Location        loc;
    String          name;
    IdVec           params;
    SymVec          addedEdb;
    UStmVec         addedStms;
    UStmVec         stms;
};

using BlockMap = ordered_map<Ground::SEdb, Block, Block::Hash, Block::Equal>;

class Program {
public:
    Program();
    Program(Program const &other) = delete;
    Program(Program &&other) noexcept = default;
    Program &operator=(Program const &other) = delete;
    Program &operator=(Program &&other) noexcept = default;
    ~Program() = default;

    void begin(Location const &loc, String name, IdVec &&params);
    void add(UStm &&stm);
    void add(TheoryDef &&def, Logger &log);
    void rewrite(Defines &defs, Logger &log);
    void check(Logger &log);
    void print(std::ostream &out) const;
    void addInput(Sig sig);
    bool empty() const;
    Ground::Program toGround(std::set<Sig> const &sigs, DomainData &domains, Logger &log);

private:
    void rewriteDots();
    void rewriteArithmetics();
    void unpool();

    unsigned              auxNames_ = 0;
    Ground::LocSet        locs_;
    Ground::SigSet        sigs_;
    BlockMap              blocks_;
    Block                *current_ = nullptr;
    Projections           project_;
    UStmVec               stms_;
    TheoryDefs            theoryDefs_;
    UGTermVec             pheads;
    UGTermVec             nheads;
};

std::ostream &operator<<(std::ostream &out, Program const &p);

// }}}

} } // namespace Input Gringo

#endif // GRINGO_INPUT_PROGRAM_HH
