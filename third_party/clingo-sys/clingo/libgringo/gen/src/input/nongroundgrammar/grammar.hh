// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton interface for Bison LALR(1) parsers in C++

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

/**
 ** \file /home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.hh
 ** Define the Gringo::Input::NonGroundGrammar::parser class.
 */

// C++ LALR(1) parser skeleton written by Akim Demaille.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.

#ifndef YY_GRINGONONGROUNDGRAMMAR_HOME_KAMINSKI_DOCUMENTS_GIT_POTASSCO_CLINGO_BUILD_DEBUG_LIBGRINGO_SRC_INPUT_NONGROUNDGRAMMAR_GRAMMAR_HH_INCLUDED
#define YY_GRINGONONGROUNDGRAMMAR_HOME_KAMINSKI_DOCUMENTS_GIT_POTASSCO_CLINGO_BUILD_DEBUG_LIBGRINGO_SRC_INPUT_NONGROUNDGRAMMAR_GRAMMAR_HH_INCLUDED
// "%code requires" blocks.
#line 46 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"

#include "gringo/input/programbuilder.hh"
#include "potassco/basic_types.h"

namespace Gringo {
namespace Input {
class NonGroundParser;
}
} // namespace Gringo

struct DefaultLocation : Gringo::Location {
    DefaultLocation() : Location("<undef>", 0, 0, "<undef>", 0, 0) {}
};

#line 61 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.hh"

#include <cstdlib> // std::abort
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined __cplusplus
#define YY_CPLUSPLUS __cplusplus
#else
#define YY_CPLUSPLUS 199711L
#endif

// Support move semantics when possible.
#if 201103L <= YY_CPLUSPLUS
#define YY_MOVE std::move
#define YY_MOVE_OR_COPY move
#define YY_MOVE_REF(Type) Type &&
#define YY_RVREF(Type) Type &&
#define YY_COPY(Type) Type
#else
#define YY_MOVE
#define YY_MOVE_OR_COPY copy
#define YY_MOVE_REF(Type) Type &
#define YY_RVREF(Type) const Type &
#define YY_COPY(Type) const Type &
#endif

// Support noexcept when possible.
#if 201103L <= YY_CPLUSPLUS
#define YY_NOEXCEPT noexcept
#define YY_NOTHROW
#else
#define YY_NOEXCEPT
#define YY_NOTHROW throw()
#endif

// Support constexpr when possible.
#if 201703 <= YY_CPLUSPLUS
#define YY_CONSTEXPR constexpr
#else
#define YY_CONSTEXPR
#endif

#ifndef YY_ATTRIBUTE_PURE
#if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_PURE __attribute__((__pure__))
#else
#define YY_ATTRIBUTE_PURE
#endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
#if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
#define YY_ATTRIBUTE_UNUSED
#endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if !defined lint || defined __GNUC__
#define YY_USE(E) ((void)(E))
#else
#define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && !defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
#if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                                                                            \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")
#else
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                                                                            \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")                               \
        _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#endif
#define YY_IGNORE_MAYBE_UNINITIALIZED_END _Pragma("GCC diagnostic pop")
#else
#define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
#define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && !defined __ICC && 6 <= __GNUC__
#define YY_IGNORE_USELESS_CAST_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define YY_IGNORE_USELESS_CAST_END _Pragma("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_END
#endif

#ifndef YY_CAST
#ifdef __cplusplus
#define YY_CAST(Type, Val) static_cast<Type>(Val)
#define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type>(Val)
#else
#define YY_CAST(Type, Val) ((Type)(Val))
#define YY_REINTERPRET_CAST(Type, Val) ((Type)(Val))
#endif
#endif
#ifndef YY_NULLPTR
#if defined __cplusplus
#if 201103L <= __cplusplus
#define YY_NULLPTR nullptr
#else
#define YY_NULLPTR 0
#endif
#else
#define YY_NULLPTR ((void *)0)
#endif
#endif

/* Debug traces.  */
#ifndef YYDEBUG
#define YYDEBUG 0
#endif

#line 28 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
namespace Gringo {
namespace Input {
namespace NonGroundGrammar {
#line 197 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.hh"

/// A Bison parser.
class parser {
  public:
#ifdef YYSTYPE
#ifdef __GNUC__
#pragma GCC message "bison: do not #define YYSTYPE in C++, use %define api.value.type"
#endif
    typedef YYSTYPE value_type;
#else
    /// Symbol semantic values.
    union value_type {
#line 108 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"

        IdVecUid idlist;
        TermUid term;
        TermVecUid termvec;
        TermVecVecUid termvecvec;
        LitVecUid litvec;
        LitUid lit;
        RelLitVecUid rellitvec;
        BdAggrElemVecUid bodyaggrelemvec;
        CondLitVecUid condlitlist;
        HdAggrElemVecUid headaggrelemvec;
        BoundVecUid bounds;
        BdLitVecUid body;
        HdLitUid head;
        Relation rel;
        AggregateFunction fun;
        struct {
            uintptr_t first;
            unsigned second;
        } pair;
        struct {
            TermVecUid first;
            LitVecUid second;
        } bodyaggrelem;
        struct {
            LitUid first;
            LitVecUid second;
        } lbodyaggrelem;
        struct {
            AggregateFunction fun;
            unsigned choice : 1;
            unsigned elems : 31;
        } aggr;
        struct {
            Relation rel;
            TermUid term;
        } bound;
        struct {
            TermUid first;
            TermUid second;
        } termpair;
        unsigned uid;
        uintptr_t str;
        int num;
        Potassco::Heuristic_t::E heu;
        TheoryOpVecUid theoryOps;
        TheoryTermUid theoryTerm;
        TheoryOptermUid theoryOpterm;
        TheoryOptermVecUid theoryOpterms;
        TheoryElemVecUid theoryElems;
        struct {
            TheoryOptermVecUid first;
            LitVecUid second;
        } theoryElem;
        TheoryAtomUid theoryAtom;
        TheoryOpDefUid theoryOpDef;
        TheoryOpDefVecUid theoryOpDefs;
        TheoryTermDefUid theoryTermDef;
        TheoryAtomDefUid theoryAtomDef;
        TheoryDefVecUid theoryDefs;
        TheoryAtomType theoryAtomType;

#line 278 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.hh"
    };
#endif
    /// Backward compatibility (Bison 3.8).
    typedef value_type semantic_type;

    /// Symbol locations.
    typedef DefaultLocation location_type;

    /// Syntax errors thrown from user actions.
    struct syntax_error : std::runtime_error {
        syntax_error(const location_type &l, const std::string &m) : std::runtime_error(m), location(l) {}

        syntax_error(const syntax_error &s) : std::runtime_error(s.what()), location(s.location) {}

        ~syntax_error() YY_NOEXCEPT YY_NOTHROW;

        location_type location;
    };

    /// Token kinds.
    struct token {
        enum token_kind_type {
            YYEMPTY = -2,
            END = 0,          // "<EOF>"
            YYerror = 256,    // error
            YYUNDEF = 257,    // "invalid token"
            ADD = 258,        // "+"
            AND = 259,        // "&"
            ANY = 260,        // "any"
            AT = 261,         // "@"
            BINARY = 262,     // "binary"
            BLOCK = 263,      // "#program"
            BNOT = 264,       // "~"
            BODY = 265,       // "body"
            COLON = 266,      // ":"
            COMMA = 267,      // ","
            CONST = 268,      // "#const"
            COUNT = 269,      // "#count"
            DEFINED = 270,    // "#defined"
            DIRECTIVE = 271,  // "directive"
            DOT = 272,        // "."
            DOTS = 273,       // ".."
            EDGE = 274,       // "#edge"
            EQ = 275,         // "="
            EXTERNAL = 276,   // "#external"
            FALSE = 277,      // "#false"
            GEQ = 278,        // ">="
            GT = 279,         // ">"
            HEAD = 280,       // "head"
            HEURISTIC = 281,  // "#heuristic"
            IF = 282,         // ":-"
            INCLUDE = 283,    // "#include"
            INFIMUM = 284,    // "#inf"
            LBRACE = 285,     // "{"
            LBRACK = 286,     // "["
            LEFT = 287,       // "left"
            LEQ = 288,        // "<="
            LPAREN = 289,     // "("
            LT = 290,         // "<"
            MAXIMIZE = 291,   // "#maximize"
            MAX = 292,        // "#max"
            MINIMIZE = 293,   // "#minimize"
            MIN = 294,        // "#min"
            MOD = 295,        // "\\"
            MUL = 296,        // "*"
            NEQ = 297,        // "!="
            PARSE_DEF = 298,  // "<define>"
            PARSE_LP = 299,   // "<program>"
            POW = 300,        // "**"
            PROJECT = 301,    // "#project"
            QUESTION = 302,   // "?"
            RBRACE = 303,     // "}"
            RBRACK = 304,     // "]"
            RIGHT = 305,      // "right"
            RPAREN = 306,     // ")"
            SEM = 307,        // ";"
            SHOW = 308,       // "#show"
            SHOWSIG = 309,    // "#showsig"
            SLASH = 310,      // "/"
            SUB = 311,        // "-"
            SUMP = 312,       // "#sum+"
            SUM = 313,        // "#sum"
            SUPREMUM = 314,   // "#sup"
            SYNC = 315,       // "EOF"
            THEORY = 316,     // "#theory"
            TRUE = 317,       // "#true"
            UBNOT = 318,      // UBNOT
            UMINUS = 319,     // UMINUS
            UNARY = 320,      // "unary"
            VBAR = 321,       // "|"
            WIF = 322,        // ":~"
            XOR = 323,        // "^"
            NUMBER = 324,     // "<NUMBER>"
            ANONYMOUS = 325,  // "<ANONYMOUS>"
            IDENTIFIER = 326, // "<IDENTIFIER>"
            SCRIPT = 327,     // "<SCRIPT>"
            CODE = 328,       // "<CODE>"
            STRING = 329,     // "<STRING>"
            VARIABLE = 330,   // "<VARIABLE>"
            THEORY_OP = 331,  // "<THEORYOP>"
            NOT = 332,        // "not"
            DEFAULT = 333,    // "default"
            OVERRIDE = 334    // "override"
        };
        /// Backward compatibility alias (Bison 3.6).
        typedef token_kind_type yytokentype;
    };

    /// Token kind, as returned by yylex.
    typedef token::token_kind_type token_kind_type;

    /// Backward compatibility alias (Bison 3.6).
    typedef token_kind_type token_type;

    /// Symbol kinds.
    struct symbol_kind {
        enum symbol_kind_type {
            YYNTOKENS = 80, ///< Number of tokens.
            S_YYEMPTY = -2,
            S_YYEOF = 0,                              // "<EOF>"
            S_YYerror = 1,                            // error
            S_YYUNDEF = 2,                            // "invalid token"
            S_ADD = 3,                                // "+"
            S_AND = 4,                                // "&"
            S_ANY = 5,                                // "any"
            S_AT = 6,                                 // "@"
            S_BINARY = 7,                             // "binary"
            S_BLOCK = 8,                              // "#program"
            S_BNOT = 9,                               // "~"
            S_BODY = 10,                              // "body"
            S_COLON = 11,                             // ":"
            S_COMMA = 12,                             // ","
            S_CONST = 13,                             // "#const"
            S_COUNT = 14,                             // "#count"
            S_DEFINED = 15,                           // "#defined"
            S_DIRECTIVE = 16,                         // "directive"
            S_DOT = 17,                               // "."
            S_DOTS = 18,                              // ".."
            S_EDGE = 19,                              // "#edge"
            S_EQ = 20,                                // "="
            S_EXTERNAL = 21,                          // "#external"
            S_FALSE = 22,                             // "#false"
            S_GEQ = 23,                               // ">="
            S_GT = 24,                                // ">"
            S_HEAD = 25,                              // "head"
            S_HEURISTIC = 26,                         // "#heuristic"
            S_IF = 27,                                // ":-"
            S_INCLUDE = 28,                           // "#include"
            S_INFIMUM = 29,                           // "#inf"
            S_LBRACE = 30,                            // "{"
            S_LBRACK = 31,                            // "["
            S_LEFT = 32,                              // "left"
            S_LEQ = 33,                               // "<="
            S_LPAREN = 34,                            // "("
            S_LT = 35,                                // "<"
            S_MAXIMIZE = 36,                          // "#maximize"
            S_MAX = 37,                               // "#max"
            S_MINIMIZE = 38,                          // "#minimize"
            S_MIN = 39,                               // "#min"
            S_MOD = 40,                               // "\\"
            S_MUL = 41,                               // "*"
            S_NEQ = 42,                               // "!="
            S_PARSE_DEF = 43,                         // "<define>"
            S_PARSE_LP = 44,                          // "<program>"
            S_POW = 45,                               // "**"
            S_PROJECT = 46,                           // "#project"
            S_QUESTION = 47,                          // "?"
            S_RBRACE = 48,                            // "}"
            S_RBRACK = 49,                            // "]"
            S_RIGHT = 50,                             // "right"
            S_RPAREN = 51,                            // ")"
            S_SEM = 52,                               // ";"
            S_SHOW = 53,                              // "#show"
            S_SHOWSIG = 54,                           // "#showsig"
            S_SLASH = 55,                             // "/"
            S_SUB = 56,                               // "-"
            S_SUMP = 57,                              // "#sum+"
            S_SUM = 58,                               // "#sum"
            S_SUPREMUM = 59,                          // "#sup"
            S_SYNC = 60,                              // "EOF"
            S_THEORY = 61,                            // "#theory"
            S_TRUE = 62,                              // "#true"
            S_UBNOT = 63,                             // UBNOT
            S_UMINUS = 64,                            // UMINUS
            S_UNARY = 65,                             // "unary"
            S_VBAR = 66,                              // "|"
            S_WIF = 67,                               // ":~"
            S_XOR = 68,                               // "^"
            S_NUMBER = 69,                            // "<NUMBER>"
            S_ANONYMOUS = 70,                         // "<ANONYMOUS>"
            S_IDENTIFIER = 71,                        // "<IDENTIFIER>"
            S_SCRIPT = 72,                            // "<SCRIPT>"
            S_CODE = 73,                              // "<CODE>"
            S_STRING = 74,                            // "<STRING>"
            S_VARIABLE = 75,                          // "<VARIABLE>"
            S_THEORY_OP = 76,                         // "<THEORYOP>"
            S_NOT = 77,                               // "not"
            S_DEFAULT = 78,                           // "default"
            S_OVERRIDE = 79,                          // "override"
            S_YYACCEPT = 80,                          // $accept
            S_start = 81,                             // start
            S_program = 82,                           // program
            S_statement = 83,                         // statement
            S_identifier = 84,                        // identifier
            S_constterm = 85,                         // constterm
            S_consttermvec = 86,                      // consttermvec
            S_constargvec = 87,                       // constargvec
            S_term = 88,                              // term
            S_unaryargvec = 89,                       // unaryargvec
            S_ntermvec = 90,                          // ntermvec
            S_termvec = 91,                           // termvec
            S_tuple = 92,                             // tuple
            S_tuplevec_sem = 93,                      // tuplevec_sem
            S_tuplevec = 94,                          // tuplevec
            S_argvec = 95,                            // argvec
            S_binaryargvec = 96,                      // binaryargvec
            S_cmp = 97,                               // cmp
            S_atom = 98,                              // atom
            S_rellitvec = 99,                         // rellitvec
            S_literal = 100,                          // literal
            S_nlitvec = 101,                          // nlitvec
            S_litvec = 102,                           // litvec
            S_optcondition = 103,                     // optcondition
            S_aggregatefunction = 104,                // aggregatefunction
            S_bodyaggrelem = 105,                     // bodyaggrelem
            S_bodyaggrelemvec = 106,                  // bodyaggrelemvec
            S_altbodyaggrelem = 107,                  // altbodyaggrelem
            S_altbodyaggrelemvec = 108,               // altbodyaggrelemvec
            S_bodyaggregate = 109,                    // bodyaggregate
            S_upper = 110,                            // upper
            S_lubodyaggregate = 111,                  // lubodyaggregate
            S_headaggrelemvec = 112,                  // headaggrelemvec
            S_altheadaggrelemvec = 113,               // altheadaggrelemvec
            S_headaggregate = 114,                    // headaggregate
            S_luheadaggregate = 115,                  // luheadaggregate
            S_conjunction = 116,                      // conjunction
            S_dsym = 117,                             // dsym
            S_disjunctionsep = 118,                   // disjunctionsep
            S_disjunction = 119,                      // disjunction
            S_bodycomma = 120,                        // bodycomma
            S_bodydot = 121,                          // bodydot
            S_bodyconddot = 122,                      // bodyconddot
            S_head = 123,                             // head
            S_optimizetuple = 124,                    // optimizetuple
            S_optimizeweight = 125,                   // optimizeweight
            S_optimizelitvec = 126,                   // optimizelitvec
            S_optimizecond = 127,                     // optimizecond
            S_maxelemlist = 128,                      // maxelemlist
            S_minelemlist = 129,                      // minelemlist
            S_define = 130,                           // define
            S_storecomment = 131,                     // storecomment
            S_nidlist = 132,                          // nidlist
            S_idlist = 133,                           // idlist
            S_theory_op = 134,                        // theory_op
            S_theory_op_list = 135,                   // theory_op_list
            S_theory_term = 136,                      // theory_term
            S_theory_opterm = 137,                    // theory_opterm
            S_theory_opterm_nlist = 138,              // theory_opterm_nlist
            S_theory_opterm_list = 139,               // theory_opterm_list
            S_theory_atom_element = 140,              // theory_atom_element
            S_theory_atom_element_nlist = 141,        // theory_atom_element_nlist
            S_theory_atom_element_list = 142,         // theory_atom_element_list
            S_theory_atom_name = 143,                 // theory_atom_name
            S_theory_atom = 144,                      // theory_atom
            S_theory_operator_nlist = 145,            // theory_operator_nlist
            S_theory_operator_list = 146,             // theory_operator_list
            S_theory_operator_definition = 147,       // theory_operator_definition
            S_theory_operator_definition_nlist = 148, // theory_operator_definition_nlist
            S_theory_operator_definiton_list = 149,   // theory_operator_definiton_list
            S_theory_definition_identifier = 150,     // theory_definition_identifier
            S_theory_term_definition = 151,           // theory_term_definition
            S_theory_atom_type = 152,                 // theory_atom_type
            S_theory_atom_definition = 153,           // theory_atom_definition
            S_theory_definition_nlist = 154,          // theory_definition_nlist
            S_theory_definition_list = 155,           // theory_definition_list
            S_enable_theory_lexing = 156,             // enable_theory_lexing
            S_enable_theory_definition_lexing = 157,  // enable_theory_definition_lexing
            S_disable_theory_lexing = 158             // disable_theory_lexing
        };
    };

    /// (Internal) symbol kind.
    typedef symbol_kind::symbol_kind_type symbol_kind_type;

    /// The number of tokens.
    static const symbol_kind_type YYNTOKENS = symbol_kind::YYNTOKENS;

    /// A complete symbol.
    ///
    /// Expects its Base type to provide access to the symbol kind
    /// via kind ().
    ///
    /// Provide access to semantic value and location.
    template <typename Base> struct basic_symbol : Base {
        /// Alias to Base.
        typedef Base super_type;

        /// Default constructor.
        basic_symbol() YY_NOEXCEPT : value(), location() {}

#if 201103L <= YY_CPLUSPLUS
        /// Move constructor.
        basic_symbol(basic_symbol &&that)
            : Base(std::move(that)), value(std::move(that.value)), location(std::move(that.location)) {}
#endif

        /// Copy constructor.
        basic_symbol(const basic_symbol &that);
        /// Constructor for valueless symbols.
        basic_symbol(typename Base::kind_type t, YY_MOVE_REF(location_type) l);

        /// Constructor for symbols with semantic value.
        basic_symbol(typename Base::kind_type t, YY_RVREF(value_type) v, YY_RVREF(location_type) l);

        /// Destroy the symbol.
        ~basic_symbol() { clear(); }

        /// Destroy contents, and record that is empty.
        void clear() YY_NOEXCEPT { Base::clear(); }

        /// The user-facing name of this symbol.
        std::string name() const YY_NOEXCEPT { return parser::symbol_name(this->kind()); }

        /// Backward compatibility (Bison 3.6).
        symbol_kind_type type_get() const YY_NOEXCEPT;

        /// Whether empty.
        bool empty() const YY_NOEXCEPT;

        /// Destructive move, \a s is emptied into this.
        void move(basic_symbol &s);

        /// The semantic value.
        value_type value;

        /// The location.
        location_type location;

      private:
#if YY_CPLUSPLUS < 201103L
        /// Assignment operator.
        basic_symbol &operator=(const basic_symbol &that);
#endif
    };

    /// Type access provider for token (enum) based symbols.
    struct by_kind {
        /// The symbol kind as needed by the constructor.
        typedef token_kind_type kind_type;

        /// Default constructor.
        by_kind() YY_NOEXCEPT;

#if 201103L <= YY_CPLUSPLUS
        /// Move constructor.
        by_kind(by_kind &&that) YY_NOEXCEPT;
#endif

        /// Copy constructor.
        by_kind(const by_kind &that) YY_NOEXCEPT;

        /// Constructor from (external) token numbers.
        by_kind(kind_type t) YY_NOEXCEPT;

        /// Record that this symbol is empty.
        void clear() YY_NOEXCEPT;

        /// Steal the symbol kind from \a that.
        void move(by_kind &that);

        /// The (internal) type number (corresponding to \a type).
        /// \a empty when empty.
        symbol_kind_type kind() const YY_NOEXCEPT;

        /// Backward compatibility (Bison 3.6).
        symbol_kind_type type_get() const YY_NOEXCEPT;

        /// The symbol kind.
        /// \a S_YYEMPTY when empty.
        symbol_kind_type kind_;
    };

    /// Backward compatibility for a private implementation detail (Bison 3.6).
    typedef by_kind by_type;

    /// "External" symbols: returned by the scanner.
    struct symbol_type : basic_symbol<by_kind> {};

    /// Build a parser object.
    parser(Gringo::Input::NonGroundParser *lexer_yyarg);
    virtual ~parser();

#if 201103L <= YY_CPLUSPLUS
    /// Non copyable.
    parser(const parser &) = delete;
    /// Non copyable.
    parser &operator=(const parser &) = delete;
#endif

    /// Parse.  An alias for parse ().
    /// \returns  0 iff parsing succeeded.
    int operator()();

    /// Parse.
    /// \returns  0 iff parsing succeeded.
    virtual int parse();

#if YYDEBUG
    /// The current debugging stream.
    std::ostream &debug_stream() const YY_ATTRIBUTE_PURE;
    /// Set the current debugging stream.
    void set_debug_stream(std::ostream &);

    /// Type for debugging levels.
    typedef int debug_level_type;
    /// The current debugging level.
    debug_level_type debug_level() const YY_ATTRIBUTE_PURE;
    /// Set the current debugging level.
    void set_debug_level(debug_level_type l);
#endif

    /// Report a syntax error.
    /// \param loc    where the syntax error is found.
    /// \param msg    a description of the syntax error.
    virtual void error(const location_type &loc, const std::string &msg);

    /// Report a syntax error.
    void error(const syntax_error &err);

    /// The user-facing name of the symbol whose (internal) number is
    /// YYSYMBOL.  No bounds checking.
    static std::string symbol_name(symbol_kind_type yysymbol);

    class context {
      public:
        context(const parser &yyparser, const symbol_type &yyla);
        const symbol_type &lookahead() const YY_NOEXCEPT { return yyla_; }
        symbol_kind_type token() const YY_NOEXCEPT { return yyla_.kind(); }
        const location_type &location() const YY_NOEXCEPT { return yyla_.location; }

        /// Put in YYARG at most YYARGN of the expected tokens, and return the
        /// number of tokens stored in YYARG.  If YYARG is null, return the
        /// number of expected tokens (guaranteed to be less than YYNTOKENS).
        int expected_tokens(symbol_kind_type yyarg[], int yyargn) const;

      private:
        const parser &yyparser_;
        const symbol_type &yyla_;
    };

  private:
#if YY_CPLUSPLUS < 201103L
    /// Non copyable.
    parser(const parser &);
    /// Non copyable.
    parser &operator=(const parser &);
#endif

    /// Stored state numbers (used for stacks).
    typedef short state_type;

    /// The arguments of the error message.
    int yy_syntax_error_arguments_(const context &yyctx, symbol_kind_type yyarg[], int yyargn) const;

    /// Generate an error message.
    /// \param yyctx     the context in which the error occurred.
    virtual std::string yysyntax_error_(const context &yyctx) const;
    /// Compute post-reduction state.
    /// \param yystate   the current state
    /// \param yysym     the nonterminal to push on the stack
    static state_type yy_lr_goto_state_(state_type yystate, int yysym);

    /// Whether the given \c yypact_ value indicates a defaulted state.
    /// \param yyvalue   the value to check
    static bool yy_pact_value_is_default_(int yyvalue) YY_NOEXCEPT;

    /// Whether the given \c yytable_ value indicates a syntax error.
    /// \param yyvalue   the value to check
    static bool yy_table_value_is_error_(int yyvalue) YY_NOEXCEPT;

    static const short yypact_ninf_;
    static const short yytable_ninf_;

    /// Convert a scanner token kind \a t to a symbol kind.
    /// In theory \a t should be a token_kind_type, but character literals
    /// are valid, yet not members of the token_kind_type enum.
    static symbol_kind_type yytranslate_(int t) YY_NOEXCEPT;

    /// Convert the symbol name \a n to a form suitable for a diagnostic.
    static std::string yytnamerr_(const char *yystr);

    /// For a symbol, its name in clear.
    static const char *const yytname_[];

    // Tables.
    // YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
    // STATE-NUM.
    static const short yypact_[];

    // YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
    // Performed when YYTABLE does not specify something else to do.  Zero
    // means the default is an error.
    static const short yydefact_[];

    // YYPGOTO[NTERM-NUM].
    static const short yypgoto_[];

    // YYDEFGOTO[NTERM-NUM].
    static const short yydefgoto_[];

    // YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
    // positive, shift that token.  If negative, reduce the rule whose
    // number is the opposite.  If YYTABLE_NINF, syntax error.
    static const short yytable_[];

    static const short yycheck_[];

    // YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
    // state STATE-NUM.
    static const unsigned char yystos_[];

    // YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.
    static const unsigned char yyr1_[];

    // YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.
    static const signed char yyr2_[];

#if YYDEBUG
    // YYRLINE[YYN] -- Source line where rule number YYN was defined.
    static const short yyrline_[];
    /// Report on the debug stream that the rule \a r is going to be reduced.
    virtual void yy_reduce_print_(int r) const;
    /// Print the state stack on the debug stream.
    virtual void yy_stack_print_() const;

    /// Debugging level.
    int yydebug_;
    /// Debug stream.
    std::ostream *yycdebug_;

    /// \brief Display a symbol kind, value and location.
    /// \param yyo    The output stream.
    /// \param yysym  The symbol.
    template <typename Base> void yy_print_(std::ostream &yyo, const basic_symbol<Base> &yysym) const;
#endif

    /// \brief Reclaim the memory associated to a symbol.
    /// \param yymsg     Why this token is reclaimed.
    ///                  If null, print nothing.
    /// \param yysym     The symbol.
    template <typename Base> void yy_destroy_(const char *yymsg, basic_symbol<Base> &yysym) const;

  private:
    /// Type access provider for state based symbols.
    struct by_state {
        /// Default constructor.
        by_state() YY_NOEXCEPT;

        /// The symbol kind as needed by the constructor.
        typedef state_type kind_type;

        /// Constructor.
        by_state(kind_type s) YY_NOEXCEPT;

        /// Copy constructor.
        by_state(const by_state &that) YY_NOEXCEPT;

        /// Record that this symbol is empty.
        void clear() YY_NOEXCEPT;

        /// Steal the symbol kind from \a that.
        void move(by_state &that);

        /// The symbol kind (corresponding to \a state).
        /// \a symbol_kind::S_YYEMPTY when empty.
        symbol_kind_type kind() const YY_NOEXCEPT;

        /// The state number used to denote an empty symbol.
        /// We use the initial state, as it does not have a value.
        enum { empty_state = 0 };

        /// The state.
        /// \a empty when empty.
        state_type state;
    };

    /// "Internal" symbol: element of the stack.
    struct stack_symbol_type : basic_symbol<by_state> {
        /// Superclass.
        typedef basic_symbol<by_state> super_type;
        /// Construct an empty symbol.
        stack_symbol_type();
        /// Move or copy construction.
        stack_symbol_type(YY_RVREF(stack_symbol_type) that);
        /// Steal the contents from \a sym to build this.
        stack_symbol_type(state_type s, YY_MOVE_REF(symbol_type) sym);
#if YY_CPLUSPLUS < 201103L
        /// Assignment, needed by push_back by some old implementations.
        /// Moves the contents of that.
        stack_symbol_type &operator=(stack_symbol_type &that);

        /// Assignment, needed by push_back by other implementations.
        /// Needed by some other old implementations.
        stack_symbol_type &operator=(const stack_symbol_type &that);
#endif
    };

    /// A stack with random access from its top.
    template <typename T, typename S = std::vector<T>> class stack {
      public:
        // Hide our reversed order.
        typedef typename S::iterator iterator;
        typedef typename S::const_iterator const_iterator;
        typedef typename S::size_type size_type;
        typedef typename std::ptrdiff_t index_type;

        stack(size_type n = 200) YY_NOEXCEPT : seq_(n) {}

#if 201103L <= YY_CPLUSPLUS
        /// Non copyable.
        stack(const stack &) = delete;
        /// Non copyable.
        stack &operator=(const stack &) = delete;
#endif

        /// Random access.
        ///
        /// Index 0 returns the topmost element.
        const T &operator[](index_type i) const { return seq_[size_type(size() - 1 - i)]; }

        /// Random access.
        ///
        /// Index 0 returns the topmost element.
        T &operator[](index_type i) { return seq_[size_type(size() - 1 - i)]; }

        /// Steal the contents of \a t.
        ///
        /// Close to move-semantics.
        void push(YY_MOVE_REF(T) t) {
            seq_.push_back(T());
            operator[](0).move(t);
        }

        /// Pop elements from the stack.
        void pop(std::ptrdiff_t n = 1) YY_NOEXCEPT {
            for (; 0 < n; --n)
                seq_.pop_back();
        }

        /// Pop all elements from the stack.
        void clear() YY_NOEXCEPT { seq_.clear(); }

        /// Number of elements on the stack.
        index_type size() const YY_NOEXCEPT { return index_type(seq_.size()); }

        /// Iterator on top of the stack (going downwards).
        const_iterator begin() const YY_NOEXCEPT { return seq_.begin(); }

        /// Bottom of the stack.
        const_iterator end() const YY_NOEXCEPT { return seq_.end(); }

        /// Present a slice of the top of a stack.
        class slice {
          public:
            slice(const stack &stack, index_type range) YY_NOEXCEPT : stack_(stack), range_(range) {}

            const T &operator[](index_type i) const { return stack_[range_ - i]; }

          private:
            const stack &stack_;
            index_type range_;
        };

      private:
#if YY_CPLUSPLUS < 201103L
        /// Non copyable.
        stack(const stack &);
        /// Non copyable.
        stack &operator=(const stack &);
#endif
        /// The wrapped container.
        S seq_;
    };

    /// Stack type.
    typedef stack<stack_symbol_type> stack_type;

    /// The stack.
    stack_type yystack_;

    /// Push a new state on the stack.
    /// \param m    a debug message to display
    ///             if null, no trace is output.
    /// \param sym  the symbol
    /// \warning the contents of \a s.value is stolen.
    void yypush_(const char *m, YY_MOVE_REF(stack_symbol_type) sym);

    /// Push a new look ahead token on the state on the stack.
    /// \param m    a debug message to display
    ///             if null, no trace is output.
    /// \param s    the state
    /// \param sym  the symbol (for its value and location).
    /// \warning the contents of \a sym.value is stolen.
    void yypush_(const char *m, state_type s, YY_MOVE_REF(symbol_type) sym);

    /// Pop \a n symbols from the stack.
    void yypop_(int n = 1) YY_NOEXCEPT;

    /// Constants.
    enum {
        yylast_ = 2028, ///< Last index in yytable_.
        yynnts_ = 79,   ///< Number of nonterminal symbols.
        yyfinal_ = 10   ///< Termination state number.
    };

    // User arguments.
    Gringo::Input::NonGroundParser *lexer;
};

#line 28 "/home/kaminski/Documents/git/potassco/clingo/libgringo/src/input/nongroundgrammar.yy"
} // namespace NonGroundGrammar
} // namespace Input
} // namespace Gringo
#line 1094 "/home/kaminski/Documents/git/potassco/clingo/build/debug/libgringo/src/input/nongroundgrammar/grammar.hh"

#endif // !YY_GRINGONONGROUNDGRAMMAR_HOME_KAMINSKI_DOCUMENTS_GIT_POTASSCO_CLINGO_BUILD_DEBUG_LIBGRINGO_SRC_INPUT_NONGROUNDGRAMMAR_GRAMMAR_HH_INCLUDED
