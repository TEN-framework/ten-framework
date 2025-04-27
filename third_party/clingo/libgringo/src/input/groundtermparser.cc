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

#include "gringo/input/groundtermparser.hh"
#include "input/groundtermgrammar/grammar.hh"
#include "gringo/logger.hh"

namespace Gringo { namespace Input {

Symbol GroundTermParser::parse(std::string const &str, Logger &log) {
    log_ = &log;
    undefined_ = false;
    while (!empty()) { pop(); }
    push(gringo_make_unique<std::stringstream>(str), 0);
    GroundTermGrammar::parser parser(this);
    parser.parse();
    return undefined_ ? Symbol() : value_;
}

Symbol GroundTermParser::term(BinOp op, Symbol a, Symbol b) {
    if (a.type() == SymbolType::Num && b.type() == SymbolType::Num && (op != BinOp::DIV || b.num() != 0)) {
        return Symbol::createNum(Gringo::eval(op, a.num(), b.num()));
    }
    undefined_ = true;
    return Symbol::createNum(0);
}

Symbol GroundTermParser::term(UnOp op, Symbol a) {
    if (a.type() == SymbolType::Num) {
        int num = a.num();
        switch (op) {
            case UnOp::NEG: { return Symbol::createNum(-num); }
            case UnOp::ABS: { return Symbol::createNum(std::abs(num)); }
            case UnOp::NOT: { return Symbol::createNum(~num); }
        }
        assert(false);
    }
    else if (op == UnOp::NEG && a.type() == SymbolType::Fun) {
        return a.flipSign();
    }
    undefined_ = true;
    return Symbol::createNum(0);
}

unsigned GroundTermParser::terms() {
    return terms_.emplace();
}

Symbol GroundTermParser::tuple(unsigned uid, bool forceTuple) {
    SymVec args(terms_.erase(uid));
    if (!forceTuple && args.size() == 1) {
        return args.front();
    }
    return Symbol::createTuple(Potassco::toSpan(args));
}

unsigned GroundTermParser::terms(unsigned uid, Symbol a) {
    terms_[uid].emplace_back(a);
    return uid;
}

SymVec GroundTermParser::terms(unsigned uid) {
    return terms_.erase(uid);
}

void GroundTermParser::parseError(std::string const &message, Logger &log) {
    static_cast<void>(log);
    Location loc("<string>", line(), column(), "<string>", line(), column());
    std::ostringstream oss;
    oss << loc << ": " << "error: " << message << "\n";
    throw GringoError(oss.str().c_str());
}

void GroundTermParser::lexerError(StringSpan token, Logger &log) {
    static_cast<void>(log);
    Location loc("<string>", line(), column(), "<string>", line(), column());
    std::ostringstream oss;
    oss << loc << ": " << "error: unexpected token:\n"
        << std::string(token.first, token.size) << "\n";
    throw GringoError(oss.str().c_str());
}

int GroundTermParser::lex(void *pValue, Logger &log) {
    return lex_impl(pValue, log);
}

void GroundTermParser::setValue(Symbol value) {
    value_ = value;
}

} }

#include "input/groundtermlexer.hh"
