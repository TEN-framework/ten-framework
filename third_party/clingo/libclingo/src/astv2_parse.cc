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

#include <clingo/astv2.hh>

namespace Gringo { namespace Input {

namespace {

template <class T>
T &get(AST &ast, clingo_ast_attribute_e name) {
    return mpark::get<T>(ast.value(name));
}

template <class T>
T const &get(AST const &ast, clingo_ast_attribute_e name) {
    return mpark::get<T>(ast.value(name));
}

AST *getOpt(AST const &ast, clingo_ast_attribute_e name) {
    if (!ast.hasValue(name)) {
        return nullptr;
    }
    return mpark::get<OAST>(ast.value(name)).ast.get();
}

struct ASTParser {
public:
    ASTParser(Logger &log, INongroundProgramBuilder &prg)
    : log_(log), prg_(prg) { }

    // {{{1 statement

    void parseStatement(AST const &ast) {
        switch (ast.type()) {
            case clingo_ast_type_rule: {
                return prg_.rule(get<Location>(ast, clingo_ast_attribute_location),
                                 parseHeadLiteral(*get<SAST>(ast, clingo_ast_attribute_head)),
                                 parseBodyLiteralVec(get<AST::ASTVec>(ast, clingo_ast_attribute_body)));
            }
            case clingo_ast_type_definition: {
                return prg_.define(get<Location>(ast, clingo_ast_attribute_location),
                                   get<String>(ast, clingo_ast_attribute_name), parseTerm(*get<SAST>(ast, clingo_ast_attribute_value)),
                                   get<int>(ast, clingo_ast_attribute_is_default) != 0, log_);
            }
            case clingo_ast_type_show_signature: {
                return prg_.showsig(get<Location>(ast, clingo_ast_attribute_location),
                                    Sig(get<String>(ast, clingo_ast_attribute_name),
                                        get<int>(ast, clingo_ast_attribute_arity),
                                        get<int>(ast, clingo_ast_attribute_positive) == 0));
            }
            case clingo_ast_type_show_term: {
                return prg_.show(get<Location>(ast, clingo_ast_attribute_location),
                                 parseTerm(*get<SAST>(ast, clingo_ast_attribute_term)),
                                 parseBodyLiteralVec(get<AST::ASTVec>(ast, clingo_ast_attribute_body)));
            }
            case clingo_ast_type_minimize: {
                return prg_.optimize(get<Location>(ast, clingo_ast_attribute_location),
                                     parseTerm(*get<SAST>(ast, clingo_ast_attribute_weight)),
                                     parseTerm(*get<SAST>(ast, clingo_ast_attribute_priority)),
                                     parseTermVec(get<AST::ASTVec>(ast, clingo_ast_attribute_terms)),
                                     parseBodyLiteralVec(get<AST::ASTVec>(ast, clingo_ast_attribute_body)));
            }
            case clingo_ast_type_script: {
                return prg_.script(get<Location>(ast, clingo_ast_attribute_location),
                                   get<String>(ast, clingo_ast_attribute_name),
                                   get<String>(ast, clingo_ast_attribute_code));
            }
            case clingo_ast_type_program: {
                return prg_.block(get<Location>(ast, clingo_ast_attribute_location),
                                  get<String>(ast, clingo_ast_attribute_name),
                                  parseIdVec(get<AST::ASTVec>(ast, clingo_ast_attribute_parameters)));
            }
            case clingo_ast_type_external: {
                return prg_.external(get<Location>(ast, clingo_ast_attribute_location),
                                     parseAtom(*get<SAST>(ast, clingo_ast_attribute_atom)),
                                     parseBodyLiteralVec(get<AST::ASTVec>(ast, clingo_ast_attribute_body)),
                                     parseTerm(*get<SAST>(ast, clingo_ast_attribute_external_type)));
            }
            case clingo_ast_type_edge: {
                return prg_.edge(get<Location>(ast, clingo_ast_attribute_location),
                                 prg_.termvecvec(prg_.termvecvec(),
                                                 prg_.termvec(prg_.termvec(prg_.termvec(),
                                                                           parseTerm(*get<SAST>(ast, clingo_ast_attribute_node_u))),
                                                              parseTerm(*get<SAST>(ast, clingo_ast_attribute_node_v)))),
                                 parseBodyLiteralVec(get<AST::ASTVec>(ast, clingo_ast_attribute_body)));
            }
            case clingo_ast_type_heuristic: {
                return prg_.heuristic(get<Location>(ast, clingo_ast_attribute_location),
                                      parseAtom(*get<SAST>(ast, clingo_ast_attribute_atom)),
                                      parseBodyLiteralVec(get<AST::ASTVec>(ast, clingo_ast_attribute_body)),
                                      parseTerm(*get<SAST>(ast, clingo_ast_attribute_bias)),
                                      parseTerm(*get<SAST>(ast, clingo_ast_attribute_priority)),
                                      parseTerm(*get<SAST>(ast, clingo_ast_attribute_modifier)));
            }
            case clingo_ast_type_project_atom: {
                return prg_.project(get<Location>(ast, clingo_ast_attribute_location),
                                    parseAtom(*get<SAST>(ast, clingo_ast_attribute_atom)),
                                    parseBodyLiteralVec(get<AST::ASTVec>(ast, clingo_ast_attribute_body)));
            }
            case clingo_ast_type_project_signature: {
                return prg_.project(get<Location>(ast, clingo_ast_attribute_location),
                                    Sig(get<String>(ast, clingo_ast_attribute_name),
                                        get<int>(ast, clingo_ast_attribute_arity),
                                        get<int>(ast, clingo_ast_attribute_positive) == 0));
            }
            case clingo_ast_type_defined: {
                return prg_.defined(get<Location>(ast, clingo_ast_attribute_location),
                                    Sig(get<String>(ast, clingo_ast_attribute_name),
                                        get<int>(ast, clingo_ast_attribute_arity),
                                        get<int>(ast, clingo_ast_attribute_positive) == 0));
            }
            case clingo_ast_type_theory_definition: {
                return prg_.theorydef(get<Location>(ast, clingo_ast_attribute_location),
                                      get<String>(ast, clingo_ast_attribute_name),
                                      parseTheoryAtomDefinitionVec(parseTheoryTermDefinitionVec(get<AST::ASTVec>(ast, clingo_ast_attribute_terms)),
                                                                   get<AST::ASTVec>(ast, clingo_ast_attribute_atoms)),
                                      log_);
            }
            case clingo_ast_type_comment: {
                return prg_.comment(get<Location>(ast, clingo_ast_attribute_location),
                             get<String>(ast, clingo_ast_attribute_value),
                             parseCommentType(get<int>(ast, clingo_ast_attribute_comment_type)) == clingo_comment_type_block);
            }
            default: {
                throw std::runtime_error("invalid ast: statement expected");
            }
        }
    }

private:
    template <class T>
    T fail_(char const *message) {
        throw std::runtime_error(message);
    }

    bool require_(bool cond, char const *message) {
        if (!cond) { fail_<void>(message); }
        return false;
    }

    // {{{1 terms

    IdVecUid parseIdVec(AST::ASTVec const &asts) {
        auto uid = prg_.idvec();
        for (auto const &ast : asts) {
            require_(ast->type() == clingo_ast_type_id, "invalid ast: id required");
            prg_.idvec(uid, get<Location>(*ast, clingo_ast_attribute_location), get<String>(*ast, clingo_ast_attribute_name));
        }
        return uid;
    }

    static UnOp parseUnOp(int num) {
        switch (num) {
            case clingo_ast_unary_operator_minus: {
                return UnOp::NEG;
            }
            case clingo_ast_unary_operator_negation: {
                return UnOp::NOT;
            }
            case clingo_ast_unary_operator_absolute: {
                return UnOp::ABS;
            }
            default: {
                throw std::runtime_error("invalid ast: invalid unary operator");
            }
        }
    }

    static BinOp parseBinOp(int num) {
        switch (num) {
            case clingo_ast_binary_operator_xor: {
                return BinOp::XOR;
            }
            case clingo_ast_binary_operator_or: {
                return BinOp::OR;
            }
            case clingo_ast_binary_operator_and: {
                return BinOp::AND;
            }
            case clingo_ast_binary_operator_plus: {
                return BinOp::ADD;
            }
            case clingo_ast_binary_operator_minus: {
                return BinOp::SUB;
            }
            case clingo_ast_binary_operator_multiplication: {
                return BinOp::MUL;
            }
            case clingo_ast_binary_operator_division: {
                return BinOp::DIV;
            }
            case clingo_ast_binary_operator_modulo: {
                return BinOp::MOD;
            }
            case clingo_ast_binary_operator_power: {
                return BinOp::POW;
            }
            default: {
                throw std::runtime_error("invalid ast: invalid binary operator");
            }
        }
    }

    TermUid parseTerm(AST const &ast) {
        switch (ast.type()) {
            case clingo_ast_type_variable: {
                return prg_.term(get<Location>(ast, clingo_ast_attribute_location),
                                 get<String>(ast, clingo_ast_attribute_name));
            }
            case clingo_ast_type_symbolic_term: {
                return prg_.term(get<Location>(ast, clingo_ast_attribute_location),
                                 get<Symbol>(ast, clingo_ast_attribute_symbol));
            }
            case clingo_ast_type_unary_operation: {
                return prg_.term(get<Location>(ast, clingo_ast_attribute_location),
                                 parseUnOp(get<int>(ast, clingo_ast_attribute_operator_type)),
                                 parseTerm(*get<SAST>(ast, clingo_ast_attribute_argument)));
            }
            case clingo_ast_type_binary_operation: {
                return prg_.term(get<Location>(ast, clingo_ast_attribute_location),
                                 parseBinOp(get<int>(ast, clingo_ast_attribute_operator_type)),
                                 parseTerm(*get<SAST>(ast, clingo_ast_attribute_left)),
                                 parseTerm(*get<SAST>(ast, clingo_ast_attribute_right)));
            }
            case clingo_ast_type_interval: {
                return prg_.term(get<Location>(ast, clingo_ast_attribute_location),
                                 parseTerm(*get<SAST>(ast, clingo_ast_attribute_left)),
                                 parseTerm(*get<SAST>(ast, clingo_ast_attribute_right)));
            }
            case clingo_ast_type_function: {
                auto external = ast.hasValue(clingo_ast_attribute_external) && get<int>(ast, clingo_ast_attribute_external) != 0;
                auto name = get<String>(ast, clingo_ast_attribute_name);
                require_(!name.empty() || !external, "invalid ast: external functions must have a name");
                return !name.empty()
                    ? prg_.term(get<Location>(ast, clingo_ast_attribute_location),
                                name,
                                prg_.termvecvec(prg_.termvecvec(),
                                                parseTermVec(get<AST::ASTVec>(ast, clingo_ast_attribute_arguments))),
                                external)
                    : prg_.term(get<Location>(ast, clingo_ast_attribute_location),
                                parseTermVec(get<AST::ASTVec>(ast, clingo_ast_attribute_arguments)),
                                true);
            }
            case clingo_ast_type_pool: {
                return prg_.pool(get<Location>(ast, clingo_ast_attribute_location),
                                 parseTermVec(get<AST::ASTVec>(ast, clingo_ast_attribute_arguments)));
            }
            default: {
                throw std::runtime_error("invalid ast: term expected");
            }
        }
    }

    TermVecUid parseTermVec(AST::ASTVec const &asts) {
        TermVecUid uid = prg_.termvec();
        for (auto const &ast : asts) {
            prg_.termvec(uid, parseTerm(*ast));
        }
        return uid;
    }

    TheoryTermUid parseTheoryTerm(AST const &ast) {
        switch (ast.type()) {
            case clingo_ast_type_symbolic_term : {
                return prg_.theorytermvalue(get<Location>(ast, clingo_ast_attribute_location), get<Symbol>(ast, clingo_ast_attribute_symbol));
            }
            case clingo_ast_type_variable: {
                return prg_.theorytermvar(get<Location>(ast, clingo_ast_attribute_location), get<String>(ast, clingo_ast_attribute_name));
            }
            case clingo_ast_type_theory_sequence: {
                switch (get<int>(ast, clingo_ast_attribute_sequence_type)) {
                    case clingo_ast_theory_sequence_type_tuple: {
                        return prg_.theorytermtuple(get<Location>(ast, clingo_ast_attribute_location),
                                                    parseTheoryOptermVec(get<AST::ASTVec>(ast, clingo_ast_attribute_terms)));
                    }
                    case clingo_ast_theory_sequence_type_list: {
                        return prg_.theoryoptermlist(get<Location>(ast, clingo_ast_attribute_location),
                                                     parseTheoryOptermVec(get<AST::ASTVec>(ast, clingo_ast_attribute_terms)));
                    }
                    case clingo_ast_theory_sequence_type_set: {
                        return prg_.theorytermset(get<Location>(ast, clingo_ast_attribute_location),
                                                  parseTheoryOptermVec(get<AST::ASTVec>(ast, clingo_ast_attribute_terms)));
                    }
                    default: {
                        throw std::runtime_error("invalid ast: invalid theory sequence type");

                    }
                }
            }
            case clingo_ast_type_theory_function: {
                return prg_.theorytermfun(get<Location>(ast, clingo_ast_attribute_location),
                                          get<String>(ast, clingo_ast_attribute_name),
                                          parseTheoryOptermVec(get<AST::ASTVec>(ast, clingo_ast_attribute_arguments)));
            }
            case clingo_ast_type_theory_unparsed_term: {
                return prg_.theorytermopterm(get<Location>(ast, clingo_ast_attribute_location),
                                             parseTheoryUnparsedTermElements(get<AST::ASTVec>(ast, clingo_ast_attribute_elements)));
            }
            default: {
                throw std::runtime_error("invalid ast: theory term expected");
            }
        }
    }


    TheoryOptermVecUid parseTheoryOptermVec(AST::ASTVec const &asts) {
        auto uid = prg_.theoryopterms();
        for (auto const &ast : asts) {
            uid = prg_.theoryopterms(uid, get<Location>(*ast, clingo_ast_attribute_location), parseTheoryOpterm(*ast));
        }
        return uid;
    }

    TheoryOpVecUid parseTheoryOpVec(AST::StrVec const &strs) {
        auto uid = prg_.theoryops();
        for (auto const &str : strs) {
            uid = prg_.theoryops(uid, str);
        }
        return uid;
    }

    TheoryOptermUid parseTheoryOpterm(AST const &ast) {
        if (ast.type() == clingo_ast_type_theory_unparsed_term) {
            return parseTheoryUnparsedTermElements(get<AST::ASTVec>(ast, clingo_ast_attribute_elements));
        }
        return prg_.theoryopterm(prg_.theoryops(), parseTheoryTerm(ast));
    }

    TheoryOptermUid parseTheoryUnparsedTermElements(AST::ASTVec const &asts) {
        require_(!asts.empty(), "invalid ast: unparsed term list must not be empty");
        auto it = asts.begin();
        auto ie = asts.end();
        auto uid = prg_.theoryopterm(parseTheoryOpVec(get<AST::StrVec>(**it, clingo_ast_attribute_operators)),
                                     parseTheoryTerm(*get<SAST>(**it, clingo_ast_attribute_term)));
        for (++it; it != ie; ++it) {
            auto const &operators = get<AST::StrVec>(**it, clingo_ast_attribute_operators);
            require_(!operators.empty(), "invalid ast: at least one operator necessary on right-hand-side of unparsed theory term");
            uid = prg_.theoryopterm(uid,
                                    parseTheoryOpVec(operators),
                                    parseTheoryTerm(*get<SAST>(**it, clingo_ast_attribute_term)));
        }
        return uid;
    }

    // {{{1 literals

    TermUid parseAtom(AST &ast) {
        require_(ast.type() == clingo_ast_type_symbolic_atom, "invalid ast: symbolic atom expected");
        return parseTerm(*get<SAST>(ast, clingo_ast_attribute_symbol));
    }

    static NAF parseSign(int sign) {
        switch (sign) {
            case clingo_ast_sign_no_sign: {
                return NAF::POS;
            }
            case clingo_ast_sign_negation: {
                return NAF::NOT;
            }
            case clingo_ast_sign_double_negation: {
                return NAF::NOTNOT;
            }
            default: {
                throw std::runtime_error("invalid ast: invalid sign");
            }
        }
    }

    static Relation parseRelation(int relation) {
        switch (relation) {
            case clingo_ast_comparison_operator_less_equal: {
                return Relation::LEQ;
            }
            case clingo_ast_comparison_operator_less_than: {
                return Relation::LT;
            }
            case clingo_ast_comparison_operator_greater_equal: {
                return Relation::GEQ;
            }
            case clingo_ast_comparison_operator_greater_than: {
                return Relation::GT;
            }
            case clingo_ast_comparison_operator_equal: {
                return Relation::EQ;
            }
            case clingo_ast_comparison_operator_not_equal: {
                return Relation::NEQ;
            }
            default: {
                throw std::runtime_error("invalid ast: invalid sign");
            }
        }
    }

    RelLitVecUid parseRightGuards(AST::ASTVec const &vec) {
        if (vec.empty()) {
            throw std::runtime_error("invalid ast: a comparision must have at least one guard");
        }
        auto lhs = get<SAST>(*vec.front(), clingo_ast_attribute_term);
        auto ret = prg_.rellitvec(get<Location>(*lhs, clingo_ast_attribute_location),
                                  parseRelation(get<int>(*vec.front(), clingo_ast_attribute_comparison)),
                                  parseTerm(*lhs));
        for (auto it = vec.begin() + 1, ie = vec.end(); it != ie; ++it) {
            auto rhs = get<SAST>(**it, clingo_ast_attribute_term);
            ret = prg_.rellitvec(get<Location>(*rhs, clingo_ast_attribute_location),
                                 ret,
                                 parseRelation(get<int>(**it, clingo_ast_attribute_comparison)),
                                 parseTerm(*rhs));
        }
        return ret;
    }

    LitUid parseLiteral(AST const &ast) {
        switch (ast.type()) {
            case clingo_ast_type_literal: {
                auto loc = get<Location>(ast, clingo_ast_attribute_location);
                auto sign = parseSign(get<int>(ast, clingo_ast_attribute_sign));
                auto const &atom = *get<SAST>(ast, clingo_ast_attribute_atom);

                switch (atom.type()) {
                    case clingo_ast_type_boolean_constant: {
                        int cmp = sign == NAF::NOT ? 1 : 0;
                        return prg_.boollit(loc,
                                            get<int>(atom, clingo_ast_attribute_value) != cmp);
                    }
                    case clingo_ast_type_symbolic_atom: {
                        return prg_.predlit(loc,
                                            sign,
                                            parseAtom(*get<SAST>(ast, clingo_ast_attribute_atom)));
                    }
                    case clingo_ast_type_comparison: {
                        return prg_.rellit(loc,
                                           sign,
                                           parseTerm(*get<SAST>(atom, clingo_ast_attribute_term)),
                                           parseRightGuards(get<AST::ASTVec>(atom, clingo_ast_attribute_guards)));
                    }
                    default: {
                        throw std::runtime_error("invalid ast: atom expected");
                    }
                }
            }
            default: {
                throw std::runtime_error("invalid ast: (CSP) literal expected");
            }
        }
    }

    LitVecUid parseLiteralVec(AST::ASTVec const &asts) {
        auto uid = prg_.litvec();
        for (auto const &ast : asts) {
            uid = prg_.litvec(uid, parseLiteral(*ast));
        }
        return uid;
    }

    // {{{1 aggregates

    static AggregateFunction parseAggregateFunction(int fun) {
        switch (fun) {
            case clingo_ast_aggregate_function_count: {
                return AggregateFunction::COUNT;
            }
            case clingo_ast_aggregate_function_sum: {
                return AggregateFunction::SUM;
            }
            case clingo_ast_aggregate_function_sump: {
                return AggregateFunction::SUMP;
            }
            case clingo_ast_aggregate_function_min: {
                return AggregateFunction::MIN;
            }
            case clingo_ast_aggregate_function_max: {
                return AggregateFunction::MAX;
            }
            default: {
                throw std::runtime_error("invalid ast: invalid aggregate function");
            }
        };
    }

    BoundVecUid parseBounds(AST const &ast) {
        auto ret = prg_.boundvec();
        auto const *right = getOpt(ast, clingo_ast_attribute_right_guard);
        if (right != nullptr) {
            ret = prg_.boundvec(ret,
                                parseRelation(get<int>(*right, clingo_ast_attribute_comparison)),
                                parseTerm(*get<SAST>(*right, clingo_ast_attribute_term)));
        }
        auto const *left = getOpt(ast, clingo_ast_attribute_left_guard);
        if (left != nullptr) {
            ret = prg_.boundvec(ret,
                                inv(parseRelation(get<int>(*left, clingo_ast_attribute_comparison))),
                                parseTerm(*get<SAST>(*left, clingo_ast_attribute_term)));
        }
        return ret;
    }

    CondLitVecUid parseCondLitVec(AST::ASTVec const &asts) {
        auto uid = prg_.condlitvec();
        for (auto const &ast : asts) {
            uid = prg_.condlitvec(uid,
                                  parseLiteral(*get<SAST>(*ast, clingo_ast_attribute_literal)),
                                  parseLiteralVec(get<AST::ASTVec>(*ast, clingo_ast_attribute_condition)));
        }
        return uid;
    }

    HdAggrElemVecUid parseHdAggrElemVec(AST::ASTVec const &asts) {
        auto uid = prg_.headaggrelemvec();
        for (auto const &ast : asts) {
            require_(ast->type() == clingo_ast_type_head_aggregate_element, "invalid ast: head aggregate element expected");
            auto const &clit = get<SAST>(*ast, clingo_ast_attribute_condition);
            require_(clit->type() == clingo_ast_type_conditional_literal, "invalid ast: conditional literal expected");
            uid = prg_.headaggrelemvec(uid,
                                       parseTermVec(get<AST::ASTVec>(*ast, clingo_ast_attribute_terms)),
                                       parseLiteral(*get<SAST>(*clit, clingo_ast_attribute_literal)),
                                       parseLiteralVec(get<AST::ASTVec>(*clit, clingo_ast_attribute_condition)));
        }
        return uid;
    }

    BdAggrElemVecUid parseBdAggrElemVec(AST::ASTVec const &asts) {
        auto uid = prg_.bodyaggrelemvec();
        for (auto const &ast : asts) {
            require_(ast->type() == clingo_ast_type_body_aggregate_element, "invalid ast: body aggregate element expected");
            uid = prg_.bodyaggrelemvec(uid,
                                       parseTermVec(get<AST::ASTVec>(*ast, clingo_ast_attribute_terms)),
                                       parseLiteralVec(get<AST::ASTVec>(*ast, clingo_ast_attribute_condition)));
        }
        return uid;
    }

    TheoryElemVecUid parseTheoryElemVec(AST::ASTVec const &asts) {
        auto uid = prg_.theoryelems();
        for (auto const &ast : asts) {
            uid = prg_.theoryelems(uid,
                                   parseTheoryOptermVec(get<AST::ASTVec>(*ast, clingo_ast_attribute_terms)),
                                   parseLiteralVec(get<AST::ASTVec>(*ast, clingo_ast_attribute_condition)));
        }
        return uid;
    }

    TheoryAtomUid parseTheoryAtom(AST const &ast) {
        require_(ast.type() == clingo_ast_type_theory_atom, "invalid ast: theory atom expected");
        auto const &loc = get<Location>(ast, clingo_ast_attribute_location);
        auto const *guard = getOpt(ast, clingo_ast_attribute_guard);
        auto term = parseTerm(*get<SAST>(ast, clingo_ast_attribute_term));
        auto elements = parseTheoryElemVec(get<AST::ASTVec>(ast, clingo_ast_attribute_elements));
        return guard != nullptr
            ? prg_.theoryatom(term,
                              elements,
                              get<String>(*guard, clingo_ast_attribute_operator_name),
                              loc,
                              parseTheoryOpterm(*get<SAST>(*guard, clingo_ast_attribute_term)))
            : prg_.theoryatom(term, elements);

    }

    // {{{1 heads

    HdLitUid parseHeadLiteral(AST const &ast) {
        switch (ast.type()) {
            case clingo_ast_type_literal: {
                return prg_.headlit(parseLiteral(ast));
            }
            case clingo_ast_type_disjunction: {
                return prg_.disjunction(get<Location>(ast, clingo_ast_attribute_location),
                                        parseCondLitVec(get<AST::ASTVec>(ast, clingo_ast_attribute_elements)));
            }
            case clingo_ast_type_aggregate: {
                return prg_.headaggr(get<Location>(ast, clingo_ast_attribute_location),
                                     AggregateFunction::COUNT,
                                     parseBounds(ast),
                                     parseCondLitVec(get<AST::ASTVec>(ast, clingo_ast_attribute_elements)));
            }
            case clingo_ast_type_head_aggregate: {
                return prg_.headaggr(get<Location>(ast, clingo_ast_attribute_location),
                                     parseAggregateFunction(get<int>(ast, clingo_ast_attribute_function)),
                                     parseBounds(ast),
                                     parseHdAggrElemVec(get<AST::ASTVec>(ast, clingo_ast_attribute_elements)));
            }
            case clingo_ast_type_theory_atom: {
                auto const &loc = get<Location>(ast, clingo_ast_attribute_location);
                return prg_.headaggr(loc, parseTheoryAtom(ast));
            }
            default: {
                throw std::runtime_error("invalid ast: head literal expected");
            }
        }
    }

    // {{{1 bodies

    BdLitVecUid parseBodyLiteralVec(AST::ASTVec const &asts) {
        auto uid = prg_.body();
        for (auto const &lit : asts) {
            switch (lit->type()) {
                case clingo_ast_type_literal: {
                    auto const &loc = get<Location>(*lit, clingo_ast_attribute_location);
                    auto sign = parseSign(get<int>(*lit, clingo_ast_attribute_sign));
                    auto const &atom = *get<SAST>(*lit, clingo_ast_attribute_atom);
                    switch (atom.type()) {
                        case clingo_ast_type_aggregate: {
                            uid = prg_.bodyaggr(uid, loc, sign, AggregateFunction::COUNT,
                                                parseBounds(atom),
                                                parseCondLitVec(get<AST::ASTVec>(atom, clingo_ast_attribute_elements)));
                            break;
                        }
                        case clingo_ast_type_body_aggregate: {
                            uid = prg_.bodyaggr(uid, loc, sign,
                                                parseAggregateFunction(get<int>(atom, clingo_ast_attribute_function)),
                                                parseBounds(atom),
                                                parseBdAggrElemVec(get<AST::ASTVec>(atom, clingo_ast_attribute_elements)));
                            break;
                        }
                        case clingo_ast_type_theory_atom: {
                            uid = prg_.bodyaggr(uid, loc, sign, parseTheoryAtom(atom));
                            break;
                        }
                        default: {
                            uid = prg_.bodylit(uid, parseLiteral(*lit));
                            break;
                        }
                    }
                    break;
                }
                case clingo_ast_type_conditional_literal: {
                    uid = prg_.conjunction(uid,
                                           get<Location>(*lit, clingo_ast_attribute_location),
                                           parseLiteral(*get<SAST>(*lit, clingo_ast_attribute_literal)),
                                           parseLiteralVec(get<AST::ASTVec>(*lit, clingo_ast_attribute_condition)));
                    break;
                }
                default: {
                    throw std::runtime_error("invalid ast: body literal expected");
                }
            }
        }
        return uid;
    }

    // {{{1 theory definitions

    static TheoryOperatorType parseTheoryOperatorType(int num) {
        switch (num) {
            case clingo_ast_theory_operator_type_unary: {
                return TheoryOperatorType::Unary;
            }
            case clingo_ast_theory_operator_type_binary_left: {
                return TheoryOperatorType::BinaryLeft;
            }
            case clingo_ast_theory_operator_type_binary_right: {
                return TheoryOperatorType::BinaryRight;
            }
            default: {
                throw std::runtime_error("invalid ast: invalid theory operator type");
            }
        }
    }

    TheoryOpDefUid parseTheoryOpDef(AST const &ast) {
        require_(ast.type() == clingo_ast_type_theory_operator_definition, "invalid ast: theory operator definition expected");
        return prg_.theoryopdef(get<Location>(ast, clingo_ast_attribute_location),
                                get<String>(ast, clingo_ast_attribute_name),
                                get<int>(ast, clingo_ast_attribute_priority),
                                parseTheoryOperatorType(get<int>(ast, clingo_ast_attribute_operator_type)));
    }

    TheoryOpDefVecUid parseTheoryOpDefVec(AST::ASTVec const &asts) {
        auto uid = prg_.theoryopdefs();
        for (auto const &ast : asts) {
            prg_.theoryopdefs(uid, parseTheoryOpDef(*ast));
        }
        return uid;
    }

    static TheoryAtomType parseTheoryAtomType(int num) {
        switch (num) {
            case clingo_ast_theory_atom_definition_type_head: {
                return TheoryAtomType::Head;
            }
            case clingo_ast_theory_atom_definition_type_body: {
                return TheoryAtomType::Body;
            }
            case clingo_ast_theory_atom_definition_type_any: {
                return TheoryAtomType::Any;
            }
            case clingo_ast_theory_atom_definition_type_directive: {
                return TheoryAtomType::Directive;
            }
            default: {
                throw std::runtime_error("invalid ast: invalid theory atom type");
            }
        };
    }

    TheoryAtomDefUid parseTheoryAtomDefinition(AST const &ast) {
        require_(ast.type() == clingo_ast_type_theory_atom_definition, "invalid ast: theory atom definition expected");
        auto const *guard = getOpt(ast, clingo_ast_attribute_guard);
        auto const &loc = get<Location>(ast, clingo_ast_attribute_location);
        auto name = get<String>(ast, clingo_ast_attribute_name);
        auto arity = get<int>(ast, clingo_ast_attribute_arity);
        auto term = get<String>(ast, clingo_ast_attribute_term);
        auto type = parseTheoryAtomType(get<int>(ast, clingo_ast_attribute_atom_type));
        return guard != nullptr
            ? prg_.theoryatomdef(loc, name, arity, term, type,
                                 parseTheoryOpVec(get<AST::StrVec>(*guard, clingo_ast_attribute_operators)),
                                 get<String>(*guard, clingo_ast_attribute_term))
            : prg_.theoryatomdef(loc, name, arity, term, type);
    }

    TheoryDefVecUid parseTheoryAtomDefinitionVec(TheoryDefVecUid uid, AST::ASTVec const &asts) {
        for (auto const &ast : asts) {
            prg_.theorydefs(uid, parseTheoryAtomDefinition(*ast));
        }
        return uid;
    }

    TheoryTermDefUid parseTheoryTermDefinition(AST const &ast) {
        return prg_.theorytermdef(get<Location>(ast, clingo_ast_attribute_location),
                                  get<String>(ast, clingo_ast_attribute_name),
                                  parseTheoryOpDefVec(get<AST::ASTVec>(ast, clingo_ast_attribute_operators)),
                                  log_);
    }

    TheoryDefVecUid parseTheoryTermDefinitionVec(AST::ASTVec const &asts) {
        auto uid = prg_.theorydefs();
        for (auto const &ast : asts) {
            prg_.theorydefs(uid, parseTheoryTermDefinition(*ast));
        }
        return uid;
    }

    // {{{1 comments

    static clingo_comment_type_t parseCommentType(int num) {
        switch (num) {
            case clingo_comment_type_line: {
                return clingo_comment_type_line;
            }
            case clingo_comment_type_block: {
                return clingo_comment_type_block;
            }
            default: {
                throw std::runtime_error("invalid ast: invalid comment type");
            }
        }
    }
    // 1}}}

    Logger &log_;
    INongroundProgramBuilder &prg_;
};

} // namespace

void parse(INongroundProgramBuilder &prg, Logger &log, AST const &ast) {
    ASTParser{log, prg}.parseStatement(ast);
}

} } // namespace Input Gringo
