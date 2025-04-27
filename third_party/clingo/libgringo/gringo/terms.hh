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

#ifndef GRINGO_TERMS_HH
#define GRINGO_TERMS_HH

#include <gringo/utility.hh>
#include <gringo/base.hh>
#include <gringo/term.hh>
#include <gringo/hash_set.hh>
#include <potassco/theory_data.h>
#include <gringo/logger.hh>
#include <functional>

namespace Gringo {

using StringVec = std::vector<String>;
template <class T>
struct GetName {
    using result_type = String;
    String const &operator()(T const &x) const {
        return x.name();
    }
};

// {{{1 TheoryOpDef

class TheoryOpDef {
public:
    using Key = std::pair<String, bool>;
    struct GetKey {
        Key operator()(TheoryOpDef const &x) const {
            return x.key();
        }
    };

    TheoryOpDef(Location const &loc, String op, unsigned priority, TheoryOperatorType type);
    TheoryOpDef(TheoryOpDef const &other) = delete;
    TheoryOpDef(TheoryOpDef &&other) noexcept = default;
    TheoryOpDef &operator=(TheoryOpDef const &other) = delete;
    TheoryOpDef &operator=(TheoryOpDef &&other) noexcept = default;
    ~TheoryOpDef() noexcept = default;

    String op() const;
    Key key() const noexcept;
    Location const &loc() const;
    void print(std::ostream &out) const;
    unsigned priority() const;
    TheoryOperatorType type() const;

private:
    Location loc_;
    String op_;
    unsigned priority_;
    TheoryOperatorType type_;
};

using TheoryOpDefs =
    ordered_set<
        TheoryOpDef,
        HashKey<TheoryOpDef::Key, TheoryOpDef::GetKey, mix_value_hash<TheoryOpDef::Key>>,
        EqualToKey<TheoryOpDef::Key, TheoryOpDef::GetKey>>;

inline std::ostream &operator<<(std::ostream &out, TheoryOpDef const &def) {
    def.print(out);
    return out;
}

// {{{1 TheoryTermDef

class TheoryTermDef {
public:
    TheoryTermDef(Location const &loc, String name);
    TheoryTermDef(TheoryTermDef const &other) = delete;
    TheoryTermDef(TheoryTermDef &&other) noexcept = default;
    TheoryTermDef &operator=(TheoryTermDef const &other) = delete;
    // Note: for some reason the compiler thinks it cannot generate a noexcept
    // move assignment operator.
    TheoryTermDef &operator=(TheoryTermDef &&other) noexcept {
        loc_ = other.loc_;
        name_ = other.name_;
        opDefs_ = std::move(other.opDefs_);
        return *this;
    }
    ~TheoryTermDef() noexcept = default;

    void addOpDef(TheoryOpDef &&def, Logger &log) const;
    String const &name() const noexcept;
    Location const &loc() const;
    void print(std::ostream &out) const;
    // returns (priority, flag) where flag is true if the binary operator is left associative
    std::pair<unsigned, bool> getPrioAndAssoc(String op) const;
    unsigned getPrio(String op, bool unary) const;
    bool hasOp(String op, bool unary) const;

private:
    Location loc_;
    String name_;
    mutable TheoryOpDefs opDefs_;
};

using TheoryTermDefs =
    ordered_set<
        TheoryTermDef,
        HashKey<String, GetName<TheoryTermDef>, mix_hash<String>>,
        EqualToKey<String, GetName<TheoryTermDef>>>;

inline std::ostream &operator<<(std::ostream &out, TheoryTermDef const &def) {
    def.print(out);
    return out;
}

// {{{1 TheoryAtomDef

class TheoryAtomDef {
public:
    using Key = Sig;
    struct GetKey {
        Key operator()(TheoryAtomDef const &x) const {
            return x.sig();
        }
    };

    TheoryAtomDef(Location const &loc, String name, unsigned arity, String elemDef, TheoryAtomType type);
    TheoryAtomDef(Location const &loc, String name, unsigned arity, String elemDef, TheoryAtomType type, StringVec &&ops, String guardDef);
    TheoryAtomDef(TheoryAtomDef const &other) = delete;
    TheoryAtomDef(TheoryAtomDef &&other) noexcept = default;
    TheoryAtomDef &operator=(TheoryAtomDef const &other) = delete;
    TheoryAtomDef &operator=(TheoryAtomDef &&other) noexcept = default;
    ~TheoryAtomDef() noexcept = default;

    Sig sig() const;
    bool hasGuard() const;
    TheoryAtomType type() const;
    StringVec const &ops() const;
    Location const &loc() const;
    String elemDef() const;
    String guardDef() const;
    void print(std::ostream &out) const;

private:
    Location loc_;
    Sig sig_;
    String elemDef_;
    String guardDef_;
    StringVec ops_;
    TheoryAtomType type_;
};

using TheoryAtomDefs =
    ordered_set<
        TheoryAtomDef,
        HashKey<TheoryAtomDef::Key, TheoryAtomDef::GetKey, mix_value_hash<TheoryAtomDef::Key>>,
        EqualToKey<TheoryAtomDef::Key, TheoryAtomDef::GetKey>>;

inline std::ostream &operator<<(std::ostream &out, TheoryAtomDef const &def) {
    def.print(out);
    return out;
}

// {{{1 TheoryDef

class TheoryDef {
public:
    TheoryDef(Location const &loc, String name);
    TheoryDef(TheoryDef const &other) = delete;
    TheoryDef(TheoryDef &&other) noexcept = default;
    TheoryDef &operator=(TheoryDef const &other) = delete;
    TheoryDef &operator=(TheoryDef &&other) noexcept = default;
    ~TheoryDef() noexcept = default;

    String const &name() const noexcept;
    void addAtomDef(TheoryAtomDef &&def, Logger &log) const;
    void addTermDef(TheoryTermDef &&def, Logger &log) const;
    TheoryAtomDef const *getAtomDef(Sig sig) const;
    TheoryTermDef const *getTermDef(String name) const;
    TheoryAtomDefs const &atomDefs() const;
    Location const &loc() const;
    void print(std::ostream &out) const;

private:
    Location loc_;
    String name_;
    mutable TheoryTermDefs termDefs_;
    mutable TheoryAtomDefs atomDefs_;
};

using TheoryDefs =
    ordered_set<
        TheoryDef,
        HashKey<String, GetName<TheoryDef>, mix_hash<String>>,
        EqualToKey<String, GetName<TheoryDef>>>;

inline std::ostream &operator<<(std::ostream &out, TheoryDef const &def) {
    def.print(out);
    return out;
}
// }}}1

} // namespace Gringo

#endif // GRINGO_TERMS_HH
