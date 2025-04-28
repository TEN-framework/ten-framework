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

tl::optional<std::vector<OAST>> unpool(OAST &ast, clingo_ast_unpool_type_bitset_t type=clingo_ast_unpool_type_all) {
    if (ast.ast.get() == nullptr) {
        return {};
    }
    auto pool = unpool(ast.ast, type);
    if (!pool.has_value()) {
        return {};
    }
    std::vector<OAST> ret;
    for (auto &unpooled : *pool) {
        ret.emplace_back(OAST{std::move(unpooled)});
    }
    return ret;
}

tl::optional<std::vector<AST::ASTVec>> unpool(AST::ASTVec &vec, clingo_ast_unpool_type_bitset_t type=clingo_ast_unpool_type_all) {
    // unpool the elements of the vector
    bool has_pool = false;
    std::vector<tl::optional<AST::ASTVec>> pools;
    for (auto &ast : vec) {
        pools.emplace_back(unpool(ast, type));
        if (pools.back().has_value()) {
            has_pool = true;
            if (pools.back()->empty()) {
                return std::vector<AST::ASTVec>{};
            }
        }
    }
    if (!has_pool) {
        return {};
    }
    // compute size of cross product
    size_t size = 1;
    auto it = vec.begin();
    for (auto &pool : pools) {
        if (pool.has_value()) {
            size *= pool->size();
        }
        else {
            pool = AST::ASTVec{*it};
        }
        ++it;
    }
    // compute cross product
    std::vector<AST::ASTVec> product;
    product.reserve(size);
    product.emplace_back();
    for (auto &pool : pools) {
        auto jt = product.begin();
        for (size_t i = 0, e = product.size(); i != e; ++i, ++jt) {
            assert(!pool->empty());
            auto kt = pool->begin();
            auto ke = pool->end();
            for (++kt; kt != ke; ++kt) {
                product.emplace_back(*jt);
                product.back().emplace_back(*kt);
            }
            jt->emplace_back(*pool->begin());
        }
    }
    return product;
}

template <int i, bool cond>
struct unpool_cross_ {
    template <class V, class... Args>
    static void apply_(V &val, tl::optional<AST::ASTVec> &ret, AST &ast, clingo_ast_attribute_e name, Args&&... args) {
        auto pool = cond ? unpool(val, clingo_ast_unpool_type_other) : unpool(val);
        if (!pool.has_value()) {
            unpool_cross_<i-1, cond>::apply(ret, ast, std::forward<Args>(args)..., name, AST::Value{val});
        }
        else {
            if (!ret.has_value()) {
                ret = AST::ASTVec{};
            }
            for (auto &unpooled : *pool) {
                unpool_cross_<i-1, cond>::apply(ret, ast, args..., name, AST::Value{std::move(unpooled)});
            }
        }
    }
    template <class... Args>
    static void apply(tl::optional<AST::ASTVec> &ret, AST &ast, clingo_ast_attribute_e name, Args&&... args) {
        auto &val = ast.value(name);
        if (mpark::holds_alternative<SAST>(val)) {
            assert(mpark::get<SAST>(val).get() != nullptr);
            apply_(mpark::get<SAST>(val), ret, ast, name, std::forward<Args>(args)...);
        }
        else if (mpark::holds_alternative<OAST>(val)) {
            apply_(mpark::get<OAST>(val), ret, ast, name, std::forward<Args>(args)...);
        }
        else if (mpark::holds_alternative<AST::ASTVec>(val)) {
            apply_(mpark::get<AST::ASTVec>(val), ret, ast, name, std::forward<Args>(args)...);
        }
        else {
            assert(false);
        }
    }
};

template <bool cond>
struct unpool_cross_<0, cond> {
    template <class... Args>
    static void apply(tl::optional<AST::ASTVec> &ret, AST &ast, Args&&... args) {
        if (ret.has_value()) {
            ret->emplace_back(ast.update(std::forward<Args>(args)...));
        }
    }
};

template <class... Args>
tl::optional<AST::ASTVec> unpool_cross(AST &ast, Args&&... args) {
    tl::optional<AST::ASTVec> ret;
    unpool_cross_<sizeof...(Args), false>::apply(ret, ast, std::forward<Args>(args)...);
    return ret;
}

template <bool cond=false>
tl::optional<SAST> unpool_chain(AST &ast, clingo_ast_attribute_e name) {
    auto &val = mpark::get<AST::ASTVec>(ast.value(name));
    AST::ASTVec chain;
    chain.reserve(val.size());
    bool has_pool = false;
    for (auto &elem : val) {
        auto pool = cond ? unpool(elem, clingo_ast_unpool_type_condition) : unpool(elem);
        if (pool.has_value()) {
            has_pool = true;
            std::move(pool->begin(), pool->end(), std::back_inserter(chain));
        }
        else {
            chain.emplace_back(elem);
        }
    }
    if (has_pool) {
        return ast.update(name, AST::Value{std::move(chain)});
    }
    return {};
}

template <bool cond, class... Args>
tl::optional<AST::ASTVec> unpool_chain_cross(AST &ast, clingo_ast_attribute_e name, Args&&... args) {
    auto chain = unpool_chain<cond>(ast, name);
    tl::optional<AST::ASTVec> ret;
    if (chain.has_value()) {
        ret = AST::ASTVec{};
        unpool_cross_<sizeof...(Args), cond>::apply(ret, **chain, std::forward<Args>(args)...);
    }
    else {
        unpool_cross_<sizeof...(Args), cond>::apply(ret, ast, std::forward<Args>(args)...);
    }
    return ret;
}

} // namespace

tl::optional<AST::ASTVec> unpool_condition(SAST &ast) {
    if (ast->type() == clingo_ast_type_conditional_literal) {
        return unpool_cross(*ast, clingo_ast_attribute_condition);
    }
    return {};
}

tl::optional<AST::ASTVec> unpool(SAST &ast, clingo_ast_unpool_type_bitset_t type) {
    if ((type & clingo_ast_unpool_type_other) == 0 && ast->type() != clingo_ast_type_conditional_literal) {
        return {};
    }
    switch (ast->type()) {
        case clingo_ast_type_theory_sequence:
        case clingo_ast_type_theory_function:
        case clingo_ast_type_theory_unparsed_term_element:
        case clingo_ast_type_theory_unparsed_term:
        case clingo_ast_type_theory_guard:
        case clingo_ast_type_script:
        case clingo_ast_type_program:
        case clingo_ast_type_project_signature:
        case clingo_ast_type_definition:
        case clingo_ast_type_show_signature:
        case clingo_ast_type_defined:
        case clingo_ast_type_theory_definition:
        case clingo_ast_type_theory_operator_definition:
        case clingo_ast_type_theory_term_definition:
        case clingo_ast_type_theory_guard_definition:
        case clingo_ast_type_theory_atom_definition:
        case clingo_ast_type_boolean_constant:
        case clingo_ast_type_id:
        case clingo_ast_type_variable:
        case clingo_ast_type_symbolic_term:
        case clingo_ast_type_comment: {
            return {};
        }
        case clingo_ast_type_unary_operation: {
            return unpool_cross(*ast, clingo_ast_attribute_argument);
        }
        case clingo_ast_type_comparison: {
            return unpool_cross(*ast, clingo_ast_attribute_term, clingo_ast_attribute_guards);
        }
        case clingo_ast_type_interval:
        case clingo_ast_type_binary_operation: {
            return unpool_cross(*ast, clingo_ast_attribute_left, clingo_ast_attribute_right);
        }
        case clingo_ast_type_function: {
            return unpool_cross(*ast, clingo_ast_attribute_arguments);
        }
        case clingo_ast_type_pool: {
            auto ret = AST::ASTVec{};
            for (auto &term : mpark::get<AST::ASTVec>(ast->value(clingo_ast_attribute_arguments))) {
                auto pool = unpool(term);
                if (pool.has_value()) {
                    std::move(pool->begin(), pool->end(), std::back_inserter(ret));
                }
                else {
                    ret.emplace_back(term);
                }
            }
            return ret;
        }
        case clingo_ast_type_symbolic_atom: {
            return unpool_cross(*ast, clingo_ast_attribute_symbol);
        }
        case clingo_ast_type_guard: {
            return unpool_cross(*ast, clingo_ast_attribute_term);
        }
        case clingo_ast_type_conditional_literal: {
            if ((type & clingo_ast_unpool_type_all) == clingo_ast_unpool_type_all) {
                return unpool_cross(*ast, clingo_ast_attribute_literal, clingo_ast_attribute_condition);
            }
            if ((type & clingo_ast_unpool_type_other) != 0) {
                return unpool_cross(*ast, clingo_ast_attribute_literal);
            }
            if ((type & clingo_ast_unpool_type_condition) != 0) {
                return unpool_cross(*ast, clingo_ast_attribute_condition);
            }
            return {};
        }
        case clingo_ast_type_body_aggregate_element:
        case clingo_ast_type_head_aggregate_element: {
            return unpool_cross(*ast, clingo_ast_attribute_terms, clingo_ast_attribute_condition);
        }
        case clingo_ast_type_body_aggregate:
        case clingo_ast_type_head_aggregate:
        case clingo_ast_type_aggregate: {
            return unpool_chain_cross<false>(*ast, clingo_ast_attribute_elements, clingo_ast_attribute_left_guard, clingo_ast_attribute_right_guard);
        }
        case clingo_ast_type_disjunction: {
            return unpool_chain_cross<true>(*ast, clingo_ast_attribute_elements, clingo_ast_attribute_elements);
        }
        case clingo_ast_type_theory_atom_element: {
            return unpool_cross(*ast, clingo_ast_attribute_condition);
        }
        case clingo_ast_type_theory_atom: {
            return unpool_chain_cross<false>(*ast, clingo_ast_attribute_elements, clingo_ast_attribute_term);
        }
        case clingo_ast_type_literal: {
            return unpool_cross(*ast, clingo_ast_attribute_atom);
        }
        case clingo_ast_type_rule: {
            return unpool_chain_cross<true>(*ast, clingo_ast_attribute_body, clingo_ast_attribute_body, clingo_ast_attribute_head);
        }
        case clingo_ast_type_show_term: {
            return unpool_chain_cross<true>(*ast, clingo_ast_attribute_body, clingo_ast_attribute_body, clingo_ast_attribute_term);
        }
        case clingo_ast_type_minimize: {
            return unpool_chain_cross<true>(*ast, clingo_ast_attribute_body, clingo_ast_attribute_body, clingo_ast_attribute_weight, clingo_ast_attribute_priority, clingo_ast_attribute_terms);
        }
        case clingo_ast_type_external: {
            return unpool_chain_cross<true>(*ast, clingo_ast_attribute_body, clingo_ast_attribute_body, clingo_ast_attribute_atom, clingo_ast_attribute_external_type);
        }
        case clingo_ast_type_edge: {
            return unpool_chain_cross<true>(*ast, clingo_ast_attribute_body, clingo_ast_attribute_body, clingo_ast_attribute_node_u, clingo_ast_attribute_node_v);
        }
        case clingo_ast_type_heuristic: {
            return unpool_chain_cross<true>(*ast, clingo_ast_attribute_body, clingo_ast_attribute_body, clingo_ast_attribute_atom, clingo_ast_attribute_bias, clingo_ast_attribute_priority, clingo_ast_attribute_modifier);
        }
        case clingo_ast_type_project_atom: {
            break;
        }
    }
    return unpool_chain_cross<true>(*ast, clingo_ast_attribute_body, clingo_ast_attribute_body, clingo_ast_attribute_atom);
}

} } //namespace Input Gringo
