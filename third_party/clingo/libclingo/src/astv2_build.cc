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

class ast {
public:
    ast(clingo_ast_type_e type, Location const &loc)
    : ast_{SAST{type}} {
        set(clingo_ast_attribute_location, loc);
    }
    ast(clingo_ast_type_e type)
    : ast_{SAST{type}} { }
    template <typename T>
    ast &set(clingo_ast_attribute_e name, T &&value) {
        ast_->value(name, std::forward<T>(value));
        return *this;
    }
    SAST move() {
        return std::move(ast_);
    }
    operator SAST() {
        return std::move(ast_);
    }
private:
    SAST ast_;
};

class ASTBuilder : public Gringo::Input::INongroundProgramBuilder {
public:
    ASTBuilder(SASTCallback cb)
    : cb_{std::move(cb)} { }
    // {{{1 terms

    TermUid term(Location const &loc, Symbol val) override {
        return terms_.insert(ast(clingo_ast_type_symbolic_term, loc)
            .set(clingo_ast_attribute_symbol, val));
    }

    TermUid term(Location const &loc, String name) override {
        return terms_.insert(ast(clingo_ast_type_variable, loc)
            .set(clingo_ast_attribute_name, name));
    }

    TermUid term(Location const &loc, UnOp op, TermUid a) override {
        return terms_.insert(ast(clingo_ast_type_unary_operation, loc)
            .set(clingo_ast_attribute_operator_type, static_cast<int>(op))
            .set(clingo_ast_attribute_argument, terms_.erase(a)));
    }

    TermUid pool_(Location const &loc, AST::ASTVec vec) {
        if (vec.size() == 1) {
            return terms_.insert(std::move(vec.front()));
        }
        return terms_.insert(ast(clingo_ast_type_pool, loc)
            .set(clingo_ast_attribute_arguments, std::move(vec)));
    }

    TermUid term(Location const &loc, UnOp op, TermVecUid a) override {
        return term(loc, op, pool_(loc, termvecs_.erase(a)));
    }

    TermUid term(Location const &loc, BinOp op, TermUid a, TermUid b) override {
        return terms_.insert(ast(clingo_ast_type_binary_operation, loc)
            .set(clingo_ast_attribute_operator_type, static_cast<int>(op))
            .set(clingo_ast_attribute_left, terms_.erase(a))
            .set(clingo_ast_attribute_right, terms_.erase(b)));
    }

    TermUid term(Location const &loc, TermUid a, TermUid b) override {
        return terms_.insert(ast(clingo_ast_type_interval, loc)
            .set(clingo_ast_attribute_left, terms_.erase(a))
            .set(clingo_ast_attribute_right, terms_.erase(b)));
    }

    static SAST fun_(Location const &loc, String name, AST::ASTVec args, bool external) {
        return ast(clingo_ast_type_function, loc)
            .set(clingo_ast_attribute_name, name)
            .set(clingo_ast_attribute_arguments, std::move(args))
            .set(clingo_ast_attribute_external, static_cast<int>(external));
    }

    TermUid term(Location const &loc, String name, TermVecVecUid a, bool lua) override {
        AST::ASTVec pool;
        for (auto &args : termvecvecs_.erase(a)) {
            pool.emplace_back(fun_(loc, name, std::move(args), lua));
        }
        return pool_(loc, std::move(pool));
    }

    TermUid term(Location const &loc, TermVecUid a, bool forceTuple) override {
        return terms_.insert((termvecs_[a].size() == 1 && !forceTuple)
            ? std::move(termvecs_.erase(a).front())
            : fun_(loc, "", termvecs_.erase(a), false));
    }

    TermUid pool(Location const &loc, TermVecUid a) override {
        return pool_(loc, termvecs_.erase(a));
    }

    IdVecUid idvec() override {
        return idvecs_.emplace();
    }

    IdVecUid idvec(IdVecUid uid, Location const &loc, String id) override {
        idvecs_[uid].emplace_back(ast(clingo_ast_type_id, loc)
            .set(clingo_ast_attribute_name, id));
        return uid;
    }

    TermVecUid termvec() override {
        return termvecs_.emplace();
    }

    TermVecUid termvec(TermVecUid uid, TermUid termUid) override {
        termvecs_[uid].emplace_back(terms_.erase(termUid));
        return uid;
    }

    TermVecVecUid termvecvec() override {
        return termvecvecs_.emplace();
    }

    TermVecVecUid termvecvec(TermVecVecUid uid, TermVecUid termvecUid) override {
        termvecvecs_[uid].emplace_back(termvecs_.erase(termvecUid));
        return uid;
    }

    // {{{1 literals

    SAST symbolicatom(TermUid termUid) {
        return ast(clingo_ast_type_symbolic_atom)
                .set(clingo_ast_attribute_symbol, terms_.erase(termUid));
    }

    LitUid boollit(Location const &loc, bool type) override {
        return lits_.insert(ast(clingo_ast_type_literal, loc)
            .set(clingo_ast_attribute_sign, static_cast<int>(clingo_ast_sign_no_sign))
            .set(clingo_ast_attribute_atom, ast(clingo_ast_type_boolean_constant)
                .set(clingo_ast_attribute_value, static_cast<int>(type))));
    }

    LitUid predlit(Location const &loc, NAF naf, TermUid termUid) override {
        return lits_.insert(ast(clingo_ast_type_literal, loc)
            .set(clingo_ast_attribute_sign, static_cast<int>(naf))
            .set(clingo_ast_attribute_atom, symbolicatom(termUid)));
    }

    RelLitVecUid rellitvec(Location const &loc, Relation rel, TermUid termUidLeft) override {
        auto id = guardvecs_.emplace();
        return rellitvec(loc, id, rel, termUidLeft);
    }

    RelLitVecUid rellitvec(Location const &loc, RelLitVecUid vecUidLeft, Relation rel, TermUid termUidRight) override {
        static_cast<void>(loc);
        guardvecs_[vecUidLeft].emplace_back(ast(clingo_ast_type_guard)
            .set(clingo_ast_attribute_comparison, static_cast<int>(rel))
            .set(clingo_ast_attribute_term, terms_.erase(termUidRight)));
        return vecUidLeft;
    }

    LitUid rellit(Location const &loc, NAF naf, TermUid termUidLeft, RelLitVecUid vecUidRight) override {
        return lits_.insert(ast(clingo_ast_type_literal, loc)
            .set(clingo_ast_attribute_sign, static_cast<int>(naf))
            .set(clingo_ast_attribute_atom, ast(clingo_ast_type_comparison)
                .set(clingo_ast_attribute_term, terms_.erase(termUidLeft))
                .set(clingo_ast_attribute_guards, guardvecs_.erase(vecUidRight))));
    }

    LitVecUid litvec() override {
        return litvecs_.emplace();
    }

    LitVecUid litvec(LitVecUid uid, LitUid literalUid) override {
        litvecs_[uid].emplace_back(lits_.erase(literalUid));
        return uid;
    }

    // {{{1 aggregates

    CondLitVecUid condlitvec() override {
        return condlitvecs_.emplace();
    }

    SAST condlit(LitUid litUid, LitVecUid litvecUid, Location const *oloc=nullptr) {
        auto const &loc = oloc != nullptr
            ? *oloc
            : get<Location>(*lits_[litUid], clingo_ast_attribute_location);
        return ast(clingo_ast_type_conditional_literal, loc)
            .set(clingo_ast_attribute_literal, lits_.erase(litUid))
            .set(clingo_ast_attribute_condition, litvecs_.erase(litvecUid));
    }

    CondLitVecUid condlitvec(CondLitVecUid uid, LitUid litUid, LitVecUid litvecUid) override {
        condlitvecs_[uid].emplace_back(condlit(litUid, litvecUid));
        return uid;
    }

    BdAggrElemVecUid bodyaggrelemvec() override {
        return bdaggrelemvecs_.emplace();
    }

    BdAggrElemVecUid bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) override {
        bdaggrelemvecs_[uid].emplace_back(ast(clingo_ast_type_body_aggregate_element)
            .set(clingo_ast_attribute_terms, termvecs_.erase(termvec))
            .set(clingo_ast_attribute_condition, litvecs_.erase(litvec)));
        return uid;
    }

    HdAggrElemVecUid headaggrelemvec() override {
        return hdaggrelemvecs_.emplace();
    }

    HdAggrElemVecUid headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid litUid, LitVecUid litvec) override {
        hdaggrelemvecs_[uid].emplace_back(ast(clingo_ast_type_head_aggregate_element)
            .set(clingo_ast_attribute_terms, termvecs_.erase(termvec))
            .set(clingo_ast_attribute_condition, condlit(litUid, litvec)));
        return uid;
    }

    BoundVecUid boundvec() override {
        return boundvecs_.emplace();
    }

    BoundVecUid boundvec(BoundVecUid uid, Relation rel, TermUid term) override {
        boundvecs_[uid].emplace_back(ast(clingo_ast_type_guard)
            .set(clingo_ast_attribute_comparison, static_cast<int>(rel))
            .set(clingo_ast_attribute_term, terms_.erase(term)));
        return uid;
    }

    // {{{1 heads

    HdLitUid headlit(LitUid litUid) override {
        return heads_.insert(lits_.erase(litUid));
    }

    HdLitUid headaggr(Location const &loc, TheoryAtomUid atomUid) override {
        return heads_.insert(theoryatoms_.erase(atomUid));
    }

    std::pair<AST::Value, AST::Value> guards_(BoundVecUid bounds) {
        AST::Value leftGuard = OAST{SAST{nullptr}};
        AST::Value rightGuard = OAST{SAST{nullptr}};
        auto guards = boundvecs_.erase(bounds);
        assert(guards.size() < 3);
        if (!guards.empty()) {
            auto &rel = get<int>(*guards.front(), clingo_ast_attribute_comparison);
            rel = static_cast<int>(inv(static_cast<Relation>(rel)));
            leftGuard = OAST{std::move(guards.front())};
        }
        if (guards.size() > 1) {
            rightGuard = OAST{std::move(guards.back())};
        }
        return {std::move(leftGuard), std::move(rightGuard)};
    }

    SAST aggr(Location const &loc, BoundVecUid bounds, CondLitVecUid elems) {
        auto guards = guards_(bounds);
        return ast(clingo_ast_type_aggregate, loc)
            .set(clingo_ast_attribute_left_guard, std::move(guards.first))
            .set(clingo_ast_attribute_elements, condlitvecs_.erase(elems))
            .set(clingo_ast_attribute_right_guard, std::move(guards.second));
    }

    HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, HdAggrElemVecUid headaggrelemvec) override {
        auto guards = guards_(bounds);
        return heads_.insert(ast(clingo_ast_type_head_aggregate, loc)
            .set(clingo_ast_attribute_left_guard, std::move(guards.first))
            .set(clingo_ast_attribute_function, static_cast<int>(fun))
            .set(clingo_ast_attribute_elements, hdaggrelemvecs_.erase(headaggrelemvec))
            .set(clingo_ast_attribute_right_guard, std::move(guards.second)));
    }

    HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid headaggrelemvec) override {
        (void)fun;
        assert(fun == AggregateFunction::COUNT);
        return heads_.insert(aggr(loc, bounds, headaggrelemvec));
    }

    HdLitUid disjunction(Location const &loc, CondLitVecUid condlitvec) override {
        return heads_.insert(ast(clingo_ast_type_disjunction, loc)
            .set(clingo_ast_attribute_elements, condlitvecs_.erase(condlitvec)));
    }

    // {{{1 bodies

    BdLitVecUid body() override {
        return bodylitvecs_.emplace();
    }

    BdLitVecUid bodylit(BdLitVecUid body, LitUid bodylit) override {
        bodylitvecs_[body].emplace_back(lits_.erase(bodylit));
        return body;
    }

    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, TheoryAtomUid atomUid) override {
        bodylitvecs_[body].emplace_back(ast(clingo_ast_type_literal, loc)
            .set(clingo_ast_attribute_sign, static_cast<int>(naf))
            .set(clingo_ast_attribute_atom, theoryatoms_.erase(atomUid)));
        return body;
    }

    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) override {
        auto guards = guards_(bounds);
        bodylitvecs_[body].emplace_back(ast(clingo_ast_type_literal, loc)
            .set(clingo_ast_attribute_sign, static_cast<int>(naf))
            .set(clingo_ast_attribute_atom, ast(clingo_ast_type_body_aggregate, loc)
                .set(clingo_ast_attribute_left_guard, std::move(guards.first))
                .set(clingo_ast_attribute_function, static_cast<int>(fun))
                .set(clingo_ast_attribute_elements, bdaggrelemvecs_.erase(bodyaggrelemvec))
                .set(clingo_ast_attribute_right_guard, std::move(guards.second))));
        return body;
    }

    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid bodyaggrelemvec) override {
        bodylitvecs_[body].emplace_back(ast(clingo_ast_type_literal, loc)
            .set(clingo_ast_attribute_sign, static_cast<int>(naf))
            .set(clingo_ast_attribute_atom, aggr(loc, bounds, bodyaggrelemvec)));
        return body;
    }

    BdLitVecUid conjunction(BdLitVecUid body, Location const &loc, LitUid head, LitVecUid litvec) override {
        bodylitvecs_[body].emplace_back(condlit(head, litvec, &loc));
        return body;
    }

    // {{{1 statements

    void rule(Location const &loc, HdLitUid head) override {
        rule(loc, head, body());
    }

    void rule(Location const &loc, HdLitUid head, BdLitVecUid body) override {
        cb_(ast(clingo_ast_type_rule, loc)
            .set(clingo_ast_attribute_head, heads_.erase(head))
            .set(clingo_ast_attribute_body, bodylitvecs_.erase(body)));
    }

    void define(Location const &loc, String name, TermUid value, bool defaultDef, Logger &logger) override {
        static_cast<void>(logger);
        cb_(ast(clingo_ast_type_definition, loc)
            .set(clingo_ast_attribute_name, name)
            .set(clingo_ast_attribute_value, terms_.erase(value))
            .set(clingo_ast_attribute_is_default, static_cast<int>(defaultDef)));
    }

    void optimize(Location const &loc, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) override {
        cb_(ast(clingo_ast_type_minimize, loc)
            .set(clingo_ast_attribute_weight, terms_.erase(weight))
            .set(clingo_ast_attribute_priority, terms_.erase(priority))
            .set(clingo_ast_attribute_terms, termvecs_.erase(cond))
            .set(clingo_ast_attribute_body, bodylitvecs_.erase(body)));
    }

    void showsig(Location const &loc, Sig sig) override {
        cb_(ast(clingo_ast_type_show_signature, loc)
            .set(clingo_ast_attribute_name, sig.name())
            .set(clingo_ast_attribute_arity, static_cast<int>(sig.arity()))
            .set(clingo_ast_attribute_positive, static_cast<int>(!sig.sign())));
    }

    void defined(Location const &loc, Sig sig) override {
        cb_(ast(clingo_ast_type_defined, loc)
            .set(clingo_ast_attribute_name, sig.name())
            .set(clingo_ast_attribute_arity, static_cast<int>(sig.arity()))
            .set(clingo_ast_attribute_positive, static_cast<int>(!sig.sign())));
    }

    void show(Location const &loc, TermUid t, BdLitVecUid body) override {
        cb_(ast(clingo_ast_type_show_term, loc)
            .set(clingo_ast_attribute_term, terms_.erase(t))
            .set(clingo_ast_attribute_body, bodylitvecs_.erase(body)));
    }

    void script(Location const &loc, String type, String code) override {
        cb_(ast(clingo_ast_type_script, loc)
            .set(clingo_ast_attribute_name, type)
            .set(clingo_ast_attribute_code, code));
    }

    void block(Location const &loc, String name, IdVecUid args) override {
        cb_(ast(clingo_ast_type_program, loc)
            .set(clingo_ast_attribute_name, name)
            .set(clingo_ast_attribute_parameters, idvecs_.erase(args)));
    }

    void external(Location const &loc, TermUid head, BdLitVecUid body, TermUid type) override {
        cb_(ast(clingo_ast_type_external, loc)
            .set(clingo_ast_attribute_atom, symbolicatom(head))
            .set(clingo_ast_attribute_body, bodylitvecs_.erase(body))
            .set(clingo_ast_attribute_external_type, terms_.erase(type)));
    }

    void edge(Location const &loc, TermVecVecUid edges, BdLitVecUid body) override {
        auto bd = bodylitvecs_.erase(body);
        for (auto &x : termvecvecs_.erase(edges)) {
            for (auto &x : bd) {
                if (x.use_count() > 1) {
                    x = x->deepcopy();
                }
            }
            assert(x.size() == 2);
            cb_(ast(clingo_ast_type_edge, loc)
                .set(clingo_ast_attribute_node_u, std::move(x.front()))
                .set(clingo_ast_attribute_node_v, std::move(x.back()))
                .set(clingo_ast_attribute_body, bd));
        }
    }

    void heuristic(Location const &loc, TermUid termUid, BdLitVecUid body, TermUid a, TermUid b, TermUid mod) override {
        cb_(ast(clingo_ast_type_heuristic, loc)
            .set(clingo_ast_attribute_atom, symbolicatom(termUid))
            .set(clingo_ast_attribute_body, bodylitvecs_.erase(body))
            .set(clingo_ast_attribute_bias, terms_.erase(a))
            .set(clingo_ast_attribute_priority, terms_.erase(b))
            .set(clingo_ast_attribute_modifier, terms_.erase(mod)));
    }

    void project(Location const &loc, TermUid termUid, BdLitVecUid body) override {
        cb_(ast(clingo_ast_type_project_atom, loc)
            .set(clingo_ast_attribute_atom, symbolicatom(termUid))
            .set(clingo_ast_attribute_body, bodylitvecs_.erase(body)));
    }

    void project(Location const &loc, Sig sig) override {
        cb_(ast(clingo_ast_type_project_signature, loc)
            .set(clingo_ast_attribute_name, sig.name())
            .set(clingo_ast_attribute_arity, static_cast<int>(sig.arity()))
            .set(clingo_ast_attribute_positive, static_cast<int>(!sig.sign())));
    }

    bool reportComment() const override {
        return true;
    }

    void comment(Location const &loc, String value, bool block) override {
        cb_(ast(clingo_ast_type_comment, loc)
            .set(clingo_ast_attribute_value, value)
            .set(clingo_ast_attribute_comment_type, block ? clingo_comment_type_block : clingo_comment_type_line));
    }

    // {{{1 theory atoms

    TheoryTermUid theorytermseq(Location const &loc, TheoryOptermVecUid args, clingo_ast_theory_sequence_type_e type) {
        return theoryterms_.insert(ast(clingo_ast_type_theory_sequence, loc)
            .set(clingo_ast_attribute_sequence_type, type)
            .set(clingo_ast_attribute_terms, theoryoptermvecs_.erase(args)));
    }

    TheoryTermUid theorytermset(Location const &loc, TheoryOptermVecUid args) override {
        return theorytermseq(loc, args, clingo_ast_theory_sequence_type_set);
    }

    TheoryTermUid theoryoptermlist(Location const &loc, TheoryOptermVecUid args) override {
        return theorytermseq(loc, args, clingo_ast_theory_sequence_type_list);
    }

    TheoryTermUid theorytermtuple(Location const &loc, TheoryOptermVecUid args) override {
        return theorytermseq(loc, args, clingo_ast_theory_sequence_type_tuple);
    }

    SAST unparsedterm(Location const &loc, TheoryOptermUid opterm) {
        auto elems = theoryopterms_.erase(opterm);
        if (elems.size() == 1 && get<AST::StrVec>(*elems.front(), clingo_ast_attribute_operators).empty()) {
            return std::move(get<SAST>(*elems.front(), clingo_ast_attribute_term));
        }
        return ast(clingo_ast_type_theory_unparsed_term, loc)
            .set(clingo_ast_attribute_elements, std::move(elems));
    }

    TheoryTermUid theorytermopterm(Location const &loc, TheoryOptermUid opterm) override {
        return theoryterms_.insert(unparsedterm(loc, opterm));
    }

    TheoryTermUid theorytermfun(Location const &loc, String name, TheoryOptermVecUid args) override {
        return theoryterms_.insert(ast(clingo_ast_type_theory_function, loc)
            .set(clingo_ast_attribute_name, name)
            .set(clingo_ast_attribute_arguments, theoryoptermvecs_.erase(args)));
    }

    TheoryTermUid theorytermvalue(Location const &loc, Symbol val) override {
        return theoryterms_.insert(ast(clingo_ast_type_symbolic_term, loc)
            .set(clingo_ast_attribute_symbol, val));
    }

    TheoryTermUid theorytermvar(Location const &loc, String var) override {
        return theoryterms_.insert(ast(clingo_ast_type_variable, loc)
            .set(clingo_ast_attribute_name, var));
    }

    SAST theoryunparsedelem(TheoryOpVecUid ops, TheoryTermUid term) {
        return ast(clingo_ast_type_theory_unparsed_term_element)
            .set(clingo_ast_attribute_operators, theoryopvecs_.erase(ops))
            .set(clingo_ast_attribute_term, theoryterms_.erase(term));
    }

    TheoryOptermUid theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) override {
        return theoryopterms_.insert({theoryunparsedelem(ops, term)});
    }

    TheoryOptermUid theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) override {
        theoryopterms_[opterm].emplace_back(theoryunparsedelem(ops, term));
        return opterm;
    }

    TheoryOpVecUid theoryops() override {
        return theoryopvecs_.emplace();
    }

    TheoryOpVecUid theoryops(TheoryOpVecUid ops, String op) override {
        theoryopvecs_[ops].emplace_back(op);
        return ops;
    }

    TheoryOptermVecUid theoryopterms() override {
        return theoryoptermvecs_.emplace();
    }

    TheoryOptermVecUid theoryopterms(TheoryOptermVecUid opterms, Location const &loc, TheoryOptermUid opterm) override {
        theoryoptermvecs_[opterms].emplace_back(unparsedterm(loc, opterm));
        return opterms;
    }

    TheoryOptermVecUid theoryopterms(Location const &loc, TheoryOptermUid opterm, TheoryOptermVecUid opterms) override {
        theoryoptermvecs_[opterms].emplace(theoryoptermvecs_[opterms].begin(), unparsedterm(loc, opterm));
        return opterms;
    }

    TheoryElemVecUid theoryelems() override {
        return theoryelemvecs_.emplace();
    }

    TheoryElemVecUid theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) override {
        theoryelemvecs_[elems].emplace_back(ast(clingo_ast_type_theory_atom_element)
            .set(clingo_ast_attribute_terms, theoryoptermvecs_.erase(opterms))
            .set(clingo_ast_attribute_condition, litvecs_.erase(cond)));
        return elems;
    }

    TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems) override {
        auto &loc = get<Location>(*terms_[term], clingo_ast_attribute_location);
        return theoryatoms_.insert(ast(clingo_ast_type_theory_atom, loc)
            .set(clingo_ast_attribute_term, terms_.erase(term))
            .set(clingo_ast_attribute_elements, theoryelemvecs_.erase(elems))
            .set(clingo_ast_attribute_guard, OAST{SAST{nullptr}}));
    }

    TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems, String op, Location const &loc, TheoryOptermUid opterm) override {
        auto &aloc = get<Location>(*terms_[term], clingo_ast_attribute_location);
        return theoryatoms_.insert(ast(clingo_ast_type_theory_atom, aloc)
            .set(clingo_ast_attribute_term, terms_.erase(term))
            .set(clingo_ast_attribute_elements, theoryelemvecs_.erase(elems))
            .set(clingo_ast_attribute_guard, OAST{ast(clingo_ast_type_theory_guard)
                .set(clingo_ast_attribute_operator_name, op)
                .set(clingo_ast_attribute_term, unparsedterm(loc, opterm))}));
    }

    // {{{1 theory definitions

    TheoryOpDefUid theoryopdef(Location const &loc, String op, unsigned priority, TheoryOperatorType type) override {
        return theoryopdefs_.insert(ast(clingo_ast_type_theory_operator_definition, loc)
            .set(clingo_ast_attribute_name, op)
            .set(clingo_ast_attribute_priority, static_cast<int>(priority))
            .set(clingo_ast_attribute_operator_type, static_cast<int>(type)));
    }

    TheoryOpDefVecUid theoryopdefs() override {
        return theoryopdefvecs_.emplace();
    }

    TheoryOpDefVecUid theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) override {
        theoryopdefvecs_[defs].emplace_back(theoryopdefs_.erase(def));
        return defs;
    }

    TheoryTermDefUid theorytermdef(Location const &loc, String name, TheoryOpDefVecUid defs, Logger &logger) override {
        static_cast<void>(logger);
        return theorytermdefs_.insert(ast(clingo_ast_type_theory_term_definition, loc)
            .set(clingo_ast_attribute_name, name)
            .set(clingo_ast_attribute_operators, theoryopdefvecs_.erase(defs)));
    }

    TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type, AST::Value guard) {
        return theoryatomdefs_.insert(ast(clingo_ast_type_theory_atom_definition, loc)
            .set(clingo_ast_attribute_atom_type, static_cast<int>(type))
            .set(clingo_ast_attribute_name, name)
            .set(clingo_ast_attribute_arity, static_cast<int>(arity))
            .set(clingo_ast_attribute_term, termDef)
            .set(clingo_ast_attribute_guard, std::move(guard)));
    }

    TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type) override {
        return theoryatomdef(loc, name, arity, termDef, type, OAST{SAST{nullptr}});
    }

    TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type, TheoryOpVecUid ops, String guardDef) override {
        return theoryatomdef(loc, name, arity, termDef, type, OAST{ast(clingo_ast_type_theory_guard_definition)
            .set(clingo_ast_attribute_operators, theoryopvecs_.erase(ops))
            .set(clingo_ast_attribute_term, guardDef)});
    }

    TheoryDefVecUid theorydefs() override {
        return theorydefvecs_.emplace();
    }

    TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) override {
        theorydefvecs_[defs].first.emplace_back(theorytermdefs_.erase(def));
        return defs;
    }

    TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) override {
        theorydefvecs_[defs].second.emplace_back(theoryatomdefs_.erase(def));
        return defs;
    }

    void theorydef(Location const &loc, String name, TheoryDefVecUid defs, Logger &logger) override {
        static_cast<void>(logger);
        auto x = theorydefvecs_.erase(defs);
        cb_(ast(clingo_ast_type_theory_definition, loc)
            .set(clingo_ast_attribute_name, name)
            .set(clingo_ast_attribute_terms, std::move(x.first))
            .set(clingo_ast_attribute_atoms, std::move(x.second)));
    }

    // 1}}}
private:
    SASTCallback cb_;
    Indexed<SAST, TermUid> terms_;
    Indexed<AST::ASTVec, TermVecUid> termvecs_;
    Indexed<std::vector<AST::ASTVec>, TermVecVecUid> termvecvecs_;
    Indexed<AST::ASTVec, IdVecUid> idvecs_;
    Indexed<SAST, LitUid> lits_;
    Indexed<AST::ASTVec, LitVecUid> litvecs_;
    Indexed<AST::ASTVec, CondLitVecUid> condlitvecs_;
    Indexed<AST::ASTVec, BdAggrElemVecUid> bdaggrelemvecs_;
    Indexed<AST::ASTVec, HdAggrElemVecUid> hdaggrelemvecs_;
    Indexed<AST::ASTVec, BoundVecUid> boundvecs_;
    Indexed<AST::ASTVec, RelLitVecUid> guardvecs_;
    Indexed<AST::ASTVec, BdLitVecUid> bodylitvecs_;
    Indexed<SAST, HdLitUid> heads_;
    Indexed<SAST, TheoryAtomUid> theoryatoms_;
    Indexed<SAST, TheoryTermUid> theoryterms_;
    Indexed<AST::ASTVec, TheoryOptermUid> theoryopterms_;
    Indexed<AST::ASTVec, TheoryOptermVecUid> theoryoptermvecs_;
    Indexed<AST::StrVec, TheoryOpVecUid> theoryopvecs_;
    Indexed<AST::ASTVec, TheoryElemVecUid> theoryelemvecs_;
    Indexed<SAST, TheoryOpDefUid> theoryopdefs_;
    Indexed<AST::ASTVec, TheoryOpDefVecUid> theoryopdefvecs_;
    Indexed<SAST, TheoryTermDefUid> theorytermdefs_;
    Indexed<SAST, TheoryAtomDefUid> theoryatomdefs_;
    Indexed<std::pair<AST::ASTVec, AST::ASTVec>, TheoryDefVecUid> theorydefvecs_;
};


} // namespace

std::unique_ptr<INongroundProgramBuilder> build(SASTCallback cb) {
    return std::make_unique<ASTBuilder>(std::move(cb));
}

} } // namespace Input Gringo
