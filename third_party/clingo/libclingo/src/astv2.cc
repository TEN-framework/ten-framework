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

struct Deepcopy {
    template <typename T>
    AST::Value operator()(T const &val) {
        return val;
    }
    AST::Value operator()(SAST &ast) {
        return ast->deepcopy();
    }
    AST::Value operator()(AST::ASTVec &asts) {
        AST::ASTVec ret;
        ret.reserve(asts.size());
        for (auto &ast : asts) {
            ret.emplace_back(ast->deepcopy());
        }
        return AST::Value{std::move(ret)};
    }
};

struct HashCombine {
    template <class T>
    void operator()(T const &a) {
        Gringo::hash_combine(seed, Gringo::get_value_hash(a));
    }
    void operator()(Location const &a) {
        // location attributes are skipped
        static_cast<void>(a);
    }
    void operator()(OAST const &a) {
        if (a.ast.get() != nullptr) {
            Gringo::hash_combine(seed, a.ast->hash());
        }
    }
    void operator()(SAST const &a) {
        Gringo::hash_combine(seed, a->hash());
    }
    void operator()(AST::ASTVec const &a) {
        Gringo::hash_combine(seed, a.size());
        for (auto const &ast : a) {
            Gringo::hash_combine(seed, ast->hash());
        }
    }
    size_t seed;
};

struct CompareLess {
    template <class T>
    bool operator()(T const &a) {
        return a < mpark::get<T>(b);
    }
    bool operator()(OAST const &a) {
        auto const &ast_b = mpark::get<OAST>(b);
        if (ast_b.ast.get() == nullptr) {
            return false;
        }
        if (a.ast.get() == nullptr) {
            return true;
        }
        return *a.ast < *ast_b.ast;
    }
    bool operator()(SAST const &a) {
        return *a < *mpark::get<SAST>(b);
    }
    bool operator()(AST::ASTVec const &a) {
        auto const &vec_b = mpark::get<AST::ASTVec>(b);
        return std::lexicographical_compare(
            a.begin(), a.end(), vec_b.begin(), vec_b.end(),
            [](SAST const &a, SAST const &b) { return *a < *b; });
    }
    AST::Value const &b;
};

struct CompareEqual {
    template <class T>
    bool operator()(T const &a) {
        return a == mpark::get<T>(b);
    }
    bool operator()(OAST const &a) {
        auto const &ast_b = mpark::get<OAST>(b);
        if (a.ast.get() == nullptr || ast_b.ast.get() == nullptr) {
            return a.ast.get() == ast_b.ast.get();
        }
        return *a.ast == *ast_b.ast;
    }
    bool operator()(SAST const &a) {
        return *a == *mpark::get<SAST>(b);
    }
    bool operator()(AST::ASTVec const &a) {
        auto const &vec_b = mpark::get<AST::ASTVec>(b);
        return std::equal(
            a.begin(), a.end(), vec_b.begin(), vec_b.end(),
            [](SAST const &a, SAST const &b) { return *a == *b; });
    }
    AST::Value const &b;
};

} // namespace

AST::AST(clingo_ast_type_e type)
: type_{type} { }

AST::AttributeVector::iterator AST::find_(clingo_ast_attribute_e name) {
    return std::find_if(values_.begin(), values_.end(), [name](auto const &x) { return x.first == name; });
}

AST::AttributeVector::const_iterator AST::find_(clingo_ast_attribute_e name) const {
    return std::find_if(values_.begin(), values_.end(), [name](auto const &x) { return x.first == name; });
}

bool AST::hasValue(clingo_ast_attribute_e name) const {
    return find_(name) != values_.end();
}

AST::Value const &AST::value(clingo_ast_attribute_e name) const {
    auto it = find_(name);
    if (it == values_.end()) {
        std::ostringstream oss;
        oss << "ast "
            << "'" << g_clingo_ast_constructors.constructors[type()].name << "'"
            << " does not have attribute "
            << "'" << g_clingo_ast_attribute_names.names[name] << "'";
        throw std::runtime_error(oss.str());
    }
    return it->second;
}

AST::Value &AST::value(clingo_ast_attribute_e name) {
    auto it = find_(name);
    if (it == values_.end()) {
        std::ostringstream oss;
        oss << "ast "
            << "'" << g_clingo_ast_constructors.constructors[type()].name << "'"
            << " does not have attribute "
            << "'" << g_clingo_ast_attribute_names.names[name] << "'";
        throw std::runtime_error(oss.str());
    }
    return it->second;
}

void AST::value(clingo_ast_attribute_e name, Value value) {
    assert(find_(name) == values_.end());
    values_.emplace_back(name, std::move(value));
}

SAST AST::copy() {
    auto ast = SAST{type_};
    ast->values_ = values_;
    return ast;
}

SAST AST::deepcopy() {
    auto ast = SAST{type_};
    for (auto &val : values_) {
        ast->values_.emplace_back(val.first, mpark::visit(Deepcopy{}, val.second));
    }
    return ast;
}

size_t AST::hash() const {
    HashCombine combine{std::hash<int>()(static_cast<int>(type()))};
    for (auto const &val : values_) {
        if (val.first == clingo_ast_attribute_location) {
            continue;
        }
        Gringo::hash_combine(combine.seed, static_cast<int>(val.first));
        mpark::visit(combine, val.second);
    }
    return combine.seed;
}

bool operator<(AST const &a, AST const &b) {
    if (a.type() != b.type()) {
        return a.type() < b.type();
    }
    auto it_a = a.values_.begin();
    auto ie_a = a.values_.end();
    auto it_b = b.values_.begin();
    auto ie_b = b.values_.end();
    for ( ; ; ++it_a, ++it_b) {
        if (it_a != ie_a && it_a->first == clingo_ast_attribute_location) {
            ++it_a;
        }
        if (it_b != ie_b && it_b->first == clingo_ast_attribute_location) {
            ++it_b;
        }
        if (it_a == ie_a) {
            return it_b != ie_b;
        }
        if (it_b == ie_b) {
            return false;
        }
        if (it_a->second.index() != it_b->second.index()) {
            return it_a->second.index() < it_b->second.index();
        }
        if (!mpark::visit(CompareEqual{it_b->second}, it_a->second)) {
            return mpark::visit(CompareLess{it_b->second}, it_a->second);
        }
    }
    return false;
}

bool operator==(AST const &a, AST const &b) {
    if (a.type() != b.type()) {
        return false;
    }
    auto it_a = a.values_.begin();
    auto ie_a = a.values_.end();
    auto it_b = b.values_.begin();
    auto ie_b = b.values_.end();
    for ( ; ; ++it_a, ++it_b) {
        if (it_a != ie_a && it_a->first == clingo_ast_attribute_location) {
            ++it_a;
        }
        if (it_b != ie_b && it_b->first == clingo_ast_attribute_location) {
            ++it_b;
        }
        if (it_a == ie_a) {
            return it_b == ie_b;
        }
        if (it_b == ie_b) {
            return false;
        }
        if (it_a->second.index() != it_b->second.index()) {
            return false;
        }
        if (!mpark::visit(CompareEqual{it_b->second}, it_a->second)) {
            return false;
        }
    }
    return true;
}

void AST::incRef() {
    ++refCount_;
}

void AST::decRef() {
    assert(refCount_ > 0);
    --refCount_;
}

unsigned AST::refCount() const {
    return refCount_;
}

bool AST::unique() const {
    return refCount_ == 1;
}

clingo_ast_type_e AST::type() const {
    return type_;
}

SAST::SAST()
: ast_{nullptr} { }

SAST::SAST(AST *ast)
: ast_{ast} {
    if (ast_ != nullptr) {
        ast_->incRef();
    }
}

SAST::SAST(clingo_ast_type_e type)
: ast_{new AST{type}} {
    ast_->incRef();
}

SAST::SAST(SAST const &ast)
: ast_{ast.ast_} {
    if (ast_ != nullptr) {
        ast_->incRef();
    }
}

SAST::SAST(SAST &&ast) noexcept
: ast_{nullptr} {
    std::swap(ast_, ast.ast_);
}

SAST &SAST::operator=(SAST const &ast) {
    if (this != &ast) {
        clear();
        if (ast.ast_ != nullptr) {
            ast_ = ast.ast_;
            ast_->incRef();
        }
    }
    return *this;
}

SAST &SAST::operator=(SAST &&ast) noexcept {
    std::swap(ast_, ast.ast_);
    return *this;
}

AST *SAST::operator->() const {
    return ast_;
}

unsigned SAST::use_count() const {
    return ast_->refCount();
}

AST *SAST::get() const {
    return ast_;
}

AST *SAST::release() {
    auto *ast = ast_;
    ast_ = nullptr;
    return ast;
}

void SAST::clear() {
    if (ast_ != nullptr) {
        ast_->decRef();
        if (ast_->refCount() == 0) {
            delete ast_;
        }
        ast_ = nullptr;
    }
}

AST &SAST::operator*() const {
    return *ast_;
}

SAST::~SAST() {
    clear();
}

} } // namespace Input Gringo
