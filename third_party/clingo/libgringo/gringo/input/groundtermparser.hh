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

#ifndef GRINGO_INPUT_GROUNDTERMPARSER_HH
#define GRINGO_INPUT_GROUNDTERMPARSER_HH

#include <gringo/lexerstate.hh>
#include <gringo/symbol.hh>
#include <gringo/term.hh>
#include <gringo/indexed.hh>

namespace Gringo { namespace Input {

// {{{ declaration of GroundTermParser

class GroundTermParser : private LexerState<int> {
    using IndexedTerms = Indexed<SymVec, unsigned>;
public:
    GroundTermParser() = default;
    GroundTermParser(GroundTermParser const &other) = delete;
    GroundTermParser(GroundTermParser &&other) noexcept = default;
    GroundTermParser &operator=(GroundTermParser const &other) = delete;
    GroundTermParser &operator=(GroundTermParser &&other) noexcept = default;
    ~GroundTermParser() noexcept = default;

    Symbol parse(std::string const &str, Logger &log);
    // NOTE: only to be used durning parsing (actually it would be better to hide this behind a private interface)
    Logger &logger() { assert(log_); return *log_; }
    void parseError(std::string const &message, Logger &log);
    void lexerError(StringSpan token, Logger &log);
    int lex(void *pValue, Logger &log);

    Symbol term(BinOp op, Symbol a, Symbol b);
    Symbol term(UnOp op, Symbol a);
    unsigned terms();
    unsigned terms(unsigned uid, Symbol a);
    SymVec terms(unsigned uid);
    Symbol tuple(unsigned uid, bool forceTuple);
    void setValue(Symbol value);

private:
    int lex_impl(void *pValue, Logger &log);

    Symbol       value_;
    IndexedTerms terms_;
    Logger *log_ = nullptr;
    bool         undefined_{false};
};

// }}}

} } // namespace Input Gringo

#endif // GRINGO_INPUT_GROUNDTERMPARSER_HH
