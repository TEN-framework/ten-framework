// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.

// Take the name prefix into account.
#define yylex GringoGroundTermGrammar_lex

// First part of user prologue.
#line 39 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"

#include "gringo/input/groundtermparser.hh"
#include "gringo/term.hh"
#include <climits>

using namespace Gringo;
using namespace Gringo::Input;

int GringoGroundTermGrammar_lex(void *value, void *, GroundTermParser *lexer) {
    return lexer->lex(value, lexer->logger());
}

#line 58 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"

#include "grammar.hh"

// Unqualified %code blocks.
#line 54 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"

void GroundTermGrammar::parser::error(GroundTermGrammar::parser::location_type const &, std::string const &msg) {
    lexer->parseError(msg, lexer->logger());
}

#line 73 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"

#ifndef YY_
#if defined YYENABLE_NLS && YYENABLE_NLS
#if ENABLE_NLS
#include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#define YY_(msgid) dgettext("bison-runtime", msgid)
#endif
#endif
#ifndef YY_
#define YY_(msgid) msgid
#endif
#endif

// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
#if defined __GNUC__ && !defined __EXCEPTIONS
#define YY_EXCEPTIONS 0
#else
#define YY_EXCEPTIONS 1
#endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(Current, Rhs, N)                                                                                \
    do                                                                                                                 \
        if (N) {                                                                                                       \
            (Current).begin = YYRHSLOC(Rhs, 1).begin;                                                                  \
            (Current).end = YYRHSLOC(Rhs, N).end;                                                                      \
        } else {                                                                                                       \
            (Current).begin = (Current).end = YYRHSLOC(Rhs, 0).end;                                                    \
        }                                                                                                              \
    while (false)
#endif

// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
#define YYCDEBUG                                                                                                       \
    if (yydebug_)                                                                                                      \
    (*yycdebug_)

#define YY_SYMBOL_PRINT(Title, Symbol)                                                                                 \
    do {                                                                                                               \
        if (yydebug_) {                                                                                                \
            *yycdebug_ << Title << ' ';                                                                                \
            yy_print_(*yycdebug_, Symbol);                                                                             \
            *yycdebug_ << '\n';                                                                                        \
        }                                                                                                              \
    } while (false)

#define YY_REDUCE_PRINT(Rule)                                                                                          \
    do {                                                                                                               \
        if (yydebug_)                                                                                                  \
            yy_reduce_print_(Rule);                                                                                    \
    } while (false)

#define YY_STACK_PRINT()                                                                                               \
    do {                                                                                                               \
        if (yydebug_)                                                                                                  \
            yy_stack_print_();                                                                                         \
    } while (false)

#else // !YYDEBUG

#define YYCDEBUG                                                                                                       \
    if (false)                                                                                                         \
    std::cerr
#define YY_SYMBOL_PRINT(Title, Symbol) YY_USE(Symbol)
#define YY_REDUCE_PRINT(Rule) static_cast<void>(0)
#define YY_STACK_PRINT() static_cast<void>(0)

#endif // !YYDEBUG

#define yyerrok (yyerrstatus_ = 0)
#define yyclearin (yyla.clear())

#define YYACCEPT goto yyacceptlab
#define YYABORT goto yyabortlab
#define YYERROR goto yyerrorlab
#define YYRECOVERING() (!!yyerrstatus_)

#line 26 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
namespace Gringo {
namespace Input {
namespace GroundTermGrammar {
#line 166 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"

/// Build a parser object.
parser::parser(Gringo::Input::GroundTermParser *lexer_yyarg)
#if YYDEBUG
    : yydebug_(false), yycdebug_(&std::cerr),
#else
    :
#endif
      lexer(lexer_yyarg) {
}

parser::~parser() {}

parser::syntax_error::~syntax_error() YY_NOEXCEPT YY_NOTHROW {}

/*---------.
| symbol.  |
`---------*/

// basic_symbol.
template <typename Base>
parser::basic_symbol<Base>::basic_symbol(const basic_symbol &that)
    : Base(that), value(that.value), location(that.location) {}

/// Constructor for valueless symbols.
template <typename Base>
parser::basic_symbol<Base>::basic_symbol(typename Base::kind_type t, YY_MOVE_REF(location_type) l)
    : Base(t), value(), location(l) {}

template <typename Base>
parser::basic_symbol<Base>::basic_symbol(typename Base::kind_type t, YY_RVREF(value_type) v, YY_RVREF(location_type) l)
    : Base(t), value(YY_MOVE(v)), location(YY_MOVE(l)) {}

template <typename Base> parser::symbol_kind_type parser::basic_symbol<Base>::type_get() const YY_NOEXCEPT {
    return this->kind();
}

template <typename Base> bool parser::basic_symbol<Base>::empty() const YY_NOEXCEPT {
    return this->kind() == symbol_kind::S_YYEMPTY;
}

template <typename Base> void parser::basic_symbol<Base>::move(basic_symbol &s) {
    super_type::move(s);
    value = YY_MOVE(s.value);
    location = YY_MOVE(s.location);
}

// by_kind.
parser::by_kind::by_kind() YY_NOEXCEPT : kind_(symbol_kind::S_YYEMPTY) {}

#if 201103L <= YY_CPLUSPLUS
parser::by_kind::by_kind(by_kind &&that) YY_NOEXCEPT : kind_(that.kind_) { that.clear(); }
#endif

parser::by_kind::by_kind(const by_kind &that) YY_NOEXCEPT : kind_(that.kind_) {}

parser::by_kind::by_kind(token_kind_type t) YY_NOEXCEPT : kind_(yytranslate_(t)) {}

void parser::by_kind::clear() YY_NOEXCEPT { kind_ = symbol_kind::S_YYEMPTY; }

void parser::by_kind::move(by_kind &that) {
    kind_ = that.kind_;
    that.clear();
}

parser::symbol_kind_type parser::by_kind::kind() const YY_NOEXCEPT { return kind_; }

parser::symbol_kind_type parser::by_kind::type_get() const YY_NOEXCEPT { return this->kind(); }

// by_state.
parser::by_state::by_state() YY_NOEXCEPT : state(empty_state) {}

parser::by_state::by_state(const by_state &that) YY_NOEXCEPT : state(that.state) {}

void parser::by_state::clear() YY_NOEXCEPT { state = empty_state; }

void parser::by_state::move(by_state &that) {
    state = that.state;
    that.clear();
}

parser::by_state::by_state(state_type s) YY_NOEXCEPT : state(s) {}

parser::symbol_kind_type parser::by_state::kind() const YY_NOEXCEPT {
    if (state == empty_state)
        return symbol_kind::S_YYEMPTY;
    else
        return YY_CAST(symbol_kind_type, yystos_[+state]);
}

parser::stack_symbol_type::stack_symbol_type() {}

parser::stack_symbol_type::stack_symbol_type(YY_RVREF(stack_symbol_type) that)
    : super_type(YY_MOVE(that.state), YY_MOVE(that.value), YY_MOVE(that.location)) {
#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
}

parser::stack_symbol_type::stack_symbol_type(state_type s, YY_MOVE_REF(symbol_type) that)
    : super_type(s, YY_MOVE(that.value), YY_MOVE(that.location)) {
    // that is emptied.
    that.kind_ = symbol_kind::S_YYEMPTY;
}

#if YY_CPLUSPLUS < 201103L
parser::stack_symbol_type &parser::stack_symbol_type::operator=(const stack_symbol_type &that) {
    state = that.state;
    value = that.value;
    location = that.location;
    return *this;
}

parser::stack_symbol_type &parser::stack_symbol_type::operator=(stack_symbol_type &that) {
    state = that.state;
    value = that.value;
    location = that.location;
    // that is emptied.
    that.state = empty_state;
    return *this;
}
#endif

template <typename Base> void parser::yy_destroy_(const char *yymsg, basic_symbol<Base> &yysym) const {
    if (yymsg)
        YY_SYMBOL_PRINT(yymsg, yysym);

    // User destructor.
    YY_USE(yysym.kind());
}

#if YYDEBUG
template <typename Base> void parser::yy_print_(std::ostream &yyo, const basic_symbol<Base> &yysym) const {
    std::ostream &yyoutput = yyo;
    YY_USE(yyoutput);
    if (yysym.empty())
        yyo << "empty symbol";
    else {
        symbol_kind_type yykind = yysym.kind();
        yyo << (yykind < YYNTOKENS ? "token" : "nterm") << ' ' << yysym.name() << " (" << yysym.location << ": ";
        YY_USE(yykind);
        yyo << ')';
    }
}
#endif

void parser::yypush_(const char *m, YY_MOVE_REF(stack_symbol_type) sym) {
    if (m)
        YY_SYMBOL_PRINT(m, sym);
    yystack_.push(YY_MOVE(sym));
}

void parser::yypush_(const char *m, state_type s, YY_MOVE_REF(symbol_type) sym) {
#if 201103L <= YY_CPLUSPLUS
    yypush_(m, stack_symbol_type(s, std::move(sym)));
#else
    stack_symbol_type ss(s, sym);
    yypush_(m, ss);
#endif
}

void parser::yypop_(int n) YY_NOEXCEPT { yystack_.pop(n); }

#if YYDEBUG
std::ostream &parser::debug_stream() const { return *yycdebug_; }

void parser::set_debug_stream(std::ostream &o) { yycdebug_ = &o; }

parser::debug_level_type parser::debug_level() const { return yydebug_; }

void parser::set_debug_level(debug_level_type l) { yydebug_ = l; }
#endif // YYDEBUG

parser::state_type parser::yy_lr_goto_state_(state_type yystate, int yysym) {
    int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
        return yytable_[yyr];
    else
        return yydefgoto_[yysym - YYNTOKENS];
}

bool parser::yy_pact_value_is_default_(int yyvalue) YY_NOEXCEPT { return yyvalue == yypact_ninf_; }

bool parser::yy_table_value_is_error_(int yyvalue) YY_NOEXCEPT { return yyvalue == yytable_ninf_; }

int parser::operator()() { return parse(); }

int parser::parse() {
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The locations where the error started and ended.
    stack_symbol_type yyerror_range[3];

    /// The return value of parse ().
    int yyresult;

#if YY_EXCEPTIONS
    try
#endif // YY_EXCEPTIONS
    {
        YYCDEBUG << "Starting parse\n";

        /* Initialize the stack.  The initial state will be set in
           yynewstate, since the latter expects the semantical and the
           location values to have been already stored, initialize these
           stacks with a primary value.  */
        yystack_.clear();
        yypush_(YY_NULLPTR, 0, YY_MOVE(yyla));

    /*-----------------------------------------------.
    | yynewstate -- push a new symbol on the stack.  |
    `-----------------------------------------------*/
    yynewstate:
        YYCDEBUG << "Entering state " << int(yystack_[0].state) << '\n';
        YY_STACK_PRINT();

        // Accept?
        if (yystack_[0].state == yyfinal_)
            YYACCEPT;

        goto yybackup;

    /*-----------.
    | yybackup.  |
    `-----------*/
    yybackup:
        // Try to take a decision without lookahead.
        yyn = yypact_[+yystack_[0].state];
        if (yy_pact_value_is_default_(yyn))
            goto yydefault;

        // Read a lookahead token.
        if (yyla.empty()) {
            YYCDEBUG << "Reading a token\n";
#if YY_EXCEPTIONS
            try
#endif // YY_EXCEPTIONS
            {
                yyla.kind_ = yytranslate_(yylex(&yyla.value, &yyla.location, lexer));
            }
#if YY_EXCEPTIONS
            catch (const syntax_error &yyexc) {
                YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
                error(yyexc);
                goto yyerrlab1;
            }
#endif // YY_EXCEPTIONS
        }
        YY_SYMBOL_PRINT("Next token is", yyla);

        if (yyla.kind() == symbol_kind::S_YYerror) {
            // The scanner already issued an error message, process directly
            // to error recovery.  But do not keep the error token as
            // lookahead, it is too special and may lead us to an endless
            // loop in error recovery. */
            yyla.kind_ = symbol_kind::S_YYUNDEF;
            goto yyerrlab1;
        }

        /* If the proper action on seeing token YYLA.TYPE is to reduce or
           to detect an error, take that action.  */
        yyn += yyla.kind();
        if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.kind()) {
            goto yydefault;
        }

        // Reduce or error.
        yyn = yytable_[yyn];
        if (yyn <= 0) {
            if (yy_table_value_is_error_(yyn))
                goto yyerrlab;
            yyn = -yyn;
            goto yyreduce;
        }

        // Count tokens shifted since error; after three, turn off error status.
        if (yyerrstatus_)
            --yyerrstatus_;

        // Shift the lookahead token.
        yypush_("Shifting", state_type(yyn), YY_MOVE(yyla));
        goto yynewstate;

    /*-----------------------------------------------------------.
    | yydefault -- do the default action for the current state.  |
    `-----------------------------------------------------------*/
    yydefault:
        yyn = yydefact_[+yystack_[0].state];
        if (yyn == 0)
            goto yyerrlab;
        goto yyreduce;

    /*-----------------------------.
    | yyreduce -- do a reduction.  |
    `-----------------------------*/
    yyreduce:
        yylen = yyr2_[yyn];
        {
            stack_symbol_type yylhs;
            yylhs.state = yy_lr_goto_state_(yystack_[yylen].state, yyr1_[yyn]);
            /* If YYLEN is nonzero, implement the default value of the
               action: '$$ = $1'.  Otherwise, use the top of the stack.

               Otherwise, the following line sets YYLHS.VALUE to garbage.
               This behavior is undocumented and Bison users should not rely
               upon it.  */
            if (yylen)
                yylhs.value = yystack_[yylen - 1].value;
            else
                yylhs.value = yystack_[0].value;

            // Default location.
            {
                stack_type::slice range(yystack_, yylen);
                YYLLOC_DEFAULT(yylhs.location, range, yylen);
                yyerror_range[1].location = yylhs.location;
            }

            // Perform the reduction.
            YY_REDUCE_PRINT(yyn);
#if YY_EXCEPTIONS
            try
#endif // YY_EXCEPTIONS
            {
                switch (yyn) {
                    case 2: // start: term
#line 117 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        lexer->setValue(Symbol((yystack_[0].value.value)));
                    }
#line 639 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 3: // term: term "^" term
#line 120 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer
                                ->term(BinOp::XOR, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 645 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 4: // term: term "?" term
#line 121 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer->term(BinOp::OR, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 651 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 5: // term: term "&" term
#line 122 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer
                                ->term(BinOp::AND, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 657 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 6: // term: term "+" term
#line 123 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer
                                ->term(BinOp::ADD, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 663 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 7: // term: term "-" term
#line 124 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer
                                ->term(BinOp::SUB, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 669 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 8: // term: term "*" term
#line 125 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer
                                ->term(BinOp::MUL, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 675 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 9: // term: term "/" term
#line 126 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer
                                ->term(BinOp::DIV, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 681 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 10: // term: term "\\" term
#line 127 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer
                                ->term(BinOp::MOD, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 687 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 11: // term: term "**" term
#line 128 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) =
                            lexer
                                ->term(BinOp::POW, Symbol((yystack_[2].value.value)), Symbol((yystack_[0].value.value)))
                                .rep();
                    }
#line 693 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 12: // term: "-" term
#line 129 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = lexer->term(UnOp::NEG, Symbol((yystack_[0].value.value))).rep();
                    }
#line 699 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 13: // term: "~" term
#line 130 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = lexer->term(UnOp::NOT, Symbol((yystack_[0].value.value))).rep();
                    }
#line 705 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 14: // term: "(" ")"
#line 131 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = Symbol::createTuple(Potassco::toSpan<Symbol>()).rep();
                    }
#line 711 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 15: // term: "(" "," ")"
#line 132 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = Symbol::createTuple(Potassco::toSpan<Symbol>()).rep();
                    }
#line 717 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 16: // term: "(" nterms ")"
#line 133 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = lexer->tuple((yystack_[1].value.uid), false).rep();
                    }
#line 723 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 17: // term: "(" nterms "," ")"
#line 134 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = lexer->tuple((yystack_[2].value.uid), true).rep();
                    }
#line 729 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 18: // term: "<IDENTIFIER>" "(" terms ")"
#line 135 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = Symbol::createFun((yystack_[3].value.str),
                                                                Potassco::toSpan(lexer->terms((yystack_[1].value.uid))))
                                                  .rep();
                    }
#line 735 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 19: // term: "|" term "|"
#line 136 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = lexer->term(UnOp::ABS, Symbol((yystack_[1].value.value))).rep();
                    }
#line 741 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 20: // term: "<IDENTIFIER>"
#line 137 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = Symbol::createId((yystack_[0].value.str)).rep();
                    }
#line 747 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 21: // term: "<NUMBER>"
#line 138 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = Symbol::createNum((yystack_[0].value.num)).rep();
                    }
#line 753 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 22: // term: "<STRING>"
#line 139 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = Symbol::createStr((yystack_[0].value.str)).rep();
                    }
#line 759 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 23: // term: "#inf"
#line 140 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = Symbol::createInf().rep();
                    }
#line 765 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 24: // term: "#sup"
#line 141 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.value) = Symbol::createSup().rep();
                    }
#line 771 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 25: // nterms: term
#line 145 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->terms(lexer->terms(), Symbol((yystack_[0].value.value)));
                    }
#line 777 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 26: // nterms: nterms "," term
#line 146 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->terms((yystack_[2].value.uid), Symbol((yystack_[0].value.value)));
                    }
#line 783 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 27: // terms: nterms
#line 150 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.uid) = (yystack_[0].value.uid);
                    }
#line 789 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

                    case 28: // terms: %empty
#line 151 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->terms();
                    }
#line 795 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
                    break;

#line 799 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"

                    default:
                        break;
                }
            }
#if YY_EXCEPTIONS
            catch (const syntax_error &yyexc) {
                YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
                error(yyexc);
                YYERROR;
            }
#endif // YY_EXCEPTIONS
            YY_SYMBOL_PRINT("-> $$ =", yylhs);
            yypop_(yylen);
            yylen = 0;

            // Shift the result of the reduction.
            yypush_(YY_NULLPTR, YY_MOVE(yylhs));
        }
        goto yynewstate;

    /*--------------------------------------.
    | yyerrlab -- here on detecting error.  |
    `--------------------------------------*/
    yyerrlab:
        // If not already recovering from an error, report this error.
        if (!yyerrstatus_) {
            ++yynerrs_;
            context yyctx(*this, yyla);
            std::string msg = yysyntax_error_(yyctx);
            error(yyla.location, YY_MOVE(msg));
        }

        yyerror_range[1].location = yyla.location;
        if (yyerrstatus_ == 3) {
            /* If just tried and failed to reuse lookahead token after an
               error, discard it.  */

            // Return failure if at end of input.
            if (yyla.kind() == symbol_kind::S_YYEOF)
                YYABORT;
            else if (!yyla.empty()) {
                yy_destroy_("Error: discarding", yyla);
                yyla.clear();
            }
        }

        // Else will try to reuse lookahead token after shifting the error token.
        goto yyerrlab1;

    /*---------------------------------------------------.
    | yyerrorlab -- error raised explicitly by YYERROR.  |
    `---------------------------------------------------*/
    yyerrorlab:
        /* Pacify compilers when the user code never invokes YYERROR and
           the label yyerrorlab therefore never appears in user code.  */
        if (false)
            YYERROR;

        /* Do not reclaim the symbols of the rule whose action triggered
           this YYERROR.  */
        yypop_(yylen);
        yylen = 0;
        YY_STACK_PRINT();
        goto yyerrlab1;

    /*-------------------------------------------------------------.
    | yyerrlab1 -- common code for both syntax error and YYERROR.  |
    `-------------------------------------------------------------*/
    yyerrlab1:
        yyerrstatus_ = 3; // Each real token shifted decrements this.
        // Pop stack until we find a state that shifts the error token.
        for (;;) {
            yyn = yypact_[+yystack_[0].state];
            if (!yy_pact_value_is_default_(yyn)) {
                yyn += symbol_kind::S_YYerror;
                if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == symbol_kind::S_YYerror) {
                    yyn = yytable_[yyn];
                    if (0 < yyn)
                        break;
                }
            }

            // Pop the current state because it cannot handle the error token.
            if (yystack_.size() == 1)
                YYABORT;

            yyerror_range[1].location = yystack_[0].location;
            yy_destroy_("Error: popping", yystack_[0]);
            yypop_();
            YY_STACK_PRINT();
        }
        {
            stack_symbol_type error_token;

            yyerror_range[2].location = yyla.location;
            YYLLOC_DEFAULT(error_token.location, yyerror_range, 2);

            // Shift the error token.
            error_token.state = state_type(yyn);
            yypush_("Shifting", YY_MOVE(error_token));
        }
        goto yynewstate;

    /*-------------------------------------.
    | yyacceptlab -- YYACCEPT comes here.  |
    `-------------------------------------*/
    yyacceptlab:
        yyresult = 0;
        goto yyreturn;

    /*-----------------------------------.
    | yyabortlab -- YYABORT comes here.  |
    `-----------------------------------*/
    yyabortlab:
        yyresult = 1;
        goto yyreturn;

    /*-----------------------------------------------------.
    | yyreturn -- parsing is finished, return the result.  |
    `-----------------------------------------------------*/
    yyreturn:
        if (!yyla.empty())
            yy_destroy_("Cleanup: discarding lookahead", yyla);

        /* Do not reclaim the symbols of the rule whose action triggered
           this YYABORT or YYACCEPT.  */
        yypop_(yylen);
        YY_STACK_PRINT();
        while (1 < yystack_.size()) {
            yy_destroy_("Cleanup: popping", yystack_[0]);
            yypop_();
        }

        return yyresult;
    }
#if YY_EXCEPTIONS
    catch (...) {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
        // Do not try to display the values of the reclaimed symbols,
        // as their printers might throw an exception.
        if (!yyla.empty())
            yy_destroy_(YY_NULLPTR, yyla);

        while (1 < yystack_.size()) {
            yy_destroy_(YY_NULLPTR, yystack_[0]);
            yypop_();
        }
        throw;
    }
#endif // YY_EXCEPTIONS
}

void parser::error(const syntax_error &yyexc) { error(yyexc.location, yyexc.what()); }

/* Return YYSTR after stripping away unnecessary quotes and
   backslashes, so that it's suitable for yyerror.  The heuristic is
   that double-quoting is unnecessary unless the string contains an
   apostrophe, a comma, or backslash (other than backslash-backslash).
   YYSTR is taken from yytname.  */
std::string parser::yytnamerr_(const char *yystr) {
    if (*yystr == '"') {
        std::string yyr;
        char const *yyp = yystr;

        for (;;)
            switch (*++yyp) {
                case '\'':
                case ',':
                    goto do_not_strip_quotes;

                case '\\':
                    if (*++yyp != '\\')
                        goto do_not_strip_quotes;
                    else
                        goto append;

                append:
                default:
                    yyr += *yyp;
                    break;

                case '"':
                    return yyr;
            }
    do_not_strip_quotes:;
    }

    return yystr;
}

std::string parser::symbol_name(symbol_kind_type yysymbol) { return yytnamerr_(yytname_[yysymbol]); }

// parser::context.
parser::context::context(const parser &yyparser, const symbol_type &yyla) : yyparser_(yyparser), yyla_(yyla) {}

int parser::context::expected_tokens(symbol_kind_type yyarg[], int yyargn) const {
    // Actual number of expected tokens
    int yycount = 0;

    const int yyn = yypact_[+yyparser_.yystack_[0].state];
    if (!yy_pact_value_is_default_(yyn)) {
        /* Start YYX at -YYN if negative to avoid negative indexes in
           YYCHECK.  In other words, skip the first -YYN actions for
           this state because they are default actions.  */
        const int yyxbegin = yyn < 0 ? -yyn : 0;
        // Stay within bounds of both yycheck and yytname.
        const int yychecklim = yylast_ - yyn + 1;
        const int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
        for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck_[yyx + yyn] == yyx && yyx != symbol_kind::S_YYerror &&
                !yy_table_value_is_error_(yytable_[yyx + yyn])) {
                if (!yyarg)
                    ++yycount;
                else if (yycount == yyargn)
                    return 0;
                else
                    yyarg[yycount++] = YY_CAST(symbol_kind_type, yyx);
            }
    }

    if (yyarg && yycount == 0 && 0 < yyargn)
        yyarg[0] = symbol_kind::S_YYEMPTY;
    return yycount;
}

int parser::yy_syntax_error_arguments_(const context &yyctx, symbol_kind_type yyarg[], int yyargn) const {
    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state merging
         (from LALR or IELR) and default reductions corrupt the expected
         token list.  However, the list is correct for canonical LR with
         one exception: it will still contain any token that will not be
         accepted due to an error action in a later state.
    */

    if (!yyctx.lookahead().empty()) {
        if (yyarg)
            yyarg[0] = yyctx.token();
        int yyn = yyctx.expected_tokens(yyarg ? yyarg + 1 : yyarg, yyargn - 1);
        return yyn + 1;
    }
    return 0;
}

// Generate an error message.
std::string parser::yysyntax_error_(const context &yyctx) const {
    // Its maximum.
    enum { YYARGS_MAX = 5 };
    // Arguments of yyformat.
    symbol_kind_type yyarg[YYARGS_MAX];
    int yycount = yy_syntax_error_arguments_(yyctx, yyarg, YYARGS_MAX);

    char const *yyformat = YY_NULLPTR;
    switch (yycount) {
#define YYCASE_(N, S)                                                                                                  \
    case N:                                                                                                            \
        yyformat = S;                                                                                                  \
        break
        default: // Avoid compiler warnings.
            YYCASE_(0, YY_("syntax error"));
            YYCASE_(1, YY_("syntax error, unexpected %s"));
            YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
            YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
            YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
            YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

    std::string yyres;
    // Argument number.
    std::ptrdiff_t yyi = 0;
    for (char const *yyp = yyformat; *yyp; ++yyp)
        if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount) {
            yyres += symbol_name(yyarg[yyi++]);
            ++yyp;
        } else
            yyres += *yyp;
    return yyres;
}

const signed char parser::yypact_ninf_ = -6;

const signed char parser::yytable_ninf_ = -1;

const signed char parser::yypact_[] = {57, 57, 32,  57,  57, -6, -6, -6,  7,  -6,  9,  76, -6, -5, -6, 76, -4,
                                       -6, 2,  57,  -6,  57, 57, 57, 57,  57, 57,  57, 57, 57, -6, 49, -6, -6,
                                       13, 8,  116, 109, 20, 20, 20, 101, 20, 116, 89, -6, 76, 57, -6};

const signed char parser::yydefact_[] = {0,  0, 0,  0, 0,  24, 23, 21, 20, 22, 0, 2,  13, 0,  14, 25, 0,
                                         12, 0, 28, 1, 0,  0,  0,  0,  0,  0,  0, 0,  0,  15, 0,  16, 19,
                                         27, 0, 6,  5, 10, 8,  11, 4,  9,  7,  3, 17, 26, 0,  18};

const signed char parser::yypgoto_[] = {-6, -6, 0, 14, -6};

const signed char parser::yydefgoto_[] = {0, 10, 15, 16, 35};

const signed char parser::yytable_[] = {
    11, 12, 31, 17, 18, 21, 22, 30, 32, 20, 23, 24, 25, 26, 19, 27, 28, 29, 33, 47, 48, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 25, 46, 0,  34, 0,  0,  0,  1,  13, 2,  0,  0,  0,  0,  14, 0,  3,  46, 4,  5,  6,  7,
    8,  9,  1,  0,  2,  0,  0,  0,  0,  45, 1,  3,  2,  4,  5,  6,  7,  8,  9,  3,  0,  4,  5,  6,  7,  8,
    9,  21, 22, 0,  0,  0,  23, 24, 25, 26, 0,  27, 28, 29, 21, 22, 0,  0,  0,  23, 24, 25, 26, 0,  27, 28,
    21, 22, 0,  0,  0,  23, 24, 25, 21, 0,  27, 28, 0,  23, 24, 25, 0,  0,  27, 28, 23, 24, 25, 0,  0,  27};

const signed char parser::yycheck_[] = {
    0,  1,  6,  3,  4,  3,  4,  12, 12, 0,  8,  9,  10, 11, 7,  13, 14, 15, 16, 6,  12, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 10, 31, -1, 19, -1, -1, -1, 5,  6,  7,  -1, -1, -1, -1, 12, -1, 14, 47, 16, 17, 18, 19,
    20, 21, 5,  -1, 7,  -1, -1, -1, -1, 12, 5,  14, 7,  16, 17, 18, 19, 20, 21, 14, -1, 16, 17, 18, 19, 20,
    21, 3,  4,  -1, -1, -1, 8,  9,  10, 11, -1, 13, 14, 15, 3,  4,  -1, -1, -1, 8,  9,  10, 11, -1, 13, 14,
    3,  4,  -1, -1, -1, 8,  9,  10, 3,  -1, 13, 14, -1, 8,  9,  10, -1, -1, 13, 14, 8,  9,  10, -1, -1, 13};

const signed char parser::yystos_[] = {0,  5,  7,  14, 16, 17, 18, 19, 20, 21, 25, 26, 26, 6,  12, 26, 27,
                                       26, 26, 7,  0,  3,  4,  8,  9,  10, 11, 13, 14, 15, 12, 6,  12, 16,
                                       27, 28, 26, 26, 26, 26, 26, 26, 26, 26, 26, 12, 26, 6,  12};

const signed char parser::yyr1_[] = {0,  24, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
                                     26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 28, 28};

const signed char parser::yyr2_[] = {0, 2, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2,
                                     3, 3, 4, 4, 3, 1, 1, 1, 1, 1, 1, 3, 1, 0};

#if YYDEBUG || 1
// YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
// First, the terminals, then, starting at \a YYNTOKENS, nonterminals.
const char *const parser::yytname_[] = {"\"<EOF>\"",    "error",        "\"invalid token\"",
                                        "\"+\"",        "\"&\"",        "\"~\"",
                                        "\",\"",        "\"(\"",        "\"\\\\\"",
                                        "\"*\"",        "\"**\"",       "\"?\"",
                                        "\")\"",        "\"/\"",        "\"-\"",
                                        "\"^\"",        "\"|\"",        "\"#sup\"",
                                        "\"#inf\"",     "\"<NUMBER>\"", "\"<IDENTIFIER>\"",
                                        "\"<STRING>\"", "UMINUS",       "UBNOT",
                                        "$accept",      "start",        "term",
                                        "nterms",       "terms",        YY_NULLPTR};
#endif

#if YYDEBUG
const unsigned char parser::yyrline_[] = {0,   117, 117, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131,
                                          132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 145, 146, 150, 151};

void parser::yy_stack_print_() const {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator i = yystack_.begin(), i_end = yystack_.end(); i != i_end; ++i)
        *yycdebug_ << ' ' << int(i->state);
    *yycdebug_ << '\n';
}

void parser::yy_reduce_print_(int yyrule) const {
    int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1 << " (line " << yylno << "):\n";
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
        YY_SYMBOL_PRINT("   $" << yyi + 1 << " =", yystack_[(yynrhs) - (yyi + 1)]);
}
#endif // YYDEBUG

parser::symbol_kind_type parser::yytranslate_(int t) YY_NOEXCEPT {
    // YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to
    // TOKEN-NUM as returned by yylex.
    static const signed char translate_table[] = {
        0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
    // Last valid token kind.
    const int code_max = 278;

    if (t <= 0)
        return symbol_kind::S_YYEOF;
    else if (t <= code_max)
        return static_cast<symbol_kind_type>(translate_table[t]);
    else
        return symbol_kind::S_YYUNDEF;
}

#line 26 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/groundtermgrammar.yy"
} // namespace GroundTermGrammar
} // namespace Input
} // namespace Gringo
#line 1352 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/groundtermgrammar/grammar.cc"
