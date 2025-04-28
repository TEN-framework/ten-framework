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

#define BEGIN_ENUM(name, type) \
char const *name(type x) { \
    switch (x) {
#define END_ENUM \
    } \
    return ""; \
}
#define CASE(value, string) \
        case value: { \
            return string; \
        }
// { {
#define END_DEFAULT(string) \
        default: { \
            break; \
        } \
    } \
    return string; \
}

BEGIN_ENUM(to_string_left, clingo_ast_unary_operator_e)
    CASE(clingo_ast_unary_operator_minus, "-")
    CASE(clingo_ast_unary_operator_absolute, "|")
    CASE(clingo_ast_unary_operator_negation, "~")
END_ENUM

BEGIN_ENUM(to_string_right, clingo_ast_unary_operator_e)
    CASE(clingo_ast_unary_operator_absolute, "|")
END_DEFAULT("")

BEGIN_ENUM(to_string, clingo_ast_binary_operator_e)
    CASE(clingo_ast_binary_operator_xor, "^")
    CASE(clingo_ast_binary_operator_or, "?")
    CASE(clingo_ast_binary_operator_and, "&")
    CASE(clingo_ast_binary_operator_plus, "+")
    CASE(clingo_ast_binary_operator_minus, "-")
    CASE(clingo_ast_binary_operator_multiplication, "*")
    CASE(clingo_ast_binary_operator_division, "/")
    CASE(clingo_ast_binary_operator_modulo, "\\")
    CASE(clingo_ast_binary_operator_power, "**")
END_ENUM

BEGIN_ENUM(to_string, clingo_ast_sign_e)
    CASE(clingo_ast_sign_no_sign, "")
    CASE(clingo_ast_sign_negation, "not ")
    CASE(clingo_ast_sign_double_negation, "not not ")
END_ENUM

BEGIN_ENUM(to_string, clingo_ast_comparison_operator_e)
    CASE(clingo_ast_comparison_operator_greater_than, ">")
    CASE(clingo_ast_comparison_operator_less_than, "<")
    CASE(clingo_ast_comparison_operator_less_equal, "<=")
    CASE(clingo_ast_comparison_operator_greater_equal, ">=")
    CASE(clingo_ast_comparison_operator_not_equal, "!=")
    CASE(clingo_ast_comparison_operator_equal, "=")
END_ENUM

BEGIN_ENUM(to_string, clingo_ast_aggregate_function_e)
    CASE(clingo_ast_aggregate_function_count, "#count")
    CASE(clingo_ast_aggregate_function_sum, "#sum")
    CASE(clingo_ast_aggregate_function_sump, "#sum+")
    CASE(clingo_ast_aggregate_function_min, "#min")
    CASE(clingo_ast_aggregate_function_max, "#max")
END_ENUM

BEGIN_ENUM(to_string_left, clingo_ast_theory_sequence_type_e)
    CASE(clingo_ast_theory_sequence_type_tuple, "(")
    CASE(clingo_ast_theory_sequence_type_list, "[")
    CASE(clingo_ast_theory_sequence_type_set, "{")
END_ENUM

BEGIN_ENUM(to_string_right, clingo_ast_theory_sequence_type_e)
    CASE(clingo_ast_theory_sequence_type_tuple, ")")
    CASE(clingo_ast_theory_sequence_type_list, "]")
    CASE(clingo_ast_theory_sequence_type_set, "}")
END_ENUM

BEGIN_ENUM(to_string, clingo_ast_theory_operator_type_e)
     CASE(clingo_ast_theory_operator_type_unary, "unary")
     CASE(clingo_ast_theory_operator_type_binary_left, "binary, left")
     CASE(clingo_ast_theory_operator_type_binary_right, "binary, right")
END_ENUM

BEGIN_ENUM(to_string, clingo_ast_theory_atom_definition_type_e)
    CASE(clingo_ast_theory_atom_definition_type_head, "head")
    CASE(clingo_ast_theory_atom_definition_type_body, "body")
    CASE(clingo_ast_theory_atom_definition_type_any, "any")
    CASE(clingo_ast_theory_atom_definition_type_directive, "directive")
END_ENUM

#undef BEGIN_ENUM
#undef CASE
#undef END_ENUM
#undef END_DEFAULT

template <class T>
T const &get(AST const &ast, clingo_ast_attribute_e name) {
    return mpark::get<T>(ast.value(name));
}

template <class T>
T get_enum(AST const &ast, clingo_ast_attribute_e name) {
    return static_cast<T>(mpark::get<int>(ast.value(name)));
}

bool get_bool(AST const &ast, clingo_ast_attribute_e name) {
    return mpark::get<int>(ast.value(name)) != 0;
}

AST const *get_opt_ast(AST const &ast, clingo_ast_attribute_e name) {
    if (!ast.hasValue(name)) {
        return nullptr;
    }
    return mpark::get<OAST>(ast.value(name)).ast.get();
}

template <class T>
char const *enum_to_string(AST const &ast, clingo_ast_attribute_e attribute) {
    return to_string(get_enum<T>(ast, attribute));
}

struct PrintValue {
    void operator()(int num) {
        out << num;
    }

    void operator()(Symbol sym) {
        out << sym;
    }

    void operator()(Location const &loc) {
        out << loc;
    }

    void operator()(String str) {
        out << str;
    }

    void operator()(SAST const &ast) {
        out << *ast;
    }

    void operator()(OAST const &ast) {
        if (ast.ast.get() != nullptr) {
            out << *ast.ast;
        }
    }

    void operator()(AST::ASTVec const &asts) {
        bool comma = false;
        for (auto const &ast : asts) {
            if (comma) {
                out << ",";
            }
            out << *ast;
            comma = true;
        }
    }

    void operator()(AST::StrVec const &strs) {
        bool comma = false;
        for (auto const &str : strs) {
            if (comma) {
                out << ",";
            }
            out << str;
            comma = true;
        }
    }

    std::ostream &out;
};

std::ostream &operator<<(std::ostream &out, SAST const &x) {
    out << *x;
    return out;
}

std::ostream &operator<<(std::ostream &out, OAST const &x) {
    out << *x.ast;
    return out;
}

template <class T>
struct PrintList {
public:
    PrintList(T const &list, char const *pre, char const *sep, char const *post, bool empty)
    : list_{list}
    , pre_{pre}
    , sep_{sep}
    , post_{post}
    , empty_{empty} { }

    friend std::ostream &operator<<(std::ostream &out, PrintList const &self) {
        bool paren = self.empty_ || !self.list_.empty();
        if (paren) {
            out << self.pre_;
        }
        bool comma = false;
        for (auto const &x : self.list_) {
            if (comma) {
                out << self.sep_;
            }
            else {
                comma = true;
            }
            out << x;
        }
        if (paren) {
            out << self.post_;
        }
        return out;
    }
private:
    T const &list_;
    char const *pre_;
    char const *sep_;
    char const *post_;
    bool empty_;
};

template <class T>
PrintList<T> print_list(T const &list, char const *pre, char const *sep, char const *post, bool empty) {
    return {list, pre, sep, post, empty};
}

template <class T>
PrintList<T> print_list(AST const &ast, clingo_ast_attribute_e attribute, char const *pre, char const *sep, char const *post, bool empty) {
    return {get<T>(ast, attribute), pre, sep, post, empty};
}

PrintList<AST::ASTVec> print_body(AST const &ast, clingo_ast_attribute_e attribute, char const *pre = " : ") {
    auto const &body = get<AST::ASTVec>(ast, attribute);
    return print_list(body, body.empty() ? "" : pre, "; ", ".", true);
}

class print {
public:
    print(AST const &ast, clingo_ast_attribute_e attr)
    : ast_{ast}
    , attr_{attr} { }

    friend std::ostream &operator<<(std::ostream &out, print const &self) {
        mpark::visit(PrintValue{out}, self.ast_.value(self.attr_));
        return out;
    }

private:
    AST const &ast_;
    clingo_ast_attribute_e attr_;
};

class print_left_guard {
public:
    print_left_guard(AST const &ast, clingo_ast_attribute_e attr)
    : ast_{ast}
    , attr_{attr} { }

    friend std::ostream &operator<<(std::ostream &out, print_left_guard const &self) {
        auto const *left = get_opt_ast(self.ast_, self.attr_);
        if (left != nullptr) {
            out << print(*left, clingo_ast_attribute_term) << " "
                << enum_to_string<clingo_ast_comparison_operator_e>(*left, clingo_ast_attribute_comparison) << " ";
        }
        return out;
    }

private:
    AST const &ast_;
    clingo_ast_attribute_e attr_;
};

class print_right_guard {
public:
    print_right_guard(AST const &ast, clingo_ast_attribute_e attr)
    : ast_{ast}
    , attr_{attr} { }

    friend std::ostream &operator<<(std::ostream &out, print_right_guard const &self) {
        auto const *left = get_opt_ast(self.ast_, self.attr_);
        if (left != nullptr) {
            out << " " << enum_to_string<clingo_ast_comparison_operator_e>(*left, clingo_ast_attribute_comparison)
                << " " << print(*left, clingo_ast_attribute_term);
        }
        return out;
    }

private:
    AST const &ast_;
    clingo_ast_attribute_e attr_;
};

} // namespace

std::ostream &operator<<(std::ostream &out, AST const &ast) {
    switch (ast.type()) {
        // {{{1 term
        case clingo_ast_type_id:
        case clingo_ast_type_variable: {
            out << print(ast, clingo_ast_attribute_name);
            break;
        }
        case clingo_ast_type_symbolic_term: {
            out << print(ast, clingo_ast_attribute_symbol);
            break;
        }
        case clingo_ast_type_unary_operation: {
            auto unop = get_enum<clingo_ast_unary_operator_e>(ast, clingo_ast_attribute_operator_type);
            out << to_string_left(unop) << print(ast, clingo_ast_attribute_argument) << to_string_right(unop);
            break;
        }
        case clingo_ast_type_binary_operation: {
            out << "("
                << print(ast, clingo_ast_attribute_left)
                << enum_to_string<clingo_ast_binary_operator_e>(ast, clingo_ast_attribute_operator_type)
                << print(ast, clingo_ast_attribute_right)
                << ")";
            break;
        }
        case clingo_ast_type_interval: {
            out << "(" << print(ast, clingo_ast_attribute_left) << ".." << print(ast, clingo_ast_attribute_right) << ")";
            break;
        }
        case clingo_ast_type_function: {
            auto name = get<String>(ast, clingo_ast_attribute_name);
            auto const &args = get<AST::ASTVec>(ast, clingo_ast_attribute_arguments);
            bool tc = name.empty() && args.size() == 1;
            bool ey = name.empty() && args.empty();
            out << (get_bool(ast, clingo_ast_attribute_external) ? "@" : "")
                << name
                << print_list(args, "(", ",", tc ? ",)" : ")", ey);
            break;
        }
        case clingo_ast_type_pool: {
            auto const &args = get<AST::ASTVec>(ast, clingo_ast_attribute_arguments);
            if (args.empty()) { out << "(1/0)"; }
            if (args.size() == 1) { out << args.front(); }
            else {
                bool old_ext = false;
                String const *old_name = nullptr;
                for (auto const &arg : args) {
                    if (arg->type() == clingo_ast_type_function) {
                        auto const &name = get<String>(*arg, clingo_ast_attribute_name);
                        auto ext = get_bool(*arg, clingo_ast_attribute_external);
                        if (old_name == nullptr) {
                            old_name = &name;
                            old_ext = ext;
                        }
                        else if (name != *old_name || ext != old_ext) {
                            old_name = nullptr;
                            break;
                        }
                    }
                    else {
                        old_name = nullptr;
                        break;
                    }
                }
                if (old_name != nullptr) {
                    out << (old_ext ? "@" : "") << *old_name << "(";
                    bool sem = false;
                    for (auto const &arg : args) {
                        if (sem) {
                            out << ";";
                        }
                        else {
                            sem = true;
                        }
                        auto const &pargs = get<AST::ASTVec>(*arg, clingo_ast_attribute_arguments);
                        bool tc = old_name->empty() && pargs.size() == 1;
                        out << print_list(pargs, "", ",", tc ? "," : "", true);
                    }
                    out << ")";
                }
                else {
                    out << print_list(args, "(", ";", ")", true);
                }
            }
            break;
        }
        // {{{1 literal
        case clingo_ast_type_literal: {
            out << enum_to_string<clingo_ast_sign_e>(ast, clingo_ast_attribute_sign)
                << print(ast, clingo_ast_attribute_atom);
            break;
        }
        case clingo_ast_type_boolean_constant: {
            out << (get_bool(ast, clingo_ast_attribute_value) ? "#true" : "#false");
            break;
        }
        case clingo_ast_type_symbolic_atom: {
            out << print(ast, clingo_ast_attribute_symbol);
            break;
        }
        case clingo_ast_type_comparison: {
            out << print(ast, clingo_ast_attribute_term)
                << print_list<AST::ASTVec>(ast, clingo_ast_attribute_guards, "", "", "", false);
            break;
        }
        // {{{1 aggregate
        case clingo_ast_type_guard: {
            out << " "
                << enum_to_string<clingo_ast_comparison_operator_e>(ast, clingo_ast_attribute_comparison)
                << " "
                << print(ast, clingo_ast_attribute_term);
            break;
        }
        case clingo_ast_type_conditional_literal: {
            out << print(ast, clingo_ast_attribute_literal)
                << print_list<AST::ASTVec>(ast, clingo_ast_attribute_condition, ": ", ", ", "", false);
            break;
        }
        case clingo_ast_type_aggregate: {
            out << print_left_guard(ast, clingo_ast_attribute_left_guard)
                << "{"
                << print_list<AST::ASTVec>(ast, clingo_ast_attribute_elements, " ", "; ", "", false)
                << " }"
                << print_right_guard(ast, clingo_ast_attribute_right_guard);
            break;
        }
        case clingo_ast_type_body_aggregate_element: {
            out << print_list<AST::ASTVec>(ast, clingo_ast_attribute_terms, "", ",", "", false)
                << print_list<AST::ASTVec>(ast, clingo_ast_attribute_condition, ": ", ", ", "", false);
            break;
        }
        case clingo_ast_type_body_aggregate: {
            out << print_left_guard(ast, clingo_ast_attribute_left_guard)
                << enum_to_string<clingo_ast_aggregate_function_e>(ast, clingo_ast_attribute_function)
                << " {" << print_list<AST::ASTVec>(ast, clingo_ast_attribute_elements, " ", "; ", "", false) << " }"
                << print_right_guard(ast, clingo_ast_attribute_right_guard);
            break;
        }
        case clingo_ast_type_head_aggregate_element: {
            out << print_list<AST::ASTVec>(ast, clingo_ast_attribute_terms, "", ",", "", false)
                << ": " << print(ast, clingo_ast_attribute_condition);
            break;
        }
        case clingo_ast_type_head_aggregate: {
            out << print_left_guard(ast, clingo_ast_attribute_left_guard)
                << enum_to_string<clingo_ast_aggregate_function_e>(ast, clingo_ast_attribute_function)
                << " {" << print_list<AST::ASTVec>(ast, clingo_ast_attribute_elements, " ", "; ", "", false) << " }"
                << print_right_guard(ast, clingo_ast_attribute_right_guard);
            break;
        }
        case clingo_ast_type_disjunction: {
            out << print_list<AST::ASTVec>(ast, clingo_ast_attribute_elements, "", "; ", "", false);
            break;
        }
        // {{{1 theory atom
        case clingo_ast_type_theory_sequence: {
            auto type = get_enum<clingo_ast_theory_sequence_type_e>(ast, clingo_ast_attribute_sequence_type);
            auto const &terms = get<AST::ASTVec>(ast, clingo_ast_attribute_terms);
            bool tc = terms.size() == 1 && type == clingo_ast_theory_sequence_type_tuple;
            out << to_string_left(type)
                << print_list<AST::ASTVec>(terms, "", ",", "", true)
                << (tc ? "," : "")
                << to_string_right(type);
            break;
        }
        case clingo_ast_type_theory_function: {
            auto const &args = get<AST::ASTVec>(ast, clingo_ast_attribute_arguments);
            out << print(ast, clingo_ast_attribute_name)
                << print_list<AST::ASTVec>(args, "(", ",", ")", !args.empty());
            break;
        }
        case clingo_ast_type_theory_unparsed_term_element: {
            out << print_list<AST::StrVec>(ast, clingo_ast_attribute_operators, "", " ", " ", false)
                << print(ast, clingo_ast_attribute_term);
            break;
        }
        case clingo_ast_type_theory_unparsed_term: {
            auto const &elems = get<AST::ASTVec>(ast, clingo_ast_attribute_elements);
            bool pp = elems.size() != 1 || !get<AST::StrVec>(*elems.front(), clingo_ast_attribute_operators).empty();
            out << (pp ? "(" : "")
                << print_list<AST::ASTVec>(elems, "", " ", "", false)
                << (pp ? ")" : "");
            break;
        }
        case clingo_ast_type_theory_guard: {
            out << print(ast, clingo_ast_attribute_operator_name) << " " << print(ast, clingo_ast_attribute_term);
            break;
        }
        case clingo_ast_type_theory_atom_element: {
            out << print_list<AST::ASTVec>(ast, clingo_ast_attribute_terms, "", ",", "", false)
                << print_list<AST::ASTVec>(ast, clingo_ast_attribute_condition, ": ", ", ", "", false);
            break;
        }
        case clingo_ast_type_theory_atom: {
            auto const *guard = get_opt_ast(ast, clingo_ast_attribute_guard);
            out << "&" << print(ast, clingo_ast_attribute_term)
                << " {" << print_list<AST::ASTVec>(ast, clingo_ast_attribute_elements, " ", "; ", "", false) << " }";
            if (guard != nullptr) {
                out << " " << *guard;
            }
            break;
        }
        // {{{1 theory definition
        case clingo_ast_type_theory_operator_definition: {
            out << print(ast, clingo_ast_attribute_name)
                << " : " << print(ast, clingo_ast_attribute_priority)
                << ", " << enum_to_string<clingo_ast_theory_operator_type_e>(ast, clingo_ast_attribute_operator_type);
            break;
        }
        case clingo_ast_type_theory_term_definition: {
            out << print(ast, clingo_ast_attribute_name)
                << " {\n" << print_list<AST::ASTVec>(ast, clingo_ast_attribute_operators, "  ", ";\n", "\n", true) << "}";
            break;
        }
        case clingo_ast_type_theory_guard_definition: {
            out << "{" << print_list<AST::StrVec>(ast, clingo_ast_attribute_operators, " ", ", ", "", false) << " }, "
                << print(ast, clingo_ast_attribute_term);
            break;
        }
        case clingo_ast_type_theory_atom_definition: {
            auto const *guard = get_opt_ast(ast, clingo_ast_attribute_guard);
            out << "&" << print(ast, clingo_ast_attribute_name) << "/" << print(ast, clingo_ast_attribute_arity) << ": " << print(ast, clingo_ast_attribute_term);
            if (guard != nullptr) {
                out << ", " << *guard;
            }
            out << ", " << enum_to_string<clingo_ast_theory_atom_definition_type_e>(ast, clingo_ast_attribute_atom_type);
            break;
        }
        case clingo_ast_type_theory_definition: {
            out << "#theory " << print(ast, clingo_ast_attribute_name) << " {\n";
            bool comma = false;
            for (auto const &y : get<AST::ASTVec>(ast, clingo_ast_attribute_terms)) {
                if (comma) {
                    out << ";\n";
                }
                else {
                    comma = true;
                }
                out << "  " << print(*y, clingo_ast_attribute_name)
                    << " {\n" << print_list<AST::ASTVec>(*y, clingo_ast_attribute_operators, "    ", ";\n    ", "\n", true) << "  }";
            }
            for (auto const &y : get<AST::ASTVec>(ast, clingo_ast_attribute_atoms)) {
                if (comma) {
                    out << ";\n";
                }
                else {
                    comma = true;
                }
                out << "  " << y;
            }
            if (comma) {
                out << "\n";
            }
            out << "}.";
            break;
        }
        // {{{1 statement
        case clingo_ast_type_rule: {
            out << print(ast, clingo_ast_attribute_head)
                << print_body(ast, clingo_ast_attribute_body, " :- ");
            return out;
            break;
        }
        case clingo_ast_type_definition: {
            out << "#const "
                << print(ast, clingo_ast_attribute_name)
                << " = "
                << print(ast, clingo_ast_attribute_value)
                << ".";
            if (!get_bool(ast, clingo_ast_attribute_is_default)) {
                out << " [override]";
            }
            break;
        }
        case clingo_ast_type_show_signature: {
            auto name = get<String>(ast, clingo_ast_attribute_name);
            if (!name.empty()) {
                out << "#show "
                    << (get_bool(ast, clingo_ast_attribute_positive) ? "" : "-")
                    << name
                    << "/"
                    << print(ast, clingo_ast_attribute_arity)
                    << ".";
            }
            else {
                out << "#show.";
            }
            break;
        }
        case clingo_ast_type_defined: {
            out << "#defined "
                << (get_bool(ast, clingo_ast_attribute_positive) ? "" : "-")
                << print(ast, clingo_ast_attribute_name) << "/"
                << print(ast, clingo_ast_attribute_arity) << ".";
            break;
        }
        case clingo_ast_type_show_term: {
            out << "#show "
                << print(ast, clingo_ast_attribute_term)
                << print_body(ast, clingo_ast_attribute_body);
            break;
        }
        case clingo_ast_type_minimize: {
            out << ":~ "
                << print_body(ast, clingo_ast_attribute_body, "")
                << " ["
                << print(ast, clingo_ast_attribute_weight)
                << "@"
                << print(ast, clingo_ast_attribute_priority)
                << print_list<AST::ASTVec>(ast, clingo_ast_attribute_terms, ",", ",", "", false)
                << "]";
            break;
        }
        case clingo_ast_type_script: {
            auto s = get<String>(ast, clingo_ast_attribute_code);
            auto t = get<String>(ast, clingo_ast_attribute_name);
            out << "#script (" << t << ")" << s << "#end.";
            break;
        }
        case clingo_ast_type_program: {
            out << "#program "
                << print(ast, clingo_ast_attribute_name)
                << print_list<AST::ASTVec>(ast, clingo_ast_attribute_parameters, "(", ", ", ")", false)
                << ".";
            break;
        }
        case clingo_ast_type_external: {
            out << "#external "
                << print(ast, clingo_ast_attribute_atom)
                << print_body(ast, clingo_ast_attribute_body)
                << " ["
                << print(ast, clingo_ast_attribute_external_type)
                << "]";
            break;
        }
        case clingo_ast_type_edge: {
            out << "#edge ("
                << print(ast, clingo_ast_attribute_node_u)
                << ","
                << print(ast, clingo_ast_attribute_node_v)
                << ")"
                << print_body(ast, clingo_ast_attribute_body);
            break;
        }
        case clingo_ast_type_heuristic: {
            out << "#heuristic "
                << print(ast, clingo_ast_attribute_atom)
                << print_body(ast, clingo_ast_attribute_body)
                << " ["
                << print(ast, clingo_ast_attribute_bias)
                << "@"
                << print(ast, clingo_ast_attribute_priority)
                << ","
                << print(ast, clingo_ast_attribute_modifier)
                << "]";
            break;
        }
        case clingo_ast_type_project_atom: {
            out << "#project "
                << print(ast, clingo_ast_attribute_atom)
                << print_body(ast, clingo_ast_attribute_body);
            break;
        }
        case clingo_ast_type_project_signature: {
            out << "#project "
                << (get_bool(ast, clingo_ast_attribute_positive) ? "" : "-")
                << print(ast, clingo_ast_attribute_name)
                << "/"
                << print(ast, clingo_ast_attribute_arity)
                << ".";
            break;
        }
        case clingo_ast_type_comment: {
            out << print(ast, clingo_ast_attribute_value);
            break;
        }
        // 1}}}
    }
    return out;
}

} } // namespace Input Gringo
