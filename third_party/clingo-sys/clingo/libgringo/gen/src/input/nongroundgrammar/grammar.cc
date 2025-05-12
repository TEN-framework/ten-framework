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
#define yylex GringoNonGroundGrammar_lex

// First part of user prologue.
#line 58 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"

#include "gringo/input/nongroundparser.hh"
#include "gringo/input/programbuilder.hh"
#include <climits>

#define BUILDER (lexer->builder())
#define LOGGER (lexer->logger())
#define YYLLOC_DEFAULT(Current, Rhs, N)                                                                                \
    do {                                                                                                               \
        if (N) {                                                                                                       \
            (Current).beginFilename = YYRHSLOC(Rhs, 1).beginFilename;                                                  \
            (Current).beginLine = YYRHSLOC(Rhs, 1).beginLine;                                                          \
            (Current).beginColumn = YYRHSLOC(Rhs, 1).beginColumn;                                                      \
            (Current).endFilename = YYRHSLOC(Rhs, N).endFilename;                                                      \
            (Current).endLine = YYRHSLOC(Rhs, N).endLine;                                                              \
            (Current).endColumn = YYRHSLOC(Rhs, N).endColumn;                                                          \
        } else {                                                                                                       \
            (Current).beginFilename = YYRHSLOC(Rhs, 0).endFilename;                                                    \
            (Current).beginLine = YYRHSLOC(Rhs, 0).endLine;                                                            \
            (Current).beginColumn = YYRHSLOC(Rhs, 0).endColumn;                                                        \
            (Current).endFilename = YYRHSLOC(Rhs, 0).endFilename;                                                      \
            (Current).endLine = YYRHSLOC(Rhs, 0).endLine;                                                              \
            (Current).endColumn = YYRHSLOC(Rhs, 0).endColumn;                                                          \
        }                                                                                                              \
    } while (false)

using namespace Gringo;
using namespace Gringo::Input;

int GringoNonGroundGrammar_lex(void *value, Gringo::Location *loc, NonGroundParser *lexer) {
    return lexer->lex(value, *loc);
}

#line 81 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"

#include "grammar.hh"

// Unqualified %code blocks.
#line 96 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"

void NonGroundGrammar::parser::error(DefaultLocation const &l, std::string const &msg) { lexer->parseError(l, msg); }

#line 96 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"

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

#line 28 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
namespace Gringo {
namespace Input {
namespace NonGroundGrammar {
#line 189 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"

/// Build a parser object.
parser::parser(Gringo::Input::NonGroundParser *lexer_yyarg)
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
                    case 7: // statement: "."
#line 327 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->parseError(yylhs.location, "syntax error, unexpected .");
                    }
#line 662 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 10: // identifier: "<IDENTIFIER>"
#line 333 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = (yystack_[0].value.str);
                    }
#line 668 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 11: // identifier: "default"
#line 334 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = (yystack_[0].value.str);
                    }
#line 674 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 12: // identifier: "override"
#line 335 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = (yystack_[0].value.str);
                    }
#line 680 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 13: // constterm: constterm "^" constterm
#line 342 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::XOR, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 686 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 14: // constterm: constterm "?" constterm
#line 343 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, BinOp::OR, (yystack_[2].value.term), (yystack_[0].value.term));
                    }
#line 692 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 15: // constterm: constterm "&" constterm
#line 344 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::AND, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 698 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 16: // constterm: constterm "+" constterm
#line 345 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::ADD, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 704 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 17: // constterm: constterm "-" constterm
#line 346 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::SUB, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 710 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 18: // constterm: constterm "*" constterm
#line 347 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::MUL, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 716 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 19: // constterm: constterm "/" constterm
#line 348 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::DIV, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 722 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 20: // constterm: constterm "\\" constterm
#line 349 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::MOD, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 728 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 21: // constterm: constterm "**" constterm
#line 350 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::POW, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 734 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 22: // constterm: "-" constterm
#line 351 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, UnOp::NEG, (yystack_[0].value.term));
                    }
#line 740 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 23: // constterm: "~" constterm
#line 352 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, UnOp::NOT, (yystack_[0].value.term));
                    }
#line 746 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 24: // constterm: "(" ")"
#line 353 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BUILDER.termvec(), false);
                    }
#line 752 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 25: // constterm: "(" "," ")"
#line 354 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BUILDER.termvec(), true);
                    }
#line 758 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 26: // constterm: "(" consttermvec ")"
#line 355 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, (yystack_[1].value.termvec), false);
                    }
#line 764 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 27: // constterm: "(" consttermvec "," ")"
#line 356 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, (yystack_[2].value.termvec), true);
                    }
#line 770 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 28: // constterm: identifier "(" constargvec ")"
#line 357 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, String::fromRep((yystack_[3].value.str)),
                                                          (yystack_[1].value.termvecvec), false);
                    }
#line 776 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 29: // constterm: "@" identifier "(" constargvec ")"
#line 358 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, String::fromRep((yystack_[3].value.str)),
                                                          (yystack_[1].value.termvecvec), true);
                    }
#line 782 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 30: // constterm: "|" constterm "|"
#line 359 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, UnOp::ABS, (yystack_[1].value.term));
                    }
#line 788 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 31: // constterm: identifier
#line 360 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, Symbol::createId(String::fromRep((yystack_[0].value.str))));
                    }
#line 794 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 32: // constterm: "@" identifier
#line 361 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, String::fromRep((yystack_[0].value.str)),
                                         BUILDER.termvecvec(BUILDER.termvecvec(), BUILDER.termvec()), true);
                    }
#line 800 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 33: // constterm: "<NUMBER>"
#line 362 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, Symbol::createNum((yystack_[0].value.num)));
                    }
#line 806 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 34: // constterm: "<STRING>"
#line 363 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, Symbol::createStr(String::fromRep((yystack_[0].value.str))));
                    }
#line 812 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 35: // constterm: "#inf"
#line 364 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, Symbol::createInf());
                    }
#line 818 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 36: // constterm: "#sup"
#line 365 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, Symbol::createSup());
                    }
#line 824 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 37: // consttermvec: constterm
#line 371 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec(BUILDER.termvec(), (yystack_[0].value.term));
                    }
#line 830 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 38: // consttermvec: consttermvec "," constterm
#line 372 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec((yystack_[2].value.termvec), (yystack_[0].value.term));
                    }
#line 836 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 39: // constargvec: consttermvec
#line 376 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvecvec) =
                            BUILDER.termvecvec(BUILDER.termvecvec(), (yystack_[0].value.termvec));
                    }
#line 842 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 40: // constargvec: %empty
#line 377 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvecvec) = BUILDER.termvecvec(BUILDER.termvecvec(), BUILDER.termvec());
                    }
#line 848 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 41: // term: term ".." term
#line 383 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, (yystack_[2].value.term), (yystack_[0].value.term));
                    }
#line 854 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 42: // term: term "^" term
#line 384 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::XOR, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 860 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 43: // term: term "?" term
#line 385 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, BinOp::OR, (yystack_[2].value.term), (yystack_[0].value.term));
                    }
#line 866 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 44: // term: term "&" term
#line 386 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::AND, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 872 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 45: // term: term "+" term
#line 387 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::ADD, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 878 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 46: // term: term "-" term
#line 388 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::SUB, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 884 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 47: // term: term "*" term
#line 389 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::MUL, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 890 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 48: // term: term "/" term
#line 390 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::DIV, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 896 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 49: // term: term "\\" term
#line 391 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::MOD, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 902 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 50: // term: term "**" term
#line 392 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BinOp::POW, (yystack_[2].value.term),
                                                          (yystack_[0].value.term));
                    }
#line 908 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 51: // term: "-" term
#line 393 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, UnOp::NEG, (yystack_[0].value.term));
                    }
#line 914 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 52: // term: "~" term
#line 394 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, UnOp::NOT, (yystack_[0].value.term));
                    }
#line 920 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 53: // term: "(" tuplevec ")"
#line 395 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.pool(yylhs.location, (yystack_[1].value.termvec));
                    }
#line 926 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 54: // term: identifier "(" argvec ")"
#line 396 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, String::fromRep((yystack_[3].value.str)),
                                                          (yystack_[1].value.termvecvec), false);
                    }
#line 932 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 55: // term: "@" identifier "(" argvec ")"
#line 397 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, String::fromRep((yystack_[3].value.str)),
                                                          (yystack_[1].value.termvecvec), true);
                    }
#line 938 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 56: // term: "|" unaryargvec "|"
#line 398 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, UnOp::ABS, (yystack_[1].value.termvec));
                    }
#line 944 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 57: // term: identifier
#line 399 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, Symbol::createId(String::fromRep((yystack_[0].value.str))));
                    }
#line 950 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 58: // term: "@" identifier
#line 400 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, String::fromRep((yystack_[0].value.str)),
                                         BUILDER.termvecvec(BUILDER.termvecvec(), BUILDER.termvec()), true);
                    }
#line 956 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 59: // term: "<NUMBER>"
#line 401 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, Symbol::createNum((yystack_[0].value.num)));
                    }
#line 962 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 60: // term: "<STRING>"
#line 402 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, Symbol::createStr(String::fromRep((yystack_[0].value.str))));
                    }
#line 968 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 61: // term: "#inf"
#line 403 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, Symbol::createInf());
                    }
#line 974 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 62: // term: "#sup"
#line 404 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, Symbol::createSup());
                    }
#line 980 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 63: // term: "<VARIABLE>"
#line 405 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, String::fromRep((yystack_[0].value.str)));
                    }
#line 986 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 64: // term: "<ANONYMOUS>"
#line 406 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, String("_"));
                    }
#line 992 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 65: // unaryargvec: term
#line 412 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec(BUILDER.termvec(), (yystack_[0].value.term));
                    }
#line 998 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 66: // unaryargvec: unaryargvec ";" term
#line 413 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec((yystack_[2].value.termvec), (yystack_[0].value.term));
                    }
#line 1004 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 67: // ntermvec: term
#line 419 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec(BUILDER.termvec(), (yystack_[0].value.term));
                    }
#line 1010 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 68: // ntermvec: ntermvec "," term
#line 420 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec((yystack_[2].value.termvec), (yystack_[0].value.term));
                    }
#line 1016 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 69: // termvec: ntermvec
#line 424 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = (yystack_[0].value.termvec);
                    }
#line 1022 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 70: // termvec: %empty
#line 425 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec();
                    }
#line 1028 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 71: // tuple: ntermvec ","
#line 429 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, (yystack_[1].value.termvec), true);
                    }
#line 1034 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 72: // tuple: ntermvec
#line 430 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, (yystack_[0].value.termvec), false);
                    }
#line 1040 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 73: // tuple: ","
#line 431 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BUILDER.termvec(), true);
                    }
#line 1046 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 74: // tuple: %empty
#line 432 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, BUILDER.termvec(), false);
                    }
#line 1052 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 75: // tuplevec_sem: tuple ";"
#line 435 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec(BUILDER.termvec(), (yystack_[1].value.term));
                    }
#line 1058 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 76: // tuplevec_sem: tuplevec_sem tuple ";"
#line 436 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec((yystack_[2].value.termvec), (yystack_[1].value.term));
                    }
#line 1064 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 77: // tuplevec: tuple
#line 439 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec(BUILDER.termvec(), (yystack_[0].value.term));
                    }
#line 1070 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 78: // tuplevec: tuplevec_sem tuple
#line 440 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec((yystack_[1].value.termvec), (yystack_[0].value.term));
                    }
#line 1076 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 79: // argvec: termvec
#line 443 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvecvec) =
                            BUILDER.termvecvec(BUILDER.termvecvec(), (yystack_[0].value.termvec));
                    }
#line 1082 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 80: // argvec: argvec ";" termvec
#line 444 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvecvec) =
                            BUILDER.termvecvec((yystack_[2].value.termvecvec), (yystack_[0].value.termvec));
                    }
#line 1088 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 81: // binaryargvec: term "," term
#line 448 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvecvec) = BUILDER.termvecvec(
                            BUILDER.termvecvec(),
                            BUILDER.termvec(BUILDER.termvec(BUILDER.termvec(), (yystack_[2].value.term)),
                                            (yystack_[0].value.term)));
                    }
#line 1094 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 82: // binaryargvec: binaryargvec ";" term "," term
#line 449 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvecvec) = BUILDER.termvecvec(
                            (yystack_[4].value.termvecvec),
                            BUILDER.termvec(BUILDER.termvec(BUILDER.termvec(), (yystack_[2].value.term)),
                                            (yystack_[0].value.term)));
                    }
#line 1100 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 83: // cmp: ">"
#line 459 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.rel) = Relation::GT;
                    }
#line 1106 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 84: // cmp: "<"
#line 460 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.rel) = Relation::LT;
                    }
#line 1112 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 85: // cmp: ">="
#line 461 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.rel) = Relation::GEQ;
                    }
#line 1118 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 86: // cmp: "<="
#line 462 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.rel) = Relation::LEQ;
                    }
#line 1124 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 87: // cmp: "="
#line 463 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.rel) = Relation::EQ;
                    }
#line 1130 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 88: // cmp: "!="
#line 464 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.rel) = Relation::NEQ;
                    }
#line 1136 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 89: // atom: identifier
#line 468 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.predRep(yylhs.location, false, String::fromRep((yystack_[0].value.str)),
                                            BUILDER.termvecvec(BUILDER.termvecvec(), BUILDER.termvec()));
                    }
#line 1142 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 90: // atom: identifier "(" argvec ")"
#line 469 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.predRep(yylhs.location, false, String::fromRep((yystack_[3].value.str)),
                                            (yystack_[1].value.termvecvec));
                    }
#line 1148 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 91: // atom: "-" identifier
#line 470 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.predRep(yylhs.location, true, String::fromRep((yystack_[0].value.str)),
                                            BUILDER.termvecvec(BUILDER.termvecvec(), BUILDER.termvec()));
                    }
#line 1154 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 92: // atom: "-" identifier "(" argvec ")"
#line 471 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.predRep(yylhs.location, true, String::fromRep((yystack_[3].value.str)),
                                            (yystack_[1].value.termvecvec));
                    }
#line 1160 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 93: // rellitvec: cmp term
#line 475 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.rellitvec) =
                            BUILDER.rellitvec(yylhs.location, (yystack_[1].value.rel), (yystack_[0].value.term));
                    }
#line 1166 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 94: // rellitvec: rellitvec cmp term
#line 476 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.rellitvec) = BUILDER.rellitvec(yylhs.location, (yystack_[2].value.rellitvec),
                                                                    (yystack_[1].value.rel), (yystack_[0].value.term));
                    }
#line 1172 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 95: // literal: "#true"
#line 480 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.boollit(yylhs.location, true);
                    }
#line 1178 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 96: // literal: "not" "#true"
#line 481 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.boollit(yylhs.location, false);
                    }
#line 1184 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 97: // literal: "not" "not" "#true"
#line 482 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.boollit(yylhs.location, true);
                    }
#line 1190 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 98: // literal: "#false"
#line 483 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.boollit(yylhs.location, false);
                    }
#line 1196 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 99: // literal: "not" "#false"
#line 484 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.boollit(yylhs.location, true);
                    }
#line 1202 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 100: // literal: "not" "not" "#false"
#line 485 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.boollit(yylhs.location, false);
                    }
#line 1208 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 101: // literal: atom
#line 486 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.predlit(yylhs.location, NAF::POS, (yystack_[0].value.term));
                    }
#line 1214 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 102: // literal: "not" atom
#line 487 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.predlit(yylhs.location, NAF::NOT, (yystack_[0].value.term));
                    }
#line 1220 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 103: // literal: "not" "not" atom
#line 488 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.predlit(yylhs.location, NAF::NOTNOT, (yystack_[0].value.term));
                    }
#line 1226 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 104: // literal: term rellitvec
#line 489 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.rellit(yylhs.location, NAF::POS, (yystack_[1].value.term),
                                                           (yystack_[0].value.rellitvec));
                    }
#line 1232 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 105: // literal: "not" term rellitvec
#line 490 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.rellit(yylhs.location, NAF::NOT, (yystack_[1].value.term),
                                                           (yystack_[0].value.rellitvec));
                    }
#line 1238 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 106: // literal: "not" "not" term rellitvec
#line 491 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lit) = BUILDER.rellit(yylhs.location, NAF::NOTNOT, (yystack_[1].value.term),
                                                           (yystack_[0].value.rellitvec));
                    }
#line 1244 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 107: // nlitvec: literal
#line 499 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.litvec) = BUILDER.litvec(BUILDER.litvec(), (yystack_[0].value.lit));
                    }
#line 1250 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 108: // nlitvec: nlitvec "," literal
#line 500 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.litvec) = BUILDER.litvec((yystack_[2].value.litvec), (yystack_[0].value.lit));
                    }
#line 1256 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 109: // litvec: nlitvec
#line 504 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.litvec) = (yystack_[0].value.litvec);
                    }
#line 1262 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 110: // litvec: %empty
#line 505 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.litvec) = BUILDER.litvec();
                    }
#line 1268 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 111: // optcondition: ":" litvec
#line 509 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.litvec) = (yystack_[0].value.litvec);
                    }
#line 1274 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 112: // optcondition: %empty
#line 510 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.litvec) = BUILDER.litvec();
                    }
#line 1280 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 113: // aggregatefunction: "#sum"
#line 514 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.fun) = AggregateFunction::SUM;
                    }
#line 1286 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 114: // aggregatefunction: "#sum+"
#line 515 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.fun) = AggregateFunction::SUMP;
                    }
#line 1292 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 115: // aggregatefunction: "#min"
#line 516 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.fun) = AggregateFunction::MIN;
                    }
#line 1298 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 116: // aggregatefunction: "#max"
#line 517 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.fun) = AggregateFunction::MAX;
                    }
#line 1304 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 117: // aggregatefunction: "#count"
#line 518 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.fun) = AggregateFunction::COUNT;
                    }
#line 1310 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 118: // bodyaggrelem: ":" litvec
#line 524 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.bodyaggrelem) = {BUILDER.termvec(), (yystack_[0].value.litvec)};
                    }
#line 1316 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 119: // bodyaggrelem: ntermvec optcondition
#line 525 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.bodyaggrelem) = {(yystack_[1].value.termvec), (yystack_[0].value.litvec)};
                    }
#line 1322 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 120: // bodyaggrelemvec: bodyaggrelem
#line 529 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.bodyaggrelemvec) =
                            BUILDER.bodyaggrelemvec(BUILDER.bodyaggrelemvec(), (yystack_[0].value.bodyaggrelem).first,
                                                    (yystack_[0].value.bodyaggrelem).second);
                    }
#line 1328 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 121: // bodyaggrelemvec: bodyaggrelemvec ";" bodyaggrelem
#line 530 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.bodyaggrelemvec) = BUILDER.bodyaggrelemvec(
                            (yystack_[2].value.bodyaggrelemvec), (yystack_[0].value.bodyaggrelem).first,
                            (yystack_[0].value.bodyaggrelem).second);
                    }
#line 1334 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 122: // altbodyaggrelem: literal optcondition
#line 536 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lbodyaggrelem) = {(yystack_[1].value.lit), (yystack_[0].value.litvec)};
                    }
#line 1340 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 123: // altbodyaggrelemvec: altbodyaggrelem
#line 540 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) =
                            BUILDER.condlitvec(BUILDER.condlitvec(), (yystack_[0].value.lbodyaggrelem).first,
                                               (yystack_[0].value.lbodyaggrelem).second);
                    }
#line 1346 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 124: // altbodyaggrelemvec: altbodyaggrelemvec ";" altbodyaggrelem
#line 541 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) =
                            BUILDER.condlitvec((yystack_[2].value.condlitlist), (yystack_[0].value.lbodyaggrelem).first,
                                               (yystack_[0].value.lbodyaggrelem).second);
                    }
#line 1352 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 125: // bodyaggregate: "{" "}"
#line 547 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.aggr) = {AggregateFunction::COUNT, true, BUILDER.condlitvec()};
                    }
#line 1358 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 126: // bodyaggregate: "{" altbodyaggrelemvec "}"
#line 548 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.aggr) = {AggregateFunction::COUNT, true, (yystack_[1].value.condlitlist)};
                    }
#line 1364 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 127: // bodyaggregate: aggregatefunction "{" "}"
#line 549 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.aggr) = {(yystack_[2].value.fun), false, BUILDER.bodyaggrelemvec()};
                    }
#line 1370 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 128: // bodyaggregate: aggregatefunction "{" bodyaggrelemvec "}"
#line 550 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.aggr) = {(yystack_[3].value.fun), false, (yystack_[1].value.bodyaggrelemvec)};
                    }
#line 1376 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 129: // upper: term
#line 554 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.bound) = {Relation::LEQ, (yystack_[0].value.term)};
                    }
#line 1382 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 130: // upper: cmp term
#line 555 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.bound) = {(yystack_[1].value.rel), (yystack_[0].value.term)};
                    }
#line 1388 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 131: // upper: %empty
#line 556 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.bound) = {Relation::LEQ, TermUid(-1)};
                    }
#line 1394 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 132: // lubodyaggregate: term bodyaggregate upper
#line 560 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->aggregate(
                            (yystack_[1].value.aggr).fun, (yystack_[1].value.aggr).choice,
                            (yystack_[1].value.aggr).elems,
                            lexer->boundvec(Relation::LEQ, (yystack_[2].value.term), (yystack_[0].value.bound).rel,
                                            (yystack_[0].value.bound).term));
                    }
#line 1400 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 133: // lubodyaggregate: term cmp bodyaggregate upper
#line 561 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->aggregate(
                            (yystack_[1].value.aggr).fun, (yystack_[1].value.aggr).choice,
                            (yystack_[1].value.aggr).elems,
                            lexer->boundvec((yystack_[2].value.rel), (yystack_[3].value.term),
                                            (yystack_[0].value.bound).rel, (yystack_[0].value.bound).term));
                    }
#line 1406 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 134: // lubodyaggregate: bodyaggregate upper
#line 562 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.uid) =
                            lexer->aggregate((yystack_[1].value.aggr).fun, (yystack_[1].value.aggr).choice,
                                             (yystack_[1].value.aggr).elems,
                                             lexer->boundvec(Relation::LEQ, TermUid(-1), (yystack_[0].value.bound).rel,
                                                             (yystack_[0].value.bound).term));
                    }
#line 1412 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 135: // lubodyaggregate: theory_atom
#line 563 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->aggregate((yystack_[0].value.theoryAtom));
                    }
#line 1418 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 136: // headaggrelemvec: headaggrelemvec ";" termvec ":" literal optcondition
#line 569 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.headaggrelemvec) =
                            BUILDER.headaggrelemvec((yystack_[5].value.headaggrelemvec), (yystack_[3].value.termvec),
                                                    (yystack_[1].value.lit), (yystack_[0].value.litvec));
                    }
#line 1424 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 137: // headaggrelemvec: termvec ":" literal optcondition
#line 570 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.headaggrelemvec) =
                            BUILDER.headaggrelemvec(BUILDER.headaggrelemvec(), (yystack_[3].value.termvec),
                                                    (yystack_[1].value.lit), (yystack_[0].value.litvec));
                    }
#line 1430 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 138: // altheadaggrelemvec: literal optcondition
#line 574 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec(BUILDER.condlitvec(), (yystack_[1].value.lit),
                                                                       (yystack_[0].value.litvec));
                    }
#line 1436 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 139: // altheadaggrelemvec: altheadaggrelemvec ";" literal optcondition
#line 575 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec(
                            (yystack_[3].value.condlitlist), (yystack_[1].value.lit), (yystack_[0].value.litvec));
                    }
#line 1442 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 140: // headaggregate: aggregatefunction "{" "}"
#line 581 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.aggr) = {(yystack_[2].value.fun), false, BUILDER.headaggrelemvec()};
                    }
#line 1448 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 141: // headaggregate: aggregatefunction "{" headaggrelemvec "}"
#line 582 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.aggr) = {(yystack_[3].value.fun), false, (yystack_[1].value.headaggrelemvec)};
                    }
#line 1454 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 142: // headaggregate: "{" "}"
#line 583 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.aggr) = {AggregateFunction::COUNT, true, BUILDER.condlitvec()};
                    }
#line 1460 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 143: // headaggregate: "{" altheadaggrelemvec "}"
#line 584 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.aggr) = {AggregateFunction::COUNT, true, (yystack_[1].value.condlitlist)};
                    }
#line 1466 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 144: // luheadaggregate: term headaggregate upper
#line 588 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->aggregate(
                            (yystack_[1].value.aggr).fun, (yystack_[1].value.aggr).choice,
                            (yystack_[1].value.aggr).elems,
                            lexer->boundvec(Relation::LEQ, (yystack_[2].value.term), (yystack_[0].value.bound).rel,
                                            (yystack_[0].value.bound).term));
                    }
#line 1472 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 145: // luheadaggregate: term cmp headaggregate upper
#line 589 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->aggregate(
                            (yystack_[1].value.aggr).fun, (yystack_[1].value.aggr).choice,
                            (yystack_[1].value.aggr).elems,
                            lexer->boundvec((yystack_[2].value.rel), (yystack_[3].value.term),
                                            (yystack_[0].value.bound).rel, (yystack_[0].value.bound).term));
                    }
#line 1478 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 146: // luheadaggregate: headaggregate upper
#line 590 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.uid) =
                            lexer->aggregate((yystack_[1].value.aggr).fun, (yystack_[1].value.aggr).choice,
                                             (yystack_[1].value.aggr).elems,
                                             lexer->boundvec(Relation::LEQ, TermUid(-1), (yystack_[0].value.bound).rel,
                                                             (yystack_[0].value.bound).term));
                    }
#line 1484 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 147: // luheadaggregate: theory_atom
#line 591 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.uid) = lexer->aggregate((yystack_[0].value.theoryAtom));
                    }
#line 1490 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 148: // conjunction: literal ":" litvec
#line 598 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.lbodyaggrelem) = {(yystack_[2].value.lit), (yystack_[0].value.litvec)};
                    }
#line 1496 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 151: // disjunctionsep: disjunctionsep literal ","
#line 612 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec((yystack_[2].value.condlitlist),
                                                                       (yystack_[1].value.lit), BUILDER.litvec());
                    }
#line 1502 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 152: // disjunctionsep: disjunctionsep literal dsym
#line 613 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec((yystack_[2].value.condlitlist),
                                                                       (yystack_[1].value.lit), BUILDER.litvec());
                    }
#line 1508 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 153: // disjunctionsep: disjunctionsep literal ":" ";"
#line 614 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec((yystack_[3].value.condlitlist),
                                                                       (yystack_[2].value.lit), BUILDER.litvec());
                    }
#line 1514 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 154: // disjunctionsep: disjunctionsep literal ":" nlitvec dsym
#line 615 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec(
                            (yystack_[4].value.condlitlist), (yystack_[3].value.lit), (yystack_[1].value.litvec));
                    }
#line 1520 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 155: // disjunctionsep: literal ","
#line 616 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) =
                            BUILDER.condlitvec(BUILDER.condlitvec(), (yystack_[1].value.lit), BUILDER.litvec());
                    }
#line 1526 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 156: // disjunctionsep: literal dsym
#line 617 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) =
                            BUILDER.condlitvec(BUILDER.condlitvec(), (yystack_[1].value.lit), BUILDER.litvec());
                    }
#line 1532 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 157: // disjunctionsep: literal ":" nlitvec dsym
#line 618 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec(BUILDER.condlitvec(), (yystack_[3].value.lit),
                                                                       (yystack_[1].value.litvec));
                    }
#line 1538 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 158: // disjunctionsep: literal ":" ";"
#line 619 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) =
                            BUILDER.condlitvec(BUILDER.condlitvec(), (yystack_[2].value.lit), BUILDER.litvec());
                    }
#line 1544 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 159: // disjunction: disjunctionsep literal optcondition
#line 623 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec(
                            (yystack_[2].value.condlitlist), (yystack_[1].value.lit), (yystack_[0].value.litvec));
                    }
#line 1550 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 160: // disjunction: literal ":" litvec
#line 624 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.condlitlist) = BUILDER.condlitvec(BUILDER.condlitvec(), (yystack_[2].value.lit),
                                                                       (yystack_[0].value.litvec));
                    }
#line 1556 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 161: // bodycomma: bodycomma literal ","
#line 631 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.bodylit((yystack_[2].value.body), (yystack_[1].value.lit));
                    }
#line 1562 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 162: // bodycomma: bodycomma literal ";"
#line 632 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.bodylit((yystack_[2].value.body), (yystack_[1].value.lit));
                    }
#line 1568 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 163: // bodycomma: bodycomma lubodyaggregate ","
#line 633 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = lexer->bodyaggregate((yystack_[2].value.body), yystack_[1].location,
                                                                  NAF::POS, (yystack_[1].value.uid));
                    }
#line 1574 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 164: // bodycomma: bodycomma lubodyaggregate ";"
#line 634 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = lexer->bodyaggregate((yystack_[2].value.body), yystack_[1].location,
                                                                  NAF::POS, (yystack_[1].value.uid));
                    }
#line 1580 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 165: // bodycomma: bodycomma "not" lubodyaggregate ","
#line 635 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) =
                            lexer->bodyaggregate((yystack_[3].value.body), yystack_[1].location + yystack_[2].location,
                                                 NAF::NOT, (yystack_[1].value.uid));
                    }
#line 1586 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 166: // bodycomma: bodycomma "not" lubodyaggregate ";"
#line 636 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) =
                            lexer->bodyaggregate((yystack_[3].value.body), yystack_[1].location + yystack_[2].location,
                                                 NAF::NOT, (yystack_[1].value.uid));
                    }
#line 1592 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 167: // bodycomma: bodycomma "not" "not" lubodyaggregate ","
#line 637 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) =
                            lexer->bodyaggregate((yystack_[4].value.body), yystack_[1].location + yystack_[3].location,
                                                 NAF::NOTNOT, (yystack_[1].value.uid));
                    }
#line 1598 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 168: // bodycomma: bodycomma "not" "not" lubodyaggregate ";"
#line 638 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) =
                            lexer->bodyaggregate((yystack_[4].value.body), yystack_[1].location + yystack_[3].location,
                                                 NAF::NOTNOT, (yystack_[1].value.uid));
                    }
#line 1604 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 169: // bodycomma: bodycomma conjunction ";"
#line 639 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.conjunction((yystack_[2].value.body), yystack_[1].location,
                                                                 (yystack_[1].value.lbodyaggrelem).first,
                                                                 (yystack_[1].value.lbodyaggrelem).second);
                    }
#line 1610 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 170: // bodycomma: %empty
#line 640 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.body();
                    }
#line 1616 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 171: // bodydot: bodycomma literal "."
#line 644 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.bodylit((yystack_[2].value.body), (yystack_[1].value.lit));
                    }
#line 1622 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 172: // bodydot: bodycomma lubodyaggregate "."
#line 645 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = lexer->bodyaggregate((yystack_[2].value.body), yystack_[1].location,
                                                                  NAF::POS, (yystack_[1].value.uid));
                    }
#line 1628 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 173: // bodydot: bodycomma "not" lubodyaggregate "."
#line 646 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) =
                            lexer->bodyaggregate((yystack_[3].value.body), yystack_[1].location + yystack_[2].location,
                                                 NAF::NOT, (yystack_[1].value.uid));
                    }
#line 1634 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 174: // bodydot: bodycomma "not" "not" lubodyaggregate "."
#line 647 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) =
                            lexer->bodyaggregate((yystack_[4].value.body), yystack_[1].location + yystack_[3].location,
                                                 NAF::NOTNOT, (yystack_[1].value.uid));
                    }
#line 1640 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 175: // bodydot: bodycomma conjunction "."
#line 648 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.conjunction((yystack_[2].value.body), yystack_[1].location,
                                                                 (yystack_[1].value.lbodyaggrelem).first,
                                                                 (yystack_[1].value.lbodyaggrelem).second);
                    }
#line 1646 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 176: // bodyconddot: "."
#line 652 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.body();
                    }
#line 1652 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 177: // bodyconddot: ":" "."
#line 653 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.body();
                    }
#line 1658 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 178: // bodyconddot: ":" bodydot
#line 654 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = (yystack_[0].value.body);
                    }
#line 1664 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 179: // head: literal
#line 657 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.head) = BUILDER.headlit((yystack_[0].value.lit));
                    }
#line 1670 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 180: // head: disjunction
#line 658 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.head) = BUILDER.disjunction(yylhs.location, (yystack_[0].value.condlitlist));
                    }
#line 1676 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 181: // head: luheadaggregate
#line 659 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.head) = lexer->headaggregate(yylhs.location, (yystack_[0].value.uid));
                    }
#line 1682 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 182: // statement: head "."
#line 663 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.rule(yylhs.location, (yystack_[1].value.head));
                    }
#line 1688 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 183: // statement: head ":-" "."
#line 664 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.rule(yylhs.location, (yystack_[2].value.head));
                    }
#line 1694 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 184: // statement: head ":-" bodydot
#line 665 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.rule(yylhs.location, (yystack_[2].value.head), (yystack_[0].value.body));
                    }
#line 1700 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 185: // statement: ":-" bodydot
#line 666 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.rule(yylhs.location, BUILDER.headlit(BUILDER.boollit(yylhs.location, false)),
                                     (yystack_[0].value.body));
                    }
#line 1706 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 186: // statement: ":-" "."
#line 667 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.rule(yylhs.location, BUILDER.headlit(BUILDER.boollit(yylhs.location, false)),
                                     BUILDER.body());
                    }
#line 1712 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 187: // optimizetuple: "," ntermvec
#line 673 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = (yystack_[0].value.termvec);
                    }
#line 1718 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 188: // optimizetuple: %empty
#line 674 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termvec) = BUILDER.termvec();
                    }
#line 1724 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 189: // optimizeweight: term "@" term
#line 678 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termpair) = {(yystack_[2].value.term), (yystack_[0].value.term)};
                    }
#line 1730 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 190: // optimizeweight: term
#line 679 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.termpair) = {(yystack_[0].value.term),
                                                  BUILDER.term(yylhs.location, Symbol::createNum(0))};
                    }
#line 1736 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 191: // optimizelitvec: literal
#line 683 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.bodylit(BUILDER.body(), (yystack_[0].value.lit));
                    }
#line 1742 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 192: // optimizelitvec: optimizelitvec "," literal
#line 684 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.bodylit((yystack_[2].value.body), (yystack_[0].value.lit));
                    }
#line 1748 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 193: // optimizecond: ":" optimizelitvec
#line 688 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = (yystack_[0].value.body);
                    }
#line 1754 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 194: // optimizecond: ":"
#line 689 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.body();
                    }
#line 1760 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 195: // optimizecond: %empty
#line 690 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.body) = BUILDER.body();
                    }
#line 1766 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 196: // statement: ":~" bodydot "[" optimizeweight optimizetuple "]"
#line 694 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.optimize(yylhs.location, (yystack_[2].value.termpair).first,
                                         (yystack_[2].value.termpair).second, (yystack_[1].value.termvec),
                                         (yystack_[4].value.body));
                    }
#line 1772 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 197: // statement: ":~" "." "[" optimizeweight optimizetuple "]"
#line 695 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.optimize(yylhs.location, (yystack_[2].value.termpair).first,
                                         (yystack_[2].value.termpair).second, (yystack_[1].value.termvec),
                                         BUILDER.body());
                    }
#line 1778 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 198: // maxelemlist: optimizeweight optimizetuple optimizecond
#line 699 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.optimize(
                            yylhs.location,
                            BUILDER.term(yystack_[2].location, UnOp::NEG, (yystack_[2].value.termpair).first),
                            (yystack_[2].value.termpair).second, (yystack_[1].value.termvec), (yystack_[0].value.body));
                    }
#line 1784 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 199: // maxelemlist: maxelemlist ";" optimizeweight optimizetuple optimizecond
#line 700 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.optimize(
                            yylhs.location,
                            BUILDER.term(yystack_[2].location, UnOp::NEG, (yystack_[2].value.termpair).first),
                            (yystack_[2].value.termpair).second, (yystack_[1].value.termvec), (yystack_[0].value.body));
                    }
#line 1790 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 200: // minelemlist: optimizeweight optimizetuple optimizecond
#line 704 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.optimize(yylhs.location, (yystack_[2].value.termpair).first,
                                         (yystack_[2].value.termpair).second, (yystack_[1].value.termvec),
                                         (yystack_[0].value.body));
                    }
#line 1796 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 201: // minelemlist: minelemlist ";" optimizeweight optimizetuple optimizecond
#line 705 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.optimize(yylhs.location, (yystack_[2].value.termpair).first,
                                         (yystack_[2].value.termpair).second, (yystack_[1].value.termvec),
                                         (yystack_[0].value.body));
                    }
#line 1802 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 206: // statement: "#showsig" identifier "/" "<NUMBER>" "."
#line 718 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.showsig(yylhs.location,
                                        Sig(String::fromRep((yystack_[3].value.str)), (yystack_[1].value.num), false));
                    }
#line 1808 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 207: // statement: "#showsig" "-" identifier "/" "<NUMBER>" "."
#line 719 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.showsig(yylhs.location,
                                        Sig(String::fromRep((yystack_[3].value.str)), (yystack_[1].value.num), true));
                    }
#line 1814 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 208: // statement: "#show" "."
#line 720 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.showsig(yylhs.location, Sig("", 0, false));
                    }
#line 1820 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 209: // statement: "#show" term ":" bodydot
#line 721 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.show(yylhs.location, (yystack_[2].value.term), (yystack_[0].value.body));
                    }
#line 1826 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 210: // statement: "#show" term "."
#line 722 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.show(yylhs.location, (yystack_[1].value.term), BUILDER.body());
                    }
#line 1832 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 211: // statement: "#defined" identifier "/" "<NUMBER>" "."
#line 728 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.defined(yylhs.location,
                                        Sig(String::fromRep((yystack_[3].value.str)), (yystack_[1].value.num), false));
                    }
#line 1838 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 212: // statement: "#defined" "-" identifier "/" "<NUMBER>" "."
#line 729 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.defined(yylhs.location,
                                        Sig(String::fromRep((yystack_[3].value.str)), (yystack_[1].value.num), true));
                    }
#line 1844 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 213: // statement: "#edge" "(" binaryargvec ")" bodyconddot
#line 734 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.edge(yylhs.location, (yystack_[2].value.termvecvec), (yystack_[0].value.body));
                    }
#line 1850 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 214: // statement: "#heuristic" atom bodyconddot "[" term "@" term "," term "]"
#line 740 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.heuristic(yylhs.location, (yystack_[8].value.term), (yystack_[7].value.body),
                                          (yystack_[5].value.term), (yystack_[3].value.term), (yystack_[1].value.term));
                    }
#line 1856 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 215: // statement: "#heuristic" atom bodyconddot "[" term "," term "]"
#line 741 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.heuristic(yylhs.location, (yystack_[6].value.term), (yystack_[5].value.body),
                                          (yystack_[3].value.term), BUILDER.term(yylhs.location, Symbol::createNum(0)),
                                          (yystack_[1].value.term));
                    }
#line 1862 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 216: // statement: "#project" identifier "/" "<NUMBER>" "."
#line 747 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.project(yylhs.location,
                                        Sig(String::fromRep((yystack_[3].value.str)), (yystack_[1].value.num), false));
                    }
#line 1868 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 217: // statement: "#project" "-" identifier "/" "<NUMBER>" "."
#line 748 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.project(yylhs.location,
                                        Sig(String::fromRep((yystack_[3].value.str)), (yystack_[1].value.num), true));
                    }
#line 1874 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 218: // statement: "#project" atom bodyconddot
#line 749 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.project(yylhs.location, (yystack_[1].value.term), (yystack_[0].value.body));
                    }
#line 1880 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 219: // define: identifier "=" constterm
#line 755 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.define(yylhs.location, String::fromRep((yystack_[2].value.str)),
                                       (yystack_[0].value.term), false, LOGGER);
                    }
#line 1886 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 220: // storecomment: %empty
#line 759 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->storeComments();
                    }
#line 1892 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 221: // statement: "#const" identifier "=" constterm "." storecomment
#line 762 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.define(yylhs.location, String::fromRep((yystack_[4].value.str)),
                                       (yystack_[2].value.term), true, LOGGER);
                        lexer->flushComments();
                    }
#line 1898 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 222: // statement: "#const" identifier "=" constterm "." storecomment "[" "default" "]"
#line 763 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->flushComments();
                        BUILDER.define(yylhs.location, String::fromRep((yystack_[7].value.str)),
                                       (yystack_[5].value.term), true, LOGGER);
                    }
#line 1904 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 223: // statement: "#const" identifier "=" constterm "." storecomment "[" "override" "]"
#line 764 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->flushComments();
                        BUILDER.define(yylhs.location, String::fromRep((yystack_[7].value.str)),
                                       (yystack_[5].value.term), false, LOGGER);
                    }
#line 1910 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 224: // statement: "<SCRIPT>" "(" "<IDENTIFIER>" ")" "<CODE>" "."
#line 770 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.script(yylhs.location, String::fromRep((yystack_[3].value.str)),
                                       String::fromRep((yystack_[1].value.str)));
                    }
#line 1916 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 225: // statement: "#include" "<STRING>" "."
#line 776 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->include(String::fromRep((yystack_[1].value.str)), yylhs.location, false, LOGGER);
                    }
#line 1922 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 226: // statement: "#include" "<" identifier ">" "."
#line 777 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->include(String::fromRep((yystack_[2].value.str)), yylhs.location, true, LOGGER);
                    }
#line 1928 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 227: // nidlist: nidlist "," identifier
#line 783 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.idlist) = BUILDER.idvec((yystack_[2].value.idlist), yystack_[0].location,
                                                             String::fromRep((yystack_[0].value.str)));
                    }
#line 1934 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 228: // nidlist: identifier
#line 784 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.idlist) = BUILDER.idvec(BUILDER.idvec(), yystack_[0].location,
                                                             String::fromRep((yystack_[0].value.str)));
                    }
#line 1940 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 229: // idlist: %empty
#line 788 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.idlist) = BUILDER.idvec();
                    }
#line 1946 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 230: // idlist: nidlist
#line 789 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.idlist) = (yystack_[0].value.idlist);
                    }
#line 1952 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 231: // statement: "#program" identifier "(" idlist ")" "."
#line 793 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.block(yylhs.location, String::fromRep((yystack_[4].value.str)),
                                      (yystack_[2].value.idlist));
                    }
#line 1958 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 232: // statement: "#program" identifier "."
#line 794 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.block(yylhs.location, String::fromRep((yystack_[1].value.str)), BUILDER.idvec());
                    }
#line 1964 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 233: // statement: "#external" atom ":" bodydot storecomment
#line 800 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.external(yylhs.location, (yystack_[3].value.term), (yystack_[1].value.body),
                                         BUILDER.term(yylhs.location, Symbol::createId("false")));
                        lexer->flushComments();
                    }
#line 1970 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 234: // statement: "#external" atom ":" "." storecomment
#line 801 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.external(yylhs.location, (yystack_[3].value.term), BUILDER.body(),
                                         BUILDER.term(yylhs.location, Symbol::createId("false")));
                        lexer->flushComments();
                    }
#line 1976 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 235: // statement: "#external" atom "." storecomment
#line 802 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.external(yylhs.location, (yystack_[2].value.term), BUILDER.body(),
                                         BUILDER.term(yylhs.location, Symbol::createId("false")));
                        lexer->flushComments();
                    }
#line 1982 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 236: // statement: "#external" atom ":" bodydot storecomment "[" term "]"
#line 803 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->flushComments();
                        BUILDER.external(yylhs.location, (yystack_[6].value.term), (yystack_[4].value.body),
                                         (yystack_[1].value.term));
                    }
#line 1988 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 237: // statement: "#external" atom ":" "." storecomment "[" term "]"
#line 804 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->flushComments();
                        BUILDER.external(yylhs.location, (yystack_[6].value.term), BUILDER.body(),
                                         (yystack_[1].value.term));
                    }
#line 1994 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 238: // statement: "#external" atom "." storecomment "[" term "]"
#line 805 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->flushComments();
                        BUILDER.external(yylhs.location, (yystack_[5].value.term), BUILDER.body(),
                                         (yystack_[1].value.term));
                    }
#line 2000 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 239: // theory_op: "<THEORYOP>"
#line 811 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = (yystack_[0].value.str);
                    }
#line 2006 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 240: // theory_op: "not"
#line 812 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = (yystack_[0].value.str);
                    }
#line 2012 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 241: // theory_op_list: theory_op_list theory_op
#line 818 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOps) =
                            BUILDER.theoryops((yystack_[1].value.theoryOps), String::fromRep((yystack_[0].value.str)));
                    }
#line 2018 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 242: // theory_op_list: theory_op
#line 819 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOps) =
                            BUILDER.theoryops(BUILDER.theoryops(), String::fromRep((yystack_[0].value.str)));
                    }
#line 2024 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 243: // theory_term: "{" theory_opterm_list "}"
#line 823 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) =
                            BUILDER.theorytermset(yylhs.location, (yystack_[1].value.theoryOpterms));
                    }
#line 2030 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 244: // theory_term: "[" theory_opterm_list "]"
#line 824 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) =
                            BUILDER.theoryoptermlist(yylhs.location, (yystack_[1].value.theoryOpterms));
                    }
#line 2036 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 245: // theory_term: "(" ")"
#line 825 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) = BUILDER.theorytermtuple(yylhs.location, BUILDER.theoryopterms());
                    }
#line 2042 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 246: // theory_term: "(" theory_opterm ")"
#line 826 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) =
                            BUILDER.theorytermopterm(yylhs.location, (yystack_[1].value.theoryOpterm));
                    }
#line 2048 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 247: // theory_term: "(" theory_opterm "," ")"
#line 827 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) = BUILDER.theorytermtuple(
                            yylhs.location, BUILDER.theoryopterms(BUILDER.theoryopterms(), yystack_[2].location,
                                                                  (yystack_[2].value.theoryOpterm)));
                    }
#line 2054 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 248: // theory_term: "(" theory_opterm "," theory_opterm_nlist ")"
#line 828 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) = BUILDER.theorytermtuple(
                            yylhs.location,
                            BUILDER.theoryopterms(yystack_[3].location, (yystack_[3].value.theoryOpterm),
                                                  (yystack_[1].value.theoryOpterms)));
                    }
#line 2060 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 249: // theory_term: identifier "(" theory_opterm_list ")"
#line 829 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) =
                            BUILDER.theorytermfun(yylhs.location, String::fromRep((yystack_[3].value.str)),
                                                  (yystack_[1].value.theoryOpterms));
                    }
#line 2066 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 250: // theory_term: identifier
#line 830 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) = BUILDER.theorytermvalue(
                            yylhs.location, Symbol::createId(String::fromRep((yystack_[0].value.str))));
                    }
#line 2072 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 251: // theory_term: "<NUMBER>"
#line 831 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) =
                            BUILDER.theorytermvalue(yylhs.location, Symbol::createNum((yystack_[0].value.num)));
                    }
#line 2078 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 252: // theory_term: "<STRING>"
#line 832 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) = BUILDER.theorytermvalue(
                            yylhs.location, Symbol::createStr(String::fromRep((yystack_[0].value.str))));
                    }
#line 2084 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 253: // theory_term: "#inf"
#line 833 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) = BUILDER.theorytermvalue(yylhs.location, Symbol::createInf());
                    }
#line 2090 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 254: // theory_term: "#sup"
#line 834 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) = BUILDER.theorytermvalue(yylhs.location, Symbol::createSup());
                    }
#line 2096 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 255: // theory_term: "<VARIABLE>"
#line 835 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTerm) =
                            BUILDER.theorytermvar(yylhs.location, String::fromRep((yystack_[0].value.str)));
                    }
#line 2102 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 256: // theory_opterm: theory_opterm theory_op_list theory_term
#line 839 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpterm) =
                            BUILDER.theoryopterm((yystack_[2].value.theoryOpterm), (yystack_[1].value.theoryOps),
                                                 (yystack_[0].value.theoryTerm));
                    }
#line 2108 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 257: // theory_opterm: theory_op_list theory_term
#line 840 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpterm) =
                            BUILDER.theoryopterm((yystack_[1].value.theoryOps), (yystack_[0].value.theoryTerm));
                    }
#line 2114 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 258: // theory_opterm: theory_term
#line 841 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpterm) =
                            BUILDER.theoryopterm(BUILDER.theoryops(), (yystack_[0].value.theoryTerm));
                    }
#line 2120 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 259: // theory_opterm_nlist: theory_opterm_nlist "," theory_opterm
#line 845 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpterms) = BUILDER.theoryopterms(
                            (yystack_[2].value.theoryOpterms), yystack_[0].location, (yystack_[0].value.theoryOpterm));
                    }
#line 2126 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 260: // theory_opterm_nlist: theory_opterm
#line 846 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpterms) = BUILDER.theoryopterms(
                            BUILDER.theoryopterms(), yystack_[0].location, (yystack_[0].value.theoryOpterm));
                    }
#line 2132 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 261: // theory_opterm_list: theory_opterm_nlist
#line 850 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpterms) = (yystack_[0].value.theoryOpterms);
                    }
#line 2138 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 262: // theory_opterm_list: %empty
#line 851 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpterms) = BUILDER.theoryopterms();
                    }
#line 2144 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 263: // theory_atom_element: theory_opterm_nlist disable_theory_lexing optcondition
#line 855 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryElem) = {(yystack_[2].value.theoryOpterms), (yystack_[0].value.litvec)};
                    }
#line 2150 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 264: // theory_atom_element: disable_theory_lexing ":" litvec
#line 856 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryElem) = {BUILDER.theoryopterms(), (yystack_[0].value.litvec)};
                    }
#line 2156 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 265: // theory_atom_element_nlist: theory_atom_element_nlist enable_theory_lexing ";"
                              // theory_atom_element
#line 860 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryElems) =
                            BUILDER.theoryelems((yystack_[3].value.theoryElems), (yystack_[0].value.theoryElem).first,
                                                (yystack_[0].value.theoryElem).second);
                    }
#line 2162 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 266: // theory_atom_element_nlist: theory_atom_element
#line 861 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryElems) =
                            BUILDER.theoryelems(BUILDER.theoryelems(), (yystack_[0].value.theoryElem).first,
                                                (yystack_[0].value.theoryElem).second);
                    }
#line 2168 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 267: // theory_atom_element_list: theory_atom_element_nlist
#line 865 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryElems) = (yystack_[0].value.theoryElems);
                    }
#line 2174 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 268: // theory_atom_element_list: %empty
#line 866 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryElems) = BUILDER.theoryelems();
                    }
#line 2180 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 269: // theory_atom_name: identifier
#line 870 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) =
                            BUILDER.term(yylhs.location, String::fromRep((yystack_[0].value.str)),
                                         BUILDER.termvecvec(BUILDER.termvecvec(), BUILDER.termvec()), false);
                    }
#line 2186 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 270: // theory_atom_name: identifier "(" argvec ")"
#line 871 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.term) = BUILDER.term(yylhs.location, String::fromRep((yystack_[3].value.str)),
                                                          (yystack_[1].value.termvecvec), false);
                    }
#line 2192 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 271: // theory_atom: "&" theory_atom_name
#line 874 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtom) = BUILDER.theoryatom((yystack_[0].value.term), BUILDER.theoryelems());
                    }
#line 2198 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 272: // theory_atom: "&" theory_atom_name enable_theory_lexing "{" theory_atom_element_list
                              // enable_theory_lexing "}" disable_theory_lexing
#line 875 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtom) =
                            BUILDER.theoryatom((yystack_[6].value.term), (yystack_[3].value.theoryElems));
                    }
#line 2204 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 273: // theory_atom: "&" theory_atom_name enable_theory_lexing "{" theory_atom_element_list
                              // enable_theory_lexing "}" theory_op theory_opterm disable_theory_lexing
#line 876 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtom) =
                            BUILDER.theoryatom((yystack_[8].value.term), (yystack_[5].value.theoryElems),
                                               String::fromRep((yystack_[2].value.str)), yystack_[1].location,
                                               (yystack_[1].value.theoryOpterm));
                    }
#line 2210 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 274: // theory_operator_nlist: theory_op
#line 882 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOps) =
                            BUILDER.theoryops(BUILDER.theoryops(), String::fromRep((yystack_[0].value.str)));
                    }
#line 2216 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 275: // theory_operator_nlist: theory_operator_nlist "," theory_op
#line 883 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOps) =
                            BUILDER.theoryops((yystack_[2].value.theoryOps), String::fromRep((yystack_[0].value.str)));
                    }
#line 2222 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 276: // theory_operator_list: theory_operator_nlist
#line 887 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOps) = (yystack_[0].value.theoryOps);
                    }
#line 2228 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 277: // theory_operator_list: %empty
#line 888 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOps) = BUILDER.theoryops();
                    }
#line 2234 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 278: // theory_operator_definition: theory_op enable_theory_definition_lexing ":" "<NUMBER>"
                              // "," "unary"
#line 892 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpDef) =
                            BUILDER.theoryopdef(yylhs.location, String::fromRep((yystack_[5].value.str)),
                                                (yystack_[2].value.num), TheoryOperatorType::Unary);
                    }
#line 2240 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 279: // theory_operator_definition: theory_op enable_theory_definition_lexing ":" "<NUMBER>"
                              // "," "binary" "," "left"
#line 893 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpDef) =
                            BUILDER.theoryopdef(yylhs.location, String::fromRep((yystack_[7].value.str)),
                                                (yystack_[4].value.num), TheoryOperatorType::BinaryLeft);
                    }
#line 2246 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 280: // theory_operator_definition: theory_op enable_theory_definition_lexing ":" "<NUMBER>"
                              // "," "binary" "," "right"
#line 894 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpDef) =
                            BUILDER.theoryopdef(yylhs.location, String::fromRep((yystack_[7].value.str)),
                                                (yystack_[4].value.num), TheoryOperatorType::BinaryRight);
                    }
#line 2252 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 281: // theory_operator_definition_nlist: theory_operator_definition
#line 898 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpDefs) =
                            BUILDER.theoryopdefs(BUILDER.theoryopdefs(), (yystack_[0].value.theoryOpDef));
                    }
#line 2258 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 282: // theory_operator_definition_nlist: theory_operator_definition_nlist enable_theory_lexing
                              // ";" theory_operator_definition
#line 899 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpDefs) =
                            BUILDER.theoryopdefs((yystack_[3].value.theoryOpDefs), (yystack_[0].value.theoryOpDef));
                    }
#line 2264 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 283: // theory_operator_definiton_list: theory_operator_definition_nlist
#line 903 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpDefs) = (yystack_[0].value.theoryOpDefs);
                    }
#line 2270 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 284: // theory_operator_definiton_list: %empty
#line 904 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryOpDefs) = BUILDER.theoryopdefs();
                    }
#line 2276 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 285: // theory_definition_identifier: identifier
#line 908 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = (yystack_[0].value.str);
                    }
#line 2282 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 286: // theory_definition_identifier: "left"
#line 909 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = String::toRep("left");
                    }
#line 2288 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 287: // theory_definition_identifier: "right"
#line 910 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = String::toRep("right");
                    }
#line 2294 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 288: // theory_definition_identifier: "unary"
#line 911 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = String::toRep("unary");
                    }
#line 2300 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 289: // theory_definition_identifier: "binary"
#line 912 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = String::toRep("binary");
                    }
#line 2306 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 290: // theory_definition_identifier: "head"
#line 913 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = String::toRep("head");
                    }
#line 2312 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 291: // theory_definition_identifier: "body"
#line 914 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = String::toRep("body");
                    }
#line 2318 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 292: // theory_definition_identifier: "any"
#line 915 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = String::toRep("any");
                    }
#line 2324 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 293: // theory_definition_identifier: "directive"
#line 916 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.str) = String::toRep("directive");
                    }
#line 2330 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 294: // theory_term_definition: theory_definition_identifier enable_theory_lexing "{"
                              // theory_operator_definiton_list enable_theory_definition_lexing "}"
#line 920 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryTermDef) =
                            BUILDER.theorytermdef(yylhs.location, String::fromRep((yystack_[5].value.str)),
                                                  (yystack_[2].value.theoryOpDefs), LOGGER);
                    }
#line 2336 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 295: // theory_atom_type: "head"
#line 924 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtomType) = TheoryAtomType::Head;
                    }
#line 2342 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 296: // theory_atom_type: "body"
#line 925 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtomType) = TheoryAtomType::Body;
                    }
#line 2348 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 297: // theory_atom_type: "any"
#line 926 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtomType) = TheoryAtomType::Any;
                    }
#line 2354 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 298: // theory_atom_type: "directive"
#line 927 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtomType) = TheoryAtomType::Directive;
                    }
#line 2360 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 299: // theory_atom_definition: "&" theory_definition_identifier "/" "<NUMBER>" ":"
                              // theory_definition_identifier "," enable_theory_lexing "{" theory_operator_list
                              // enable_theory_definition_lexing "}" "," theory_definition_identifier ","
                              // theory_atom_type
#line 932 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtomDef) = BUILDER.theoryatomdef(
                            yylhs.location, String::fromRep((yystack_[14].value.str)), (yystack_[12].value.num),
                            String::fromRep((yystack_[10].value.str)), (yystack_[0].value.theoryAtomType),
                            (yystack_[6].value.theoryOps), String::fromRep((yystack_[2].value.str)));
                    }
#line 2366 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 300: // theory_atom_definition: "&" theory_definition_identifier "/" "<NUMBER>" ":"
                              // theory_definition_identifier "," theory_atom_type
#line 933 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryAtomDef) = BUILDER.theoryatomdef(
                            yylhs.location, String::fromRep((yystack_[6].value.str)), (yystack_[4].value.num),
                            String::fromRep((yystack_[2].value.str)), (yystack_[0].value.theoryAtomType));
                    }
#line 2372 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 301: // theory_definition_nlist: theory_definition_list ";" theory_atom_definition
#line 937 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryDefs) =
                            BUILDER.theorydefs((yystack_[2].value.theoryDefs), (yystack_[0].value.theoryAtomDef));
                    }
#line 2378 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 302: // theory_definition_nlist: theory_definition_list ";" theory_term_definition
#line 938 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryDefs) =
                            BUILDER.theorydefs((yystack_[2].value.theoryDefs), (yystack_[0].value.theoryTermDef));
                    }
#line 2384 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 303: // theory_definition_nlist: theory_atom_definition
#line 939 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryDefs) =
                            BUILDER.theorydefs(BUILDER.theorydefs(), (yystack_[0].value.theoryAtomDef));
                    }
#line 2390 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 304: // theory_definition_nlist: theory_term_definition
#line 940 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryDefs) =
                            BUILDER.theorydefs(BUILDER.theorydefs(), (yystack_[0].value.theoryTermDef));
                    }
#line 2396 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 305: // theory_definition_list: theory_definition_nlist
#line 944 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryDefs) = (yystack_[0].value.theoryDefs);
                    }
#line 2402 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 306: // theory_definition_list: %empty
#line 945 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        (yylhs.value.theoryDefs) = BUILDER.theorydefs();
                    }
#line 2408 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 307: // statement: "#theory" identifier enable_theory_definition_lexing "{"
                              // theory_definition_list "}" disable_theory_lexing "."
#line 949 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        BUILDER.theorydef(yylhs.location, String::fromRep((yystack_[6].value.str)),
                                          (yystack_[3].value.theoryDefs), LOGGER);
                    }
#line 2414 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 308: // enable_theory_lexing: %empty
#line 955 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->theoryLexing(TheoryLexing::Theory);
                    }
#line 2420 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 309: // enable_theory_definition_lexing: %empty
#line 959 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->theoryLexing(TheoryLexing::Definition);
                    }
#line 2426 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

                    case 310: // disable_theory_lexing: %empty
#line 963 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
                    {
                        lexer->theoryLexing(TheoryLexing::Disabled);
                    }
#line 2432 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
                    break;

#line 2436 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"

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

const short parser::yypact_ninf_ = -487;

const short parser::yytable_ninf_ = -311;

const short parser::yypact_[] = {
    173,  236,  -487, 71,   -487, -487, -487, 84,   18,   668,  -487, 852,  -487, -487, 236,  236,  236,  1487, 236,
    -487, 210,  -487, 57,   252,  -487, 252,  100,  15,   -487, 1087, 1296, 89,   -487, 135,  -487, 272,  1325, 348,
    1487, -487, -487, -487, -487, 236,  -487, 1487, 113,  -487, -487, 195,  -487, -487, 1212, -487, 433,  1556, -487,
    91,   192,  952,  -487, 1241, -487, 188,  -487, 236,  852,  -487, 327,  852,  -487, 852,  -487, -487, 197,  1850,
    40,   200,  194,  202,  144,  1487, 206,  -487, 227,  236,  207,  1487, 236,  216,  58,   203,  -487, 750,  -487,
    236,  235,  -487, 1648, 256,  97,   -487, 1826, 258,  233,  1296, 228,  1354, 1383, 236,  28,   203,  -487, 254,
    236,  238,  460,  -487, -487, 1826, 94,   259,  265,  240,  -487, -487, 1270, 1648, -487, 1487, 1487, 1487, 1487,
    -487, -487, -487, -487, -487, 1487, 1487, -487, 1487, 1487, 1487, 1487, 1487, 995,  622,  952,  1116, -487, -487,
    -487, -487, 1412, 1826, 1487, -487, 98,   -487, 303,  290,  -487, 278,  -487, 1850, 46,   -487, 208,  852,  852,
    852,  852,  852,  852,  852,  852,  852,  852,  -487, -487, 1487, 302,  1487, -487, 236,  1487, 852,  292,  276,
    1651, 209,  315,  1487, 346,  -487, 349,  -487, 356,  1154, 826,  1602, 69,   359,  952,  190,  44,   -487, 370,
    -487, 1487, 1241, -487, -487, 1241, 1487, -487, 369,  -487, 383,  367,  420,  106,  430,  420,  232,  65,   390,
    -487, -487, -487, 402,  392,  1487, 435,  1487, -487, 1487, 1487, 412,  -487, -487, 1648, -487, 622,  458,  -487,
    253,  319,  297,  1872, 437,  437,  437,  872,  437,  319,  1875, 1826, 952,  1487, -487, -487, -487, 41,   -487,
    -487, 473,  239,  1826, 1183, -487, -487, -487, -487, -487, 852,  -487, 788,  -487, -487, 474,  439,  388,  607,
    447,  447,  447,  1096, 447,  388,  1896, 283,  969,  306,  -487, 483,  446,  325,  520,  429,  485,  1487, 203,
    1487, 1487, 358,  -487, -487, 469,  -487, -487, 1487, -487, 256,  -487, 320,  885,  1602, 226,  1049, 952,  1241,
    -487, -487, -487, 707,  -487, -487, -487, -487, -487, -487, 487,  493,  -487, 256,  1826, -487, -487, 1487, 1487,
    495,  490,  1487, -487, 495,  492,  1487, 442,  498,  -487, 450,  499,  379,  524,  1826, 420,  420,  449,  622,
    1487, 922,  1487, -487, 1826, 1241, -487, 1241, -487, 1487, -487, 41,   479,  -487, 1850, 852,  -487, -487, -487,
    1949, 1949, 1895, -487, -487, -487, -487, -487, -487, 501,  -487, 1949, -487, 361,  521,  -487, 480,  -487, 525,
    -487, 236,  522,  -487, -487, 527,  -487, 1826, -487, 1682, 417,  -487, 510,  511,  1487, 588,  -487, -487, 1241,
    1602, 234,  -487, -487, -487, 952,  -487, -487, 1241, -487, 462,  -487, 336,  -487, -487, 1826, 458,  1241, -487,
    -487, 420,  -487, -487, 420,  529,  -487, 533,  -487, 1065, 561,  -487, -487, -487, -487, -487, -487, -487, -487,
    -487, -487, -487, -487, -487, 372,  503,  504,  537,  -487, -487, 256,  544,  -487, -487, 521,  514,  509,  -487,
    47,   1949, -487, -487, 1949, 1949, 256,  507,  515,  1241, -487, -487, 542,  -487, 1487, -487, 1487, 1487, 1462,
    1487, 1487, -487, -487, -487, -487, -487, -487, -487, -487, 1463, -487, 566,  495,  495,  -487, -487, 526,  549,
    -487, 524,  -487, -487, -487, -487, 1241, -487, -487, 1926, -487, 531,  -487, 361,  -487, 1949, 361,  -487, 410,
    1826, 1706, 1730, -487, 1754, 1778, -487, 1241, -487, -487, 516,  361,  563,  -487, -487, 256,  -487, 54,   -487,
    -487, 1949, -487, 534,  538,  -487, -487, 1487, -487, -487, 579,  -487, -487, 480,  -487, -487, -487, -487, 361,
    -487, -487, 1802, 561,  585,  545,  550,  -487, -487, 587,  532,  361,  -487, 131,  592,  -487, -487, -487, -487,
    -487, -487, 577,  49,   361,  596,  -487, -487, 597,  -487, 112,  361,  565,  -487, -487, -487, 604,  561,  605,
    131,  -487};

const short parser::yydefact_[] = {
    0,   0,   5,   0,   10,  11,  12,  0,   0,   0,   1,   0,   3,   310, 0,   0,   0,   0,   0,   117, 0,   7,   0,
    0,   98,  0,   170, 0,   61,  0,   74,  0,   116, 0,   115, 0,   0,   0,   0,   114, 113, 62,  6,   0,   95,  0,
    170, 59,  64,  0,   60,  63,  0,   4,   57,  0,   101, 179, 0,   131, 181, 0,   180, 0,   147, 0,   0,   35,  0,
    0,   36,  0,   33,  34,  31,  219, 0,   269, 271, 58,  0,   0,   57,  52,  0,   0,   0,   0,   0,   89,  0,   0,
    186, 0,   185, 0,   0,   142, 0,   112, 0,   73,  67,  72,  77,  74,  0,   0,   0,   0,   89,  0,   208, 0,   0,
    0,   57,  51,  309, 65,  0,   0,   0,   0,   99,  96,  0,   0,   102, 70,  0,   0,   0,   87,  85,  83,  86,  84,
    0,   0,   88,  0,   0,   0,   0,   0,   0,   104, 131, 110, 155, 149, 150, 156, 70,  129, 0,   146, 112, 182, 170,
    32,  23,  0,   24,  37,  0,   22,  0,   40,  0,   0,   0,   0,   0,   0,   0,   0,   0,   8,   9,   70,  0,   70,
    232, 229, 70,  0,   0,   0,   0,   0,   91,  70,  170, 220, 170, 176, 0,   0,   0,   0,   0,   0,   131, 0,   0,
    135, 0,   225, 0,   110, 138, 143, 0,   71,  75,  78,  53,  0,   190, 188, 0,   0,   188, 0,   91,  0,   218, 170,
    210, 0,   0,   70,  0,   0,   56,  0,   0,   0,   100, 97,  0,   103, 105, 69,  79,  0,   45,  44,  41,  49,  47,
    50,  43,  48,  46,  42,  93,  131, 0,   144, 158, 107, 109, 160, 140, 0,   0,   130, 110, 151, 159, 152, 183, 184,
    40,  25,  0,   26,  30,  39,  0,   16,  15,  20,  18,  21,  14,  19,  17,  13,  0,   268, 0,   228, 230, 0,   0,
    0,   0,   0,   0,   0,   0,   70,  0,   220, 220, 235, 177, 178, 0,   125, 112, 123, 0,   0,   0,   0,   0,   131,
    110, 161, 171, 162, 0,   134, 163, 172, 164, 175, 169, 0,   109, 111, 112, 68,  76,  203, 0,   0,   195, 0,   0,
    202, 195, 0,   0,   0,   0,   209, 0,   0,   0,   306, 66,  188, 188, 0,   106, 0,   54,  70,  145, 94,  0,   157,
    0,   141, 70,  153, 109, 0,   27,  38,  0,   28,  270, 253, 262, 262, 0,   254, 251, 252, 255, 239, 240, 250, 242,
    0,   258, 260, 310, 266, 267, 308, 0,   55,  0,   0,   54,  220, 0,   211, 81,  213, 0,   0,   90,  234, 233, 0,
    0,   122, 126, 0,   0,   0,   165, 173, 166, 131, 132, 148, 110, 127, 112, 120, 0,   226, 139, 189, 187, 194, 198,
    205, 188, 200, 204, 188, 0,   216, 0,   206, 54,  0,   292, 289, 291, 293, 290, 286, 287, 288, 285, 308, 304, 303,
    305, 0,   0,   0,   0,   80,  108, 112, 0,   154, 29,  261, 0,   0,   245, 0,   262, 241, 257, 0,   0,   112, 0,
    0,   110, 227, 231, 221, 212, 0,   92,  0,   0,   0,   0,   0,   124, 167, 174, 168, 133, 118, 119, 128, 0,   191,
    193, 195, 195, 217, 207, 0,   0,   310, 0,   197, 196, 224, 137, 0,   243, 244, 0,   246, 0,   256, 259, 263, 310,
    310, 264, 0,   82,  0,   0,   238, 0,   0,   121, 0,   199, 201, 0,   284, 0,   302, 301, 112, 247, 0,   249, 265,
    0,   272, 0,   0,   237, 236, 0,   215, 192, 0,   309, 281, 283, 309, 307, 136, 248, 310, 222, 223, 0,   0,   0,
    0,   0,   273, 214, 0,   0,   0,   294, 308, 0,   282, 297, 296, 298, 295, 300, 0,   0,   277, 0,   278, 274, 276,
    309, 0,   0,   0,   279, 280, 275, 0,   0,   0,   0,   299};

const short parser::yypgoto_[] = {-487, -487, -487, -487, -1,   19,   551,  342,  310,  -487, -28,  -145, 518,  -487,
                                  -487, -138, -487, -16,  0,    -115, 45,   -117, -143, -155, 17,   116,  -487, 204,
                                  -487, -181, -141, -189, -487, -487, -6,   -487, -487, -142, -487, -487, -487, 27,
                                  -84,  -487, -206, -103, -487, -325, -487, -487, -487, -261, -487, -487, -275, -360,
                                  -347, -351, -289, -343, 96,   -487, -487, -487, 616,  -487, -487, 53,   -487, -487,
                                  -418, 123,  30,   127,  -487, -487, -356, -486, -12};

const short parser::yydefgoto_[] = {0,   3,   9,   53,  82,  165, 281, 282, 98,  120, 245, 246, 104, 105, 106, 247,
                                    191, 156, 56,  147, 263, 334, 335, 212, 203, 429, 430, 315, 316, 204, 157, 205,
                                    268, 100, 59,  60,  206, 153, 61,  62,  93,  94,  198, 63,  342, 221, 506, 436,
                                    222, 225, 8,   309, 296, 297, 390, 391, 392, 393, 471, 472, 395, 396, 397, 78,
                                    207, 597, 598, 563, 564, 565, 457, 458, 590, 459, 460, 461, 182, 234, 398};

const short parser::yytable_[] = {
    7,   76,  103, 272, 394,  224, 265, 261, 54,  267, 74,  319, 244,  77,  79,  80,  273, 84,  346, 86,  321, 439,
    89,  90,  89,  91,  58,   228, 54,  511, 75,  475, 264, 479, 110,  111, 115, 116, 473, 146, 482, 483, 118, 292,
    478, 294, 411, 412, 298,  148, 95,  54,  128, 366, 57,  306, 594,  179, 278, 522, 54,  331, 193, 327, 161, 74,
    480, 74,  74,  194, 74,   10,  58,  122, 99,  195, 574, 103, 12,   576, 322, 323, 210, 227, 188, 162, 324, 192,
    167, 96,  168, 87,  54,   151, 208, 354, 332, 279, 523, 305, 180,  512, 149, 150, 11,  568, 158, 152, 226, 270,
    271, 210, 601, 231, 595,  479, 477, 92,  364, 107, 349, 325, 367,  387, 388, 54,  243, 360, 419, 526, 121, 260,
    525, 524, 357, 358, 586,  321, 202, 423, 259, 587, 487, 151, 602,  213, 235, 588, 54,  214, 151, 462, 463, 372,
    343, 579, 589, 152, 344,  415, 236, 184, 603, 58,  152, 108, 479,  409, 74,  74,  74,  74,  74,  74,  74,  74,
    74,  74,  185, 425, 424,  432, 540, 541, 295, 320, 74,  275, 607,  283, 284, 285, 286, 287, 288, 289, 290, 291,
    54,  54,  128, 569, 328,  244, 477, 159, 299, 329, 575, 479, 54,   170, 171, 54,  196, 160, 1,   2,   465, 407,
    197, 308, 154, 311, -308, 468, 210, 591, 260, 123, 469, 169, 507,  549, 181, 508, 183, 321, 420, 394, 186, 438,
    330, 421, 314, 441, 497,  187, 172, 173, 193, 498, 209, 174, 552,  175, 351, 130, 131, 336, 303, 304, 189, 176,
    177, 229, 85,  211, 562,  54,  215, 230, 132, 502, 280, 74,  178,  74,  422, 218, 347, 4,   500, 501, 348, 216,
    499, 369, 5,   6,   237,  370, 389, 232, 138, 139, 238, 375, 428,  141, 130, 142, 320, 360, 362, 363, 562, 4,
    88,  143, 144, 239, 518,  434, 5,   6,   54,  243, 596, 55,  274,  54,  145, 4,   276, 604, 527, 83,  109, 277,
    5,   6,   293, 65,  378,  363, 66,  138, 139, 163, 102, 530, 141,  4,   260, 301, 113, 300, 117, 305, 5,   6,
    143, 144, 456, 119, 67,   399, 363, 138, 139, 68,  127, 307, 141,  54,  310, 54,  416, 155, 130, 131, 417, 340,
    143, 74,  402, 363, 164,  389, 389, 389, 481, 69,  503, 132, 70,   312, 504, 326, 389, 117, 567, 71,  333, 375,
    72,  190, 4,   485, 339,  73,  320, 201, 114, 5,   6,   138, 139,  410, 363, 466, 141, 467, 142, 102, 54,  220,
    220, 4,   513, 338, 143,  144, 514, 54,  5,   6,   172, 173, 446,  363, 341, 174, 54,  145, 242, 387, 388, 102,
    248, 249, 250, 176, -89,  -89, 456, 345, 251, 252, -89, 253, 254,  255, 256, 257, 258, 352, 155, 350, -89, 353,
    314, 359, 102, 355, 269,  129, 490, 363, 361, -91, -91, 211, 361,  389, 428, -91, 389, 389, 505, -89, 141, 54,
    368, -89, 376, -91, 554,  555, 377, 102, 174, 102, 233, 400, 102,  401, 404, -89, 413, 544, 405, 102, 431, 366,
    435, 437, -91, 440, 318,  442, -91, 456, 155, 443, 445, 553, 54,   444, 258, 389, 464, 170, 171, 337, -91, 389,
    447, 448, 470, 449, -308, 480, 450, 476, 484, 403, 54,  486, 451,  491, 492, 102, 488, 356, 509, 220, 220, 452,
    510, 389, 515, 516, 517,  519, 453, 577, 521, 528, 172, 173, 520,  529, 547, 174, 448, 175, 449, 155, 365, 450,
    456, 531, 454, 176, 177,  451, 539, 543, 566, 542, 550, 570, 560,  561, 452, 571, 178, 455, 573, 130, 131, 453,
    494, 4,   580, 581, 582,  583, 495, 584, 5,   6,   592, 456, 132,  593, 599, 600, 170, 454, 406, 605, 408, 102,
    606, 608, 373, 166, 538,  496, 414, 217, 551, 64,  455, 418, 138,  139, 258, 155, 4,   141, 585, 142, 102, 545,
    609, 5,   6,   546, 133,  143, 144, 134, 135, 172, 173, 0,   433,  102, 174, 0,   220, 136, 145, 137, 220, 0,
    0,   0,   176, 177, 140,  0,   0,   0,   -2,  13,  0,   337, 14,   102, 15,  0,   16,  17,  0,   0,   102, 18,
    19,  20,  0,   21,  0,    22,  0,   23,  24,  0,   0,   0,   25,   26,  27,  28,  29,  0,   0,   0,   30,  0,
    31,  32,  33,  34,  0,    0,   0,   0,   0,   15,  35,  0,   17,   0,   426, 0,   0,   36,  37,  493, 38,  39,
    40,  41,  42,  43,  44,   0,   0,   155, 45,  46,  28,  47,  48,   4,   49,  30,  50,  51,  0,   52,  5,   6,
    0,   0,   0,   0,   0,    0,   14,  427, 15,  0,   0,   17,  0,    0,   0,   81,  19,  0,   41,  0,   0,   0,
    0,   0,   24,  45,  0,    0,   47,  48,  4,   28,  199, 50,  51,   0,   30,  5,   6,   32,  0,   34,  0,   0,
    0,   0,   65,  0,   0,    66,  0,   532, 0,   533, 534, 0,   536,  537, 38,  39,  40,  41,  0,   0,   44,  0,
    102, 0,   45,  67,  0,    47,  48,  4,   68,  0,   50,  51,  0,    200, 5,   6,   14,  0,   15,  0,   0,   17,
    0,   0,   0,   374, 19,   0,   0,   0,   69,  0,   0,   70,  124,  0,   0,   0,   0,   0,   71,  28,  199, 72,
    65,  4,   30,  66,  73,   32,  0,   34,  5,   6,   572, 0,   0,    0,   0,   0,   0,   130, 131, 0,   0,   0,
    0,   67,  38,  39,  40,   41,  68,  0,   125, 14,  0,   15,  45,   0,   17,  47,  48,  4,   0,   19,  50,  51,
    0,   317, 5,   6,   0,    240, 69,  0,   0,   70,  138, 139, 28,   199, 0,   141, 71,  30,  0,   72,  32,  4,
    34,  0,   73,  143, 144,  0,   5,   6,   0,   -90, -90, 0,   0,    0,   0,   -90, 0,   38,  39,  40,  41,  0,
    0,   241, 0,   -90, 0,    45,  0,   0,   47,  48,  4,   0,   15,   50,  51,  17,  0,   5,   6,   0,   0,   0,
    0,   0,   -90, 0,   133,  0,   -90, 134, 135, 0,   0,   0,   -310, 28,  0,   0,   0,   136, 30,  137, -90, 0,
    0,   0,   0,   0,   140,  0,   0,   0,   379, 380, 381, 15,  0,    382, 17,  0,   0,   0,   81,  19,  0,   41,
    0,   0,   0,   0,   0,    0,   45,  0,   0,   47,  48,  4,   28,   29,  50,  51,  383, 30,  5,   6,   32,  0,
    34,  0,   0,   0,   384,  0,   4,   0,   0,   385, 386, 387, 388,  5,   6,   0,   0,   81,  39,  40,  41,  15,
    0,   0,   17,  0,   0,    45,  0,   19,  47,  48,  4,   0,   0,    50,  51,  0,   0,   5,   6,   0,   -92, -92,
    28,  199, 0,   0,   -92,  30,  0,   0,   32,  0,   34,  0,   0,    0,   -92, 15,  0,   0,   17,  0,   0,   170,
    171, 0,   0,   0,   0,    81,  39,  40,  41,  24,  0,   0,   0,    -92, 0,   45,  28,  -92, 47,  48,  4,   30,
    15,  50,  51,  17,  0,    5,   6,   0,   0,   -92, 0,   0,   0,    97,  172, 173, 24,  0,   0,   174, 0,   38,
    0,   28,  41,  0,   0,    44,  30,  176, 177, 45,  0,   0,   47,   48,  4,   0,   15,  50,  51,  17,  52,  5,
    6,   0,   262, 0,   0,    0,   38,  0,   0,   41,  24,  0,   44,   0,   0,   0,   45,  28,  0,   47,  48,  4,
    30,  15,  50,  51,  17,   52,  5,   6,   0,   0,   0,   0,   0,    0,   313, 0,   0,   24,  0,   0,   0,   0,
    38,  0,   28,  41,  0,    0,   44,  30,  15,  0,   45,  17,  0,    47,  48,  4,   0,   0,   50,  51,  0,   52,
    5,   6,   124, 371, 0,    0,   0,   38,  0,   28,  41,  0,   0,    44,  30,  15,  0,   45,  17,  0,   47,  48,
    4,   0,   0,   50,  51,   0,   52,  5,   6,   24,  0,   0,   0,    0,   38,  0,   28,  41,  0,   0,   125, 30,
    15,  0,   45,  17,  0,    47,  48,  4,   0,   0,   50,  51,  0,    126, 5,   6,   240, 0,   0,   0,   0,   38,
    0,   28,  41,  0,   15,   44,  30,  17,  0,   45,  101, 0,   47,   48,  4,   0,   0,   50,  51,  0,   52,  5,
    6,   0,   0,   0,   0,    28,  38,  0,   0,   41,  30,  15,  241,  0,   17,  0,   45,  0,   0,   47,  48,  4,
    112, 0,   50,  51,  0,    0,   5,   6,   0,   0,   81,  0,   28,   41,  0,   0,   0,   30,  15,  0,   45,  17,
    0,   47,  48,  4,   0,    0,   50,  51,  0,   0,   5,   6,   0,    0,   0,   0,   0,   81,  0,   28,  41,  0,
    0,   0,   30,  15,  0,    45,  17,  0,   47,  48,  4,   0,   0,    50,  51,  0,   219, 5,   6,   0,   0,   0,
    0,   0,   81,  0,   28,   41,  0,   0,   0,   30,  15,  0,   45,   17,  0,   47,  48,  4,   0,   0,   50,  51,
    0,   223, 5,   6,   0,    0,   0,   0,   0,   81,  0,   28,  41,   0,   0,   0,   30,  0,   0,   45,  0,   0,
    47,  48,  4,   0,   0,    50,  51,  0,   266, 5,   6,   0,   0,    130, 131, 0,   81,  15,  0,   41,  17,  0,
    426, 0,   0,   0,   45,   0,   132, 47,  48,  4,   0,   0,   50,   51,  0,   0,   5,   6,   28,  15,  0,   0,
    17,  30,  0,   0,   0,    0,   138, 139, 0,   0,   0,   141, 0,    142, 0,   535, 0,   0,   0,   0,   28,  143,
    144, 81,  0,   30,  41,   0,   0,   0,   0,   0,   0,   45,  145,  0,   47,  48,  4,   0,   0,   50,  51,  0,
    0,   5,   6,   81,  0,    0,   41,  0,   0,   0,   0,   0,   0,    45,  0,   0,   47,  48,  4,   130, 131, 50,
    51,  0,   0,   5,   6,    0,   0,   0,   19,  0,   0,   0,   132,  0,   133, 0,   0,   134, 135, 0,   0,   0,
    0,   0,   29,  0,   0,    136, 0,   137, 0,   32,  0,   34,  138,  139, 140, 0,   0,   141, 0,   142, 0,   130,
    131, 0,   0,   0,   0,    143, 144, 39,  40,  0,   19,  0,   0,    0,   132, 0,   133, 0,   145, 134, 135, 0,
    0,   0,   0,   0,   199,  0,   0,   136, 0,   137, 0,   32,  0,    34,  138, 139, 140, 0,   0,   141, 0,   142,
    0,   130, 131, 0,   130,  131, 0,   143, 144, 39,  40,  0,   0,    302, 0,   0,   132, 0,   133, 132, 145, 134,
    135, 0,   0,   0,   0,    0,   0,   0,   0,   136, 0,   137, 0,    130, 131, 0,   138, 139, 140, 138, 139, 141,
    489, 142, 141, 0,   142,  0,   132, 0,   0,   143, 144, 0,   143,  144, 0,   130, 131, 0,   0,   0,   0,   0,
    145, 0,   0,   145, 0,    0,   138, 139, 132, 0,   0,   141, 0,    142, 0,   0,   0,   130, 131, 0,   0,   143,
    144, 0,   0,   0,   0,    0,   0,   0,   138, 139, 132, 0,   145,  141, 0,   142, 0,   556, 0,   130, 131, 0,
    0,   143, 144, 0,   0,    0,   558, 0,   0,   0,   138, 139, 132,  0,   145, 141, 0,   142, 0,   557, 0,   130,
    131, 0,   0,   143, 144,  0,   0,   0,   0,   0,   0,   0,   138,  139, 132, 0,   145, 141, 0,   142, 0,   0,
    0,   130, 131, 0,   0,    143, 144, 0,   0,   0,   0,   0,   0,    0,   138, 139, 132, 0,   145, 141, 0,   142,
    0,   559, 0,   130, 131,  0,   0,   143, 144, 0,   0,   0,   0,    0,   0,   0,   138, 139, 132, 0,   145, 141,
    0,   142, 0,   578, 0,    170, 171, 0,   0,   143, 144, 0,   0,    0,   0,   0,   0,   0,   138, 139, 0,   0,
    145, 141, 0,   142, 0,    130, 131, 0,   130, 131, 0,   143, 144,  0,   0,   0,   0,   0,   0,   0,   172, 173,
    0,   0,   145, 174, 0,    175, 0,   170, 171, 0,   0,   0,   0,    176, 177, 0,   0,   0,   0,   0,   138, 139,
    0,   138, 139, 141, 178,  142, 141, 0,   142, 0,   379, 380, 381,  143, 144, 382, 143, 144, 0,   0,   0,   0,
    172, 173, 0,   0,   145,  174, 0,   175, 0,   0,   474, 0,   0,    0,   0,   176, 177, 0,   383, 379, 380, 381,
    0,   0,   382, 0,   0,    0,   384, 0,   4,   0,   0,   385, 386,  387, 388, 5,   6,   0,   0,   548, 379, 380,
    381, 0,   0,   382, 0,    383, 0,   0,   0,   0,   0,   0,   0,    0,   0,   384, 0,   4,   0,   0,   385, 386,
    387, 388, 5,   6,   0,    0,   383, 0,   0,   0,   0,   0,   0,    0,   0,   0,   384, 0,   4,   0,   0,   385,
    386, 387, 388, 5,   6};

const short parser::yycheck_[] = {
    1,   13,  30,  158, 293, 108, 149, 148, 9,   154, 11,  200, 127, 14,  15,  16,  158, 18,  224, 20,  201, 346, 23,
    23,  25,  25,  9,   111, 29,  447, 11,  382, 149, 393, 35,  35,  37,  38,  381, 55,  396, 397, 43,  181, 391, 183,
    307, 308, 186, 55,  35,  52,  52,  12,  9,   193, 7,   17,  12,  12,  61,  17,  34,  204, 65,  66,  12,  68,  69,
    11,  71,  0,   55,  46,  29,  17,  562, 105, 60,  565, 11,  12,  98,  55,  85,  66,  17,  88,  69,  74,  71,  34,
    93,  52,  95,  233, 52,  51,  51,  34,  60,  457, 11,  12,  20,  51,  61,  66,  109, 11,  12,  127, 598, 114, 65,
    475, 391, 17,  259, 30,  55,  52,  264, 76,  77,  126, 126, 242, 317, 480, 17,  147, 479, 476, 237, 238, 5,   318,
    93,  320, 146, 10,  403, 52,  32,  48,  52,  16,  149, 52,  52,  357, 358, 270, 48,  573, 25,  66,  52,  314, 66,
    17,  50,  146, 66,  30,  526, 305, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 34,  322, 321, 336, 507, 508,
    185, 201, 187, 160, 606, 170, 171, 172, 173, 174, 175, 176, 177, 178, 199, 200, 200, 552, 12,  318, 479, 17,  187,
    17,  564, 569, 211, 3,   4,   214, 11,  27,  43,  44,  363, 303, 17,  194, 30,  196, 30,  370, 242, 583, 244, 34,
    372, 34,  438, 522, 34,  441, 34,  418, 12,  528, 34,  344, 52,  17,  199, 348, 12,  20,  40,  41,  34,  17,  17,
    45,  529, 47,  229, 3,   4,   214, 51,  52,  55,  55,  56,  11,  56,  11,  543, 270, 12,  17,  18,  428, 66,  276,
    68,  278, 52,  51,  48,  71,  423, 426, 52,  52,  52,  48,  78,  79,  31,  52,  293, 55,  40,  41,  31,  278, 326,
    45,  3,   47,  318, 418, 51,  52,  581, 71,  56,  55,  56,  71,  467, 341, 78,  79,  317, 317, 593, 9,   17,  322,
    68,  71,  34,  600, 481, 17,  56,  51,  78,  79,  30,  6,   51,  52,  9,   40,  41,  12,  30,  484, 45,  71,  360,
    69,  36,  55,  38,  34,  78,  79,  55,  56,  355, 45,  29,  51,  52,  40,  41,  34,  52,  17,  45,  366, 17,  368,
    48,  59,  3,   4,   52,  6,   55,  376, 51,  52,  51,  380, 381, 382, 394, 56,  48,  18,  59,  31,  52,  30,  391,
    81,  547, 66,  24,  376, 69,  87,  71,  400, 17,  74,  418, 93,  56,  78,  79,  40,  41,  51,  52,  366, 45,  368,
    47,  105, 417, 107, 108, 71,  48,  52,  55,  56,  52,  426, 78,  79,  40,  41,  51,  52,  12,  45,  435, 68,  126,
    76,  77,  129, 130, 131, 132, 55,  11,  12,  447, 17,  138, 139, 17,  141, 142, 143, 144, 145, 146, 55,  148, 69,
    27,  69,  417, 51,  154, 30,  156, 34,  51,  52,  12,  11,  12,  11,  12,  476, 504, 17,  479, 480, 435, 48,  45,
    484, 11,  52,  12,  27,  78,  79,  51,  181, 45,  183, 34,  12,  186, 51,  69,  66,  31,  513, 17,  193, 17,  12,
    11,  17,  48,  17,  200, 69,  52,  514, 204, 17,  17,  529, 519, 69,  210, 522, 73,  3,   4,   215, 66,  528, 4,
    5,   51,  7,   52,  12,  10,  34,  11,  17,  539, 17,  16,  31,  31,  233, 17,  235, 17,  237, 238, 25,  17,  552,
    49,  49,  17,  11,  32,  569, 49,  52,  40,  41,  48,  48,  519, 45,  5,   47,  7,   259, 260, 10,  573, 31,  50,
    55,  56,  16,  12,  30,  17,  55,  51,  49,  539, 69,  25,  49,  68,  65,  11,  3,   4,   32,  6,   71,  11,  52,
    48,  12,  12,  69,  78,  79,  12,  606, 18,  30,  12,  12,  3,   50,  302, 48,  304, 305, 12,  12,  276, 68,  504,
    417, 312, 105, 528, 9,   65,  317, 40,  41,  320, 321, 71,  45,  581, 47,  326, 514, 608, 78,  79,  514, 20,  55,
    56,  23,  24,  40,  41,  -1,  340, 341, 45,  -1,  344, 33,  68,  35,  348, -1,  -1,  -1,  55,  56,  42,  -1,  -1,
    -1,  0,   1,   -1,  361, 4,   363, 6,   -1,  8,   9,   -1,  -1,  370, 13,  14,  15,  -1,  17,  -1,  19,  -1,  21,
    22,  -1,  -1,  -1,  26,  27,  28,  29,  30,  -1,  -1,  -1,  34,  -1,  36,  37,  38,  39,  -1,  -1,  -1,  -1,  -1,
    6,   46,  -1,  9,   -1,  11,  -1,  -1,  53,  54,  413, 56,  57,  58,  59,  60,  61,  62,  -1,  -1,  423, 66,  67,
    29,  69,  70,  71,  72,  34,  74,  75,  -1,  77,  78,  79,  -1,  -1,  -1,  -1,  -1,  -1,  4,   48,  6,   -1,  -1,
    9,   -1,  -1,  -1,  56,  14,  -1,  59,  -1,  -1,  -1,  -1,  -1,  22,  66,  -1,  -1,  69,  70,  71,  29,  30,  74,
    75,  -1,  34,  78,  79,  37,  -1,  39,  -1,  -1,  -1,  -1,  6,   -1,  -1,  9,   -1,  489, -1,  491, 492, -1,  494,
    495, 56,  57,  58,  59,  -1,  -1,  62,  -1,  504, -1,  66,  29,  -1,  69,  70,  71,  34,  -1,  74,  75,  -1,  77,
    78,  79,  4,   -1,  6,   -1,  -1,  9,   -1,  -1,  -1,  51,  14,  -1,  -1,  -1,  56,  -1,  -1,  59,  22,  -1,  -1,
    -1,  -1,  -1,  66,  29,  30,  69,  6,   71,  34,  9,   74,  37,  -1,  39,  78,  79,  558, -1,  -1,  -1,  -1,  -1,
    -1,  3,   4,   -1,  -1,  -1,  -1,  29,  56,  57,  58,  59,  34,  -1,  62,  4,   -1,  6,   66,  -1,  9,   69,  70,
    71,  -1,  14,  74,  75,  -1,  77,  78,  79,  -1,  22,  56,  -1,  -1,  59,  40,  41,  29,  30,  -1,  45,  66,  34,
    -1,  69,  37,  71,  39,  -1,  74,  55,  56,  -1,  78,  79,  -1,  11,  12,  -1,  -1,  -1,  -1,  17,  -1,  56,  57,
    58,  59,  -1,  -1,  62,  -1,  27,  -1,  66,  -1,  -1,  69,  70,  71,  -1,  6,   74,  75,  9,   -1,  78,  79,  -1,
    -1,  -1,  -1,  -1,  48,  -1,  20,  -1,  52,  23,  24,  -1,  -1,  -1,  11,  29,  -1,  -1,  -1,  33,  34,  35,  66,
    -1,  -1,  -1,  -1,  -1,  42,  -1,  -1,  -1,  29,  30,  31,  6,   -1,  34,  9,   -1,  -1,  -1,  56,  14,  -1,  59,
    -1,  -1,  -1,  -1,  -1,  -1,  66,  -1,  -1,  69,  70,  71,  29,  30,  74,  75,  59,  34,  78,  79,  37,  -1,  39,
    -1,  -1,  -1,  69,  -1,  71,  -1,  -1,  74,  75,  76,  77,  78,  79,  -1,  -1,  56,  57,  58,  59,  6,   -1,  -1,
    9,   -1,  -1,  66,  -1,  14,  69,  70,  71,  -1,  -1,  74,  75,  -1,  -1,  78,  79,  -1,  11,  12,  29,  30,  -1,
    -1,  17,  34,  -1,  -1,  37,  -1,  39,  -1,  -1,  -1,  27,  6,   -1,  -1,  9,   -1,  -1,  3,   4,   -1,  -1,  -1,
    -1,  56,  57,  58,  59,  22,  -1,  -1,  -1,  48,  -1,  66,  29,  52,  69,  70,  71,  34,  6,   74,  75,  9,   -1,
    78,  79,  -1,  -1,  66,  -1,  -1,  -1,  48,  40,  41,  22,  -1,  -1,  45,  -1,  56,  -1,  29,  59,  -1,  -1,  62,
    34,  55,  56,  66,  -1,  -1,  69,  70,  71,  -1,  6,   74,  75,  9,   77,  78,  79,  -1,  52,  -1,  -1,  -1,  56,
    -1,  -1,  59,  22,  -1,  62,  -1,  -1,  -1,  66,  29,  -1,  69,  70,  71,  34,  6,   74,  75,  9,   77,  78,  79,
    -1,  -1,  -1,  -1,  -1,  -1,  48,  -1,  -1,  22,  -1,  -1,  -1,  -1,  56,  -1,  29,  59,  -1,  -1,  62,  34,  6,
    -1,  66,  9,   -1,  69,  70,  71,  -1,  -1,  74,  75,  -1,  77,  78,  79,  22,  52,  -1,  -1,  -1,  56,  -1,  29,
    59,  -1,  -1,  62,  34,  6,   -1,  66,  9,   -1,  69,  70,  71,  -1,  -1,  74,  75,  -1,  77,  78,  79,  22,  -1,
    -1,  -1,  -1,  56,  -1,  29,  59,  -1,  -1,  62,  34,  6,   -1,  66,  9,   -1,  69,  70,  71,  -1,  -1,  74,  75,
    -1,  77,  78,  79,  22,  -1,  -1,  -1,  -1,  56,  -1,  29,  59,  -1,  6,   62,  34,  9,   -1,  66,  12,  -1,  69,
    70,  71,  -1,  -1,  74,  75,  -1,  77,  78,  79,  -1,  -1,  -1,  -1,  29,  56,  -1,  -1,  59,  34,  6,   62,  -1,
    9,   -1,  66,  -1,  -1,  69,  70,  71,  17,  -1,  74,  75,  -1,  -1,  78,  79,  -1,  -1,  56,  -1,  29,  59,  -1,
    -1,  -1,  34,  6,   -1,  66,  9,   -1,  69,  70,  71,  -1,  -1,  74,  75,  -1,  -1,  78,  79,  -1,  -1,  -1,  -1,
    -1,  56,  -1,  29,  59,  -1,  -1,  -1,  34,  6,   -1,  66,  9,   -1,  69,  70,  71,  -1,  -1,  74,  75,  -1,  48,
    78,  79,  -1,  -1,  -1,  -1,  -1,  56,  -1,  29,  59,  -1,  -1,  -1,  34,  6,   -1,  66,  9,   -1,  69,  70,  71,
    -1,  -1,  74,  75,  -1,  48,  78,  79,  -1,  -1,  -1,  -1,  -1,  56,  -1,  29,  59,  -1,  -1,  -1,  34,  -1,  -1,
    66,  -1,  -1,  69,  70,  71,  -1,  -1,  74,  75,  -1,  48,  78,  79,  -1,  -1,  3,   4,   -1,  56,  6,   -1,  59,
    9,   -1,  11,  -1,  -1,  -1,  66,  -1,  18,  69,  70,  71,  -1,  -1,  74,  75,  -1,  -1,  78,  79,  29,  6,   -1,
    -1,  9,   34,  -1,  -1,  -1,  -1,  40,  41,  -1,  -1,  -1,  45,  -1,  47,  -1,  49,  -1,  -1,  -1,  -1,  29,  55,
    56,  56,  -1,  34,  59,  -1,  -1,  -1,  -1,  -1,  -1,  66,  68,  -1,  69,  70,  71,  -1,  -1,  74,  75,  -1,  -1,
    78,  79,  56,  -1,  -1,  59,  -1,  -1,  -1,  -1,  -1,  -1,  66,  -1,  -1,  69,  70,  71,  3,   4,   74,  75,  -1,
    -1,  78,  79,  -1,  -1,  -1,  14,  -1,  -1,  -1,  18,  -1,  20,  -1,  -1,  23,  24,  -1,  -1,  -1,  -1,  -1,  30,
    -1,  -1,  33,  -1,  35,  -1,  37,  -1,  39,  40,  41,  42,  -1,  -1,  45,  -1,  47,  -1,  3,   4,   -1,  -1,  -1,
    -1,  55,  56,  57,  58,  -1,  14,  -1,  -1,  -1,  18,  -1,  20,  -1,  68,  23,  24,  -1,  -1,  -1,  -1,  -1,  30,
    -1,  -1,  33,  -1,  35,  -1,  37,  -1,  39,  40,  41,  42,  -1,  -1,  45,  -1,  47,  -1,  3,   4,   -1,  3,   4,
    -1,  55,  56,  57,  58,  -1,  -1,  12,  -1,  -1,  18,  -1,  20,  18,  68,  23,  24,  -1,  -1,  -1,  -1,  -1,  -1,
    -1,  -1,  33,  -1,  35,  -1,  3,   4,   -1,  40,  41,  42,  40,  41,  45,  12,  47,  45,  -1,  47,  -1,  18,  -1,
    -1,  55,  56,  -1,  55,  56,  -1,  3,   4,   -1,  -1,  -1,  -1,  -1,  68,  -1,  -1,  68,  -1,  -1,  40,  41,  18,
    -1,  -1,  45,  -1,  47,  -1,  -1,  -1,  3,   4,   -1,  -1,  55,  56,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  40,  41,
    18,  -1,  68,  45,  -1,  47,  -1,  49,  -1,  3,   4,   -1,  -1,  55,  56,  -1,  -1,  -1,  12,  -1,  -1,  -1,  40,
    41,  18,  -1,  68,  45,  -1,  47,  -1,  49,  -1,  3,   4,   -1,  -1,  55,  56,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
    40,  41,  18,  -1,  68,  45,  -1,  47,  -1,  -1,  -1,  3,   4,   -1,  -1,  55,  56,  -1,  -1,  -1,  -1,  -1,  -1,
    -1,  40,  41,  18,  -1,  68,  45,  -1,  47,  -1,  49,  -1,  3,   4,   -1,  -1,  55,  56,  -1,  -1,  -1,  -1,  -1,
    -1,  -1,  40,  41,  18,  -1,  68,  45,  -1,  47,  -1,  49,  -1,  3,   4,   -1,  -1,  55,  56,  -1,  -1,  -1,  -1,
    -1,  -1,  -1,  40,  41,  -1,  -1,  68,  45,  -1,  47,  -1,  3,   4,   -1,  3,   4,   -1,  55,  56,  -1,  -1,  -1,
    -1,  -1,  -1,  -1,  40,  41,  -1,  -1,  68,  45,  -1,  47,  -1,  3,   4,   -1,  -1,  -1,  -1,  55,  56,  -1,  -1,
    -1,  -1,  -1,  40,  41,  -1,  40,  41,  45,  68,  47,  45,  -1,  47,  -1,  29,  30,  31,  55,  56,  34,  55,  56,
    -1,  -1,  -1,  -1,  40,  41,  -1,  -1,  68,  45,  -1,  47,  -1,  -1,  51,  -1,  -1,  -1,  -1,  55,  56,  -1,  59,
    29,  30,  31,  -1,  -1,  34,  -1,  -1,  -1,  69,  -1,  71,  -1,  -1,  74,  75,  76,  77,  78,  79,  -1,  -1,  51,
    29,  30,  31,  -1,  -1,  34,  -1,  59,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  69,  -1,  71,  -1,  -1,  74,
    75,  76,  77,  78,  79,  -1,  -1,  59,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  69,  -1,  71,  -1,  -1,  74,
    75,  76,  77,  78,  79};

const unsigned char parser::yystos_[] = {
    0,   43,  44,  81,  71,  78,  79,  84,  130, 82,  0,   20,  60,  1,   4,   6,   8,   9,   13,  14,  15,  17,  19,
    21,  22,  26,  27,  28,  29,  30,  34,  36,  37,  38,  39,  46,  53,  54,  56,  57,  58,  59,  60,  61,  62,  66,
    67,  69,  70,  72,  74,  75,  77,  83,  84,  88,  98,  100, 104, 114, 115, 118, 119, 123, 144, 6,   9,   29,  34,
    56,  59,  66,  69,  74,  84,  85,  158, 84,  143, 84,  84,  56,  84,  88,  84,  56,  84,  34,  56,  84,  98,  98,
    17,  120, 121, 35,  74,  48,  88,  100, 113, 12,  88,  90,  92,  93,  94,  30,  30,  56,  84,  98,  17,  88,  56,
    84,  84,  88,  84,  88,  89,  17,  121, 34,  22,  62,  77,  88,  98,  34,  3,   4,   18,  20,  23,  24,  33,  35,
    40,  41,  42,  45,  47,  55,  56,  68,  97,  99,  114, 11,  12,  52,  66,  117, 30,  88,  97,  110, 100, 17,  27,
    84,  85,  12,  51,  85,  86,  85,  85,  34,  3,   4,   40,  41,  45,  47,  55,  56,  68,  17,  60,  34,  156, 34,
    17,  34,  34,  20,  84,  55,  88,  96,  84,  34,  11,  17,  11,  17,  122, 30,  77,  88,  100, 104, 109, 111, 116,
    144, 84,  17,  97,  11,  103, 48,  52,  12,  52,  92,  51,  48,  88,  125, 128, 48,  125, 129, 84,  55,  122, 11,
    17,  84,  55,  34,  157, 52,  66,  31,  31,  71,  22,  62,  88,  98,  99,  90,  91,  95,  88,  88,  88,  88,  88,
    88,  88,  88,  88,  88,  88,  114, 97,  110, 52,  100, 101, 102, 48,  91,  112, 88,  11,  12,  103, 117, 17,  121,
    34,  51,  12,  51,  66,  86,  87,  85,  85,  85,  85,  85,  85,  85,  85,  85,  95,  30,  95,  84,  132, 133, 95,
    85,  55,  69,  12,  51,  52,  34,  95,  17,  121, 131, 17,  121, 31,  48,  100, 107, 108, 77,  88,  111, 97,  109,
    11,  12,  17,  52,  30,  110, 12,  17,  52,  17,  52,  24,  101, 102, 100, 88,  52,  17,  6,   12,  124, 48,  52,
    17,  124, 48,  52,  55,  69,  121, 55,  69,  95,  30,  88,  125, 125, 51,  99,  12,  51,  52,  110, 88,  12,  117,
    11,  48,  52,  52,  101, 87,  51,  85,  12,  51,  51,  29,  30,  31,  34,  59,  69,  74,  75,  76,  77,  84,  134,
    135, 136, 137, 138, 140, 141, 142, 158, 51,  12,  51,  51,  17,  69,  17,  88,  122, 88,  95,  51,  131, 131, 31,
    88,  103, 48,  52,  88,  111, 12,  17,  52,  109, 110, 102, 11,  48,  90,  105, 106, 17,  103, 88,  90,  11,  127,
    17,  125, 127, 17,  125, 69,  17,  69,  17,  51,  4,   5,   7,   10,  16,  25,  32,  50,  65,  84,  150, 151, 153,
    154, 155, 124, 124, 73,  91,  100, 100, 91,  117, 51,  138, 139, 139, 51,  137, 34,  134, 136, 135, 12,  158, 156,
    156, 11,  84,  17,  131, 17,  12,  51,  31,  31,  88,  6,   12,  107, 12,  17,  52,  110, 102, 103, 48,  52,  100,
    126, 124, 124, 17,  17,  150, 156, 48,  52,  49,  49,  17,  103, 11,  48,  49,  12,  51,  139, 136, 137, 103, 52,
    48,  102, 31,  88,  88,  88,  49,  88,  88,  105, 12,  127, 127, 55,  30,  158, 151, 153, 100, 51,  138, 51,  140,
    134, 158, 78,  79,  49,  49,  12,  49,  100, 69,  134, 147, 148, 149, 17,  103, 51,  137, 49,  49,  88,  11,  157,
    156, 157, 158, 49,  150, 11,  52,  48,  12,  69,  147, 5,   10,  16,  25,  152, 156, 12,  30,  7,   65,  134, 145,
    146, 12,  12,  157, 32,  50,  134, 48,  12,  150, 12,  152};

const unsigned char parser::yyr1_[] = {
    0,   80,  81,  81,  82,  82,  83,  83,  83,  83,  84,  84,  84,  85,  85,  85,  85,  85,  85,  85,  85,  85,  85,
    85,  85,  85,  85,  85,  85,  85,  85,  85,  85,  85,  85,  85,  85,  86,  86,  87,  87,  88,  88,  88,  88,  88,
    88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  88,  89,  89,  90,  90,
    91,  91,  92,  92,  92,  92,  93,  93,  94,  94,  95,  95,  96,  96,  97,  97,  97,  97,  97,  97,  98,  98,  98,
    98,  99,  99,  100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 101, 101, 102, 102, 103, 103, 104, 104,
    104, 104, 104, 105, 105, 106, 106, 107, 108, 108, 109, 109, 109, 109, 110, 110, 110, 111, 111, 111, 111, 112, 112,
    113, 113, 114, 114, 114, 114, 115, 115, 115, 115, 116, 117, 117, 118, 118, 118, 118, 118, 118, 118, 118, 119, 119,
    120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 121, 121, 121, 121, 121, 122, 122, 122, 123, 123, 123, 83,  83,
    83,  83,  83,  124, 124, 125, 125, 126, 126, 127, 127, 127, 83,  83,  128, 128, 129, 129, 83,  83,  83,  83,  83,
    83,  83,  83,  83,  83,  83,  83,  83,  83,  83,  83,  83,  130, 131, 83,  83,  83,  83,  83,  83,  132, 132, 133,
    133, 83,  83,  83,  83,  83,  83,  83,  83,  134, 134, 135, 135, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136,
    136, 136, 136, 137, 137, 137, 138, 138, 139, 139, 140, 140, 141, 141, 142, 142, 143, 143, 144, 144, 144, 145, 145,
    146, 146, 147, 147, 147, 148, 148, 149, 149, 150, 150, 150, 150, 150, 150, 150, 150, 150, 151, 152, 152, 152, 152,
    153, 153, 154, 154, 154, 154, 155, 155, 83,  156, 157, 158};

const signed char parser::yyr2_[] = {
    0, 2, 2, 3, 2,  0, 1, 1, 3, 3, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3,  3, 3, 2, 2, 2, 3, 3, 4, 4,  5, 3, 1, 2, 1, 1,
    1, 1, 1, 3, 1,  0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 3, 4,  5, 3, 1, 2, 1, 1, 1, 1, 1,  1, 1, 3, 1, 3, 1,
    0, 2, 1, 1, 0,  2, 3, 1, 2, 1, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1,  4, 2, 5, 2, 3, 1, 2, 3, 1,  2, 3, 1, 2, 3, 2,
    3, 4, 1, 3, 1,  0, 2, 0, 1, 1, 1, 1, 1, 2, 2, 1, 3, 2, 1, 3,  2, 3, 3, 4, 1, 2, 0, 3, 4,  2, 1, 6, 4, 2, 4,
    3, 4, 2, 3, 3,  4, 2, 1, 3, 1, 1, 3, 3, 4, 5, 2, 2, 4, 3, 3,  3, 3, 3, 3, 3, 4, 4, 5, 5,  3, 0, 3, 3, 4, 5,
    3, 1, 2, 2, 1,  1, 1, 2, 3, 3, 2, 2, 2, 0, 3, 1, 1, 3, 2, 1,  0, 6, 6, 3, 5, 3, 5, 4, 4,  5, 5, 5, 6, 2, 4,
    3, 5, 6, 5, 10, 8, 5, 6, 3, 3, 0, 6, 9, 9, 6, 3, 5, 3, 1, 0,  1, 6, 3, 5, 5, 4, 8, 8, 7,  1, 1, 2, 1, 3, 3,
    2, 3, 4, 5, 4,  1, 1, 1, 1, 1, 1, 3, 2, 1, 3, 1, 1, 0, 3, 3,  4, 1, 1, 0, 1, 4, 2, 8, 10, 1, 3, 1, 0, 6, 8,
    8, 1, 4, 1, 0,  1, 1, 1, 1, 1, 1, 1, 1, 1, 6, 1, 1, 1, 1, 16, 8, 3, 3, 1, 1, 1, 0, 8, 0,  0, 0};

#if YYDEBUG || 1
// YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
// First, the terminals, then, starting at \a YYNTOKENS, nonterminals.
const char *const parser::yytname_[] = {"\"<EOF>\"",
                                        "error",
                                        "\"invalid token\"",
                                        "\"+\"",
                                        "\"&\"",
                                        "\"any\"",
                                        "\"@\"",
                                        "\"binary\"",
                                        "\"#program\"",
                                        "\"~\"",
                                        "\"body\"",
                                        "\":\"",
                                        "\",\"",
                                        "\"#const\"",
                                        "\"#count\"",
                                        "\"#defined\"",
                                        "\"directive\"",
                                        "\".\"",
                                        "\"..\"",
                                        "\"#edge\"",
                                        "\"=\"",
                                        "\"#external\"",
                                        "\"#false\"",
                                        "\">=\"",
                                        "\">\"",
                                        "\"head\"",
                                        "\"#heuristic\"",
                                        "\":-\"",
                                        "\"#include\"",
                                        "\"#inf\"",
                                        "\"{\"",
                                        "\"[\"",
                                        "\"left\"",
                                        "\"<=\"",
                                        "\"(\"",
                                        "\"<\"",
                                        "\"#maximize\"",
                                        "\"#max\"",
                                        "\"#minimize\"",
                                        "\"#min\"",
                                        "\"\\\\\"",
                                        "\"*\"",
                                        "\"!=\"",
                                        "\"<define>\"",
                                        "\"<program>\"",
                                        "\"**\"",
                                        "\"#project\"",
                                        "\"?\"",
                                        "\"}\"",
                                        "\"]\"",
                                        "\"right\"",
                                        "\")\"",
                                        "\";\"",
                                        "\"#show\"",
                                        "\"#showsig\"",
                                        "\"/\"",
                                        "\"-\"",
                                        "\"#sum+\"",
                                        "\"#sum\"",
                                        "\"#sup\"",
                                        "\"EOF\"",
                                        "\"#theory\"",
                                        "\"#true\"",
                                        "UBNOT",
                                        "UMINUS",
                                        "\"unary\"",
                                        "\"|\"",
                                        "\":~\"",
                                        "\"^\"",
                                        "\"<NUMBER>\"",
                                        "\"<ANONYMOUS>\"",
                                        "\"<IDENTIFIER>\"",
                                        "\"<SCRIPT>\"",
                                        "\"<CODE>\"",
                                        "\"<STRING>\"",
                                        "\"<VARIABLE>\"",
                                        "\"<THEORYOP>\"",
                                        "\"not\"",
                                        "\"default\"",
                                        "\"override\"",
                                        "$accept",
                                        "start",
                                        "program",
                                        "statement",
                                        "identifier",
                                        "constterm",
                                        "consttermvec",
                                        "constargvec",
                                        "term",
                                        "unaryargvec",
                                        "ntermvec",
                                        "termvec",
                                        "tuple",
                                        "tuplevec_sem",
                                        "tuplevec",
                                        "argvec",
                                        "binaryargvec",
                                        "cmp",
                                        "atom",
                                        "rellitvec",
                                        "literal",
                                        "nlitvec",
                                        "litvec",
                                        "optcondition",
                                        "aggregatefunction",
                                        "bodyaggrelem",
                                        "bodyaggrelemvec",
                                        "altbodyaggrelem",
                                        "altbodyaggrelemvec",
                                        "bodyaggregate",
                                        "upper",
                                        "lubodyaggregate",
                                        "headaggrelemvec",
                                        "altheadaggrelemvec",
                                        "headaggregate",
                                        "luheadaggregate",
                                        "conjunction",
                                        "dsym",
                                        "disjunctionsep",
                                        "disjunction",
                                        "bodycomma",
                                        "bodydot",
                                        "bodyconddot",
                                        "head",
                                        "optimizetuple",
                                        "optimizeweight",
                                        "optimizelitvec",
                                        "optimizecond",
                                        "maxelemlist",
                                        "minelemlist",
                                        "define",
                                        "storecomment",
                                        "nidlist",
                                        "idlist",
                                        "theory_op",
                                        "theory_op_list",
                                        "theory_term",
                                        "theory_opterm",
                                        "theory_opterm_nlist",
                                        "theory_opterm_list",
                                        "theory_atom_element",
                                        "theory_atom_element_nlist",
                                        "theory_atom_element_list",
                                        "theory_atom_name",
                                        "theory_atom",
                                        "theory_operator_nlist",
                                        "theory_operator_list",
                                        "theory_operator_definition",
                                        "theory_operator_definition_nlist",
                                        "theory_operator_definiton_list",
                                        "theory_definition_identifier",
                                        "theory_term_definition",
                                        "theory_atom_type",
                                        "theory_atom_definition",
                                        "theory_definition_nlist",
                                        "theory_definition_list",
                                        "enable_theory_lexing",
                                        "enable_theory_definition_lexing",
                                        "disable_theory_lexing",
                                        YY_NULLPTR};
#endif

#if YYDEBUG
const short parser::yyrline_[] = {
    0,   314, 314, 315, 319, 320, 326, 327, 328, 329, 333, 334, 335, 342, 343, 344, 345, 346, 347, 348, 349, 350, 351,
    352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 363, 364, 365, 371, 372, 376, 377, 383, 384, 385, 386, 387,
    388, 389, 390, 391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401, 402, 403, 404, 405, 406, 412, 413, 419, 420,
    424, 425, 429, 430, 431, 432, 435, 436, 439, 440, 443, 444, 448, 449, 459, 460, 461, 462, 463, 464, 468, 469, 470,
    471, 475, 476, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 499, 500, 504, 505, 509, 510, 514, 515,
    516, 517, 518, 524, 525, 529, 530, 536, 540, 541, 547, 548, 549, 550, 554, 555, 556, 560, 561, 562, 563, 569, 570,
    574, 575, 581, 582, 583, 584, 588, 589, 590, 591, 598, 605, 606, 612, 613, 614, 615, 616, 617, 618, 619, 623, 624,
    631, 632, 633, 634, 635, 636, 637, 638, 639, 640, 644, 645, 646, 647, 648, 652, 653, 654, 657, 658, 659, 663, 664,
    665, 666, 667, 673, 674, 678, 679, 683, 684, 688, 689, 690, 694, 695, 699, 700, 704, 705, 709, 710, 711, 712, 718,
    719, 720, 721, 722, 728, 729, 734, 740, 741, 747, 748, 749, 755, 759, 762, 763, 764, 770, 776, 777, 783, 784, 788,
    789, 793, 794, 800, 801, 802, 803, 804, 805, 811, 812, 818, 819, 823, 824, 825, 826, 827, 828, 829, 830, 831, 832,
    833, 834, 835, 839, 840, 841, 845, 846, 850, 851, 855, 856, 860, 861, 865, 866, 870, 871, 874, 875, 876, 882, 883,
    887, 888, 892, 893, 894, 898, 899, 903, 904, 908, 909, 910, 911, 912, 913, 914, 915, 916, 920, 924, 925, 926, 927,
    931, 933, 937, 938, 939, 940, 944, 945, 949, 955, 959, 963};

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
        0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
        2,  2,  2,  2,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
        53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
    // Last valid token kind.
    const int code_max = 334;

    if (t <= 0)
        return symbol_kind::S_YYEOF;
    else if (t <= code_max)
        return static_cast<symbol_kind_type>(translate_table[t]);
    else
        return symbol_kind::S_YYUNDEF;
}

#line 28 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
} // namespace NonGroundGrammar
} // namespace Input
} // namespace Gringo
#line 3675 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.cc"
