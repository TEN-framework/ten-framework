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

#include "luaclingo.h"

#include <lua.hpp>
#include <cstring>
#include <forward_list>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <limits>

namespace {

// {{{1 error handling

template <typename T>
auto protect(lua_State *L, T f) -> decltype(f()) {
    try                             { return f(); }
    catch (std::exception const &e) { luaL_error(L, e.what()); }
    catch (...)                     { luaL_error(L, "unknown error"); }
    throw std::logic_error("cannot happen");
}
#define PROTECT(E) (protect(L, [&]{ return (E); }))

// translates a clingo api error into a lua error
void handle_c_error(lua_State *L, bool ret) {
    if (!ret) {
        char const *msg = clingo_error_message();
        if (!msg) { msg = "no message"; }
        luaL_error(L, msg);
    }
}

// translates a lua error into a a clingo api error
bool handle_lua_error(lua_State *L, char const *loc, char const *desc, int code) {
    try {
        switch (code) {
            case LUA_ERRRUN:
            case LUA_ERRERR:
            case LUA_ERRSYNTAX: {
                char const *s = lua_tostring(L, -1);
                std::ostringstream msg;
                msg << loc << ": " << "error: " << desc << ":\n"
                    << (code == LUA_ERRSYNTAX ? "  SyntaxError: " : "  RuntimeError: ")
                    << s << "\n"
                    ;
                clingo_set_error(clingo_error_runtime, msg.str().c_str());
                lua_pop(L, 1);
                return false;
            }
            case LUA_ERRMEM: {
                std::stringstream msg;
                msg << loc << ": error: lua interpreter ran out of memory" << "\n";
                clingo_set_error(clingo_error_bad_alloc, msg.str().c_str());
                lua_pop(L, 1);
                return false;
            }
        }
    }
    catch(...) {
        lua_pop(L, 1);
        clingo_set_error(clingo_error_logic, "error during error handling");
        return false;
    }
    return true;
}


template <typename... T>
struct Types { };

template<int, typename... T>
struct LastTypes;
template<>
struct LastTypes<0> { using Type = Types<>; };
template<typename A, typename... T>
struct LastTypes<0, A, T...> { using Type = Types<A, T...>; };
template<int n, typename A, typename... T>
struct LastTypes<n, A, T...> : LastTypes<n-1, T...> { };

template<int n, typename T>
struct LastArgs;
template<int n, typename... Args>
struct LastArgs<n, bool (*)(Args...)> : LastTypes<n, Args...> { };

template <typename F, typename... Args>
void call_c_(Types<>*, lua_State *L, F f, Args ...args) {
    handle_c_error(L, f(args...));
}

template <typename T, typename F, typename... Args>
T call_c_(Types<T*>*, lua_State *L, F f, Args ...args) {
    T ret;
    handle_c_error(L, f(args..., &ret));
    return ret;
}

template <typename T, typename U, typename F, typename... Args>
std::pair<T, U> call_c_(Types<T*, U*>*, lua_State *L, F f, Args ...args) {
    T ret1;
    U ret2;
    handle_c_error(L, f(args..., &ret1, &ret2));
    return {ret1, ret2};
}

template <typename F, typename... Args, typename T=typename LastArgs<sizeof...(Args), F>::Type>
static auto call_c(lua_State *L, F f, Args... args) -> decltype(call_c_(static_cast<T*>(nullptr), L, f, args...)) {
    return call_c_(static_cast<T*>(nullptr), L, f, args...);
}

static int luaTraceback (lua_State *L) {
    if (!lua_isstring(L, 1)) { return 1; }
    lua_getglobal(L, "debug");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 1;
    }
    lua_getfield(L, -1, "traceback");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 1;
    }
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    lua_getglobal(L, "string");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 1;
    }
    lua_getfield(L, -1, "gsub");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return 1;
    }
    lua_pushvalue(L, -3);
    lua_pushliteral(L, "\t");
    lua_pushliteral(L, "  ");
    lua_call(L, 3, 1);
    return 1;
}

// {{{1 Object

void lua_regMeta(lua_State *L, char const *name, luaL_Reg const * funs, lua_CFunction indexfun = nullptr, lua_CFunction newindexfun = nullptr) {
#if LUA_VERSION_NUM < 502
    luaL_newmetatable(L, name);
    luaL_register(L, 0, funs);
#else
    luaL_newmetatable(L, name);
    luaL_setfuncs(L, funs, 0);
#endif
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -2);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__index");
    if (indexfun) {
        lua_pushcfunction(L, indexfun);
    }
    else {
        lua_pushvalue(L, -2);
    }
    lua_rawset(L, -3);
    if (newindexfun) {
        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, newindexfun);
        lua_rawset(L, -3);
    }
}

template <typename T>
struct Object {
    template <typename... Args>
    static int new_(lua_State *L, Args&&... args) {
        new (lua_newuserdata(L, sizeof(T))) T(std::forward<Args>(args)...);
        luaL_getmetatable(L, T::typeName);
        lua_setmetatable(L, -2);
        return 1;
    }

    static void reg(lua_State *L) {
        lua_regMeta(L, T::typeName, T::meta, T::index, T::newindex);
    }

    static T &get_self(lua_State* L) {
        return *(T*)luaL_checkudata(L, 1, T::typeName);
    }

    T &cmpKey() { return *static_cast<T*>(this); }

    static int eq(lua_State *L) {
        T *a = static_cast<T*>(luaL_checkudata(L, 1, T::typeName));
        T *b = static_cast<T*>(luaL_checkudata(L, 2, T::typeName));
        lua_pushboolean(L, a->cmpKey() == b->cmpKey());
        return 1;
    }
    static int lt(lua_State *L) {
        T *a = static_cast<T*>(luaL_checkudata(L, 1, T::typeName));
        T *b = static_cast<T*>(luaL_checkudata(L, 2, T::typeName));
        lua_pushboolean(L, a->cmpKey() < b->cmpKey());
        return 1;
    }
    static int le(lua_State *L) {
        T *a = static_cast<T*>(luaL_checkudata(L, 1, T::typeName));
        T *b = static_cast<T*>(luaL_checkudata(L, 2, T::typeName));
        lua_pushboolean(L, a->cmpKey() <= b->cmpKey());
        return 1;
    }
    static constexpr luaL_Reg const meta[] = {{nullptr, nullptr}};
    static constexpr lua_CFunction const index = nullptr;
    static constexpr lua_CFunction const newindex = nullptr;
};

template <typename T>
constexpr luaL_Reg const Object<T>::meta[];
template <typename T>
constexpr lua_CFunction const Object<T>::index;
template <typename T>
constexpr lua_CFunction const Object<T>::newindex;

// {{{1 wrap Any

struct Any {
    struct PlaceHolder {
        virtual ~PlaceHolder() { };
    };
    template <class T>
    struct Holder : public PlaceHolder {
        template <typename... Args>
        Holder(Args&&... args) : value(std::forward<Args>(args)...) { }
        T value;
    };
    Any() : content(nullptr) { }
    Any(Any &&other) : content(nullptr) { std::swap(content, other.content); }
    template<typename T, typename... Args>
    Any(T*, Args&&... args) : content(new Holder<T>(std::forward<T>(args)...)) { }
    ~Any() { delete content; }

    Any &operator=(Any &&other) {
        std::swap(content, other.content);
        return *this;
    }

    template<typename T>
    T *get() {
        auto x = dynamic_cast<Holder<T>*>(content);
        return x ? &x->value : nullptr;
    }
    template<typename T>
    T const *get() const {
        auto x = dynamic_cast<Holder<T>*>(content);
        return x ? &x->value : nullptr;
    }
    bool empty() const { return !content; }

    PlaceHolder *content = nullptr;
};

template <typename T, typename... Args>
Any make_any(Args&&... args) {
    return Any(static_cast<T*>(nullptr), std::forward<Args>(args)...);
}

struct AnyWrap : Object<AnyWrap> {
    Any any;
    AnyWrap() { }
    template <typename T, typename... Args>
    static T *new_(lua_State *L, Args&&... args) {
        Object::new_(L);
        auto self = static_cast<AnyWrap*>(lua_touserdata(L, -1));
        PROTECT((self->any = make_any<T>(std::forward<Args>(args)...), 0));
        return self->any.get<T>();
    }
    static int gc(lua_State *L) {
        auto &self = get_self(L);
        self.~AnyWrap();
        return 0;
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo._Any";
};

constexpr char const *AnyWrap::typeName;

luaL_Reg const AnyWrap::meta[] = {
    {"__gc", gc},
    {nullptr, nullptr}
};

// {{{1 auxliary functions

namespace Detail {

template <int X> using int_type = std::integral_constant<int, X>;
template <class T, class S>
inline void nc_check(S s, int_type<0>) { // same sign
    (void)s;
    assert((std::is_same<T, S>::value) || (s >= std::numeric_limits<T>::min() && s <= std::numeric_limits<T>::max()));
}
template <class T, class S>
inline void nc_check(S s, int_type<-1>) { // Signed -> Unsigned
    (void)s;
    assert(s >= 0 && static_cast<S>(static_cast<T>(s)) == s);
}
template <class T, class S>
inline void nc_check(S s, int_type<1>) { // Unsigned -> Signed
    (void)s;
    assert(!(s > static_cast<typename std::make_unsigned<T>::type>(std::numeric_limits<T>::max())));
}

} // namespace Detail

template <class T, class S>
inline T numeric_cast(S s) {
    constexpr int sv = int(std::numeric_limits<T>::is_signed) - int(std::numeric_limits<S>::is_signed);
    Detail::nc_check<T>(s, Detail::int_type<sv>());
    return static_cast<T>(s);
}


clingo_symbol_t luaToVal(lua_State *L, int idx);

#if LUA_VERSION_NUM < 502

int lua_absindex(lua_State *L, int idx) {
    return (idx < 0) ? lua_gettop(L) + idx + 1 : idx;
}

// size_t lua_len(lua_State *L, int index) {
//     return lua_objlen(L, index);
// }

// size_t lua_rawlen(lua_State *L, int index) {
//     return lua_objlen(L, index);
// }

#endif

static int luaPushKwArg(lua_State *L, int index, int pos, char const *name, bool optional) {
    index = lua_absindex(L, index);
    lua_pushinteger(L, pos);
    lua_gettable(L, index);
    if (!lua_isnil(L, -1)) {
        if (name) {
            lua_getfield(L, index, name);
            if (!lua_isnil(L, -1)) {
                lua_pop(L, 2);
                return luaL_error(L, "argument #%d also given by keyword %s", pos, name);
            }
            lua_pop(L, 1);
        }
    }
    else if (name) {
        lua_pop(L, 1);
        lua_getfield(L, index, name);
    }
    if (!optional && lua_isnil(L, -1)) {
        return name ? luaL_error(L, "argument %s (#%d) missing", name, pos) : luaL_error(L, "argument #%d missing", pos);
    }
    return 1;
}

template <class T, class U>
void luaToCpp(lua_State *L, int index, std::pair<T, U> &x);

void luaToCpp(lua_State *L, int index, clingo_weighted_literal_t &x);

template <class T>
void luaToCpp(lua_State *L, int index, std::vector<T> &x);

void luaToCpp(lua_State *L, int index, bool &x) {
    x = lua_toboolean(L, index) != 0;
}

void luaToCpp(lua_State *L, int index, std::string &x) {
    char const *str = lua_tostring(L, index);
    PROTECT(x = str);
}

template <class T>
void luaToCpp(lua_State *L, int index, T &x, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr) {
    if (lua_type(L, index) != LUA_TNUMBER) {
        luaL_error(L, "number expected");
    }
    x = numeric_cast<T>(lua_tointeger(L, index));
}

struct symbol_wrapper {
    clingo_symbol_t symbol;
};

void luaToCpp(lua_State *L, int index, symbol_wrapper &x) {
    x.symbol = luaToVal(L, index);
}

struct symbolic_literal_t {
    clingo_symbol_t symbol;
    bool positive;
};

void luaToCpp(lua_State *L, int index, symbolic_literal_t &x);

template <class T>
void luaToCpp(lua_State *L, int index, std::vector<T> &x) {
    index = lua_absindex(L, index);
    if (lua_type(L, index) != LUA_TTABLE) {
        luaL_error(L, "table expected");
    }
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        auto &y = protect(L, [&x]() -> T& { x.emplace_back(); return x.back(); });
        luaToCpp(L, -1, y);
        lua_pop(L, 1);
    }
}

template <class T, class U>
void luaToCpp(lua_State *L, int index, std::pair<T, U> &x) {
    index = lua_absindex(L, index);
    if (lua_type(L, index) != LUA_TTABLE) {
        luaL_error(L, "table expected");
    }
    lua_pushnil(L);
    if (lua_next(L, index) != 0) {
        luaToCpp(L, -1, x.first);
        lua_pop(L, 1);
    }
    else {
        luaL_error(L, "tuple expected");
    }
    if (lua_next(L, index) != 0) {
        luaToCpp(L, -1, x.second);
        lua_pop(L, 1);
    }
    else {
        luaL_error(L, "tuple expected");
    }
    if (lua_next(L, index) != 0) {
        luaL_error(L, "tuple expected");
    }
}

void luaToCpp(lua_State *L, int index, symbolic_literal_t &x) {
    std::pair<symbol_wrapper&, bool&> p{reinterpret_cast<symbol_wrapper&>(x.symbol), x.positive};
    luaToCpp(L, index, p);
}

void luaToCpp(lua_State *L, int index, clingo_weighted_literal_t &x) {
    std::pair<clingo_literal_t&, clingo_weight_t&> y{x.literal, x.weight};
    luaToCpp(L, index, y);
}

template <class T>
typename std::enable_if<std::is_integral<T>::value, void>::type cppToLua(lua_State *L, T value) {
    lua_pushnumber(L, value);
}

template <class T>
void cppToLua(lua_State *L, T *values, size_t size) {
    lua_createtable(L, size, 0);
    int i = 1;
    for (auto it = values, ie = it + size; it != ie; ++it) {
        cppToLua(L, *it);
        lua_rawseti(L, -2, i++);
    }
}

// replaces the table at index idx with a pointer holding a vector
std::vector<clingo_symbol_t> *luaToVals(lua_State *L, int idx) {
    idx = lua_absindex(L, idx);
    luaL_checktype(L, idx, LUA_TTABLE);
    std::vector<clingo_symbol_t> *vals = AnyWrap::new_<std::vector<clingo_symbol_t>>(L);
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        clingo_symbol_t sym = luaToVal(L, -1);
        protect(L, [sym, &vals](){ vals->push_back(sym); });
        lua_pop(L, 1);
    }
    lua_replace(L, idx);
    return vals;
}

struct LuaClear {
    LuaClear(lua_State *L) : L(L), n(lua_gettop(L)) { }
    ~LuaClear() { lua_settop(L, n); }
    lua_State *L;
    int n;
};

std::ostream &operator<<(std::ostream &out, clingo_location_t loc) {
    out << loc.begin_file << ":" << loc.begin_line << ":" << loc.begin_column;
    if (strcmp(loc.begin_file, loc.end_file) != 0) {
        out << "-" << loc.end_file << ":" << loc.end_line << ":" << loc.end_column;
    }
    else if (loc.begin_line != loc.end_line) {
        out << "-" << loc.end_line << ":" << loc.end_column;
    }
    else if (loc.begin_column != loc.end_column) {
        out << "-" << loc.end_column;
    }
    return out;
}

// {{{1 wrap SolveResult

struct SolveResult : Object<SolveResult> {
    clingo_solve_result_bitset_t res;
    SolveResult(clingo_solve_result_bitset_t res) : res(res) { }
    static int satisfiable(lua_State *L) {
        auto &&res = get_self(L).res;
        if      (res & clingo_solve_result_satisfiable)   { lua_pushboolean(L, true); }
        else if (res & clingo_solve_result_unsatisfiable) { lua_pushboolean(L, false); }
        else                                              { lua_pushnil(L); }
        return 1;
    }
    static int unsatisfiable(lua_State *L) {
        auto &&res = get_self(L).res;
        if      (res & clingo_solve_result_unsatisfiable) { lua_pushboolean(L, true); }
        else if (res & clingo_solve_result_satisfiable)   { lua_pushboolean(L, false); }
        else                                              { lua_pushnil(L); }
        return 1;
    }
    static int unknown(lua_State *L) {
        auto &&res = get_self(L).res;
        if ((res & clingo_solve_result_unsatisfiable) ||
            (res & clingo_solve_result_satisfiable)) { lua_pushboolean(L, false); }
        else                                         { lua_pushboolean(L, true); }
        return 1;
    }
    static int exhausted(lua_State *L) {
        auto &&res = get_self(L).res;
        lua_pushboolean(L, res & clingo_solve_result_exhausted);
        return 1;
    }
    static int interrupted(lua_State *L) {
        auto &&res = get_self(L).res;
        lua_pushboolean(L, res & clingo_solve_result_interrupted);
        return 1;
    }
    static int index(lua_State *L) {
        char const *name = luaL_checkstring(L, 2);
        if      (strcmp(name, "satisfiable")   == 0) { return satisfiable(L); }
        else if (strcmp(name, "unsatisfiable") == 0) { return unsatisfiable(L); }
        else if (strcmp(name, "unknown")       == 0) { return unknown(L); }
        else if (strcmp(name, "exhausted")     == 0) { return exhausted(L); }
        else if (strcmp(name, "interrupted")   == 0) { return interrupted(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return !lua_isnil(L, -1) ? 1 : luaL_error(L, "unknown field: %s", name);
        }
    }
    static int toString(lua_State *L) {
        auto &&res = get_self(L).res;
        if      (res & clingo_solve_result_satisfiable)   { lua_pushstring(L, "SAT"); }
        else if (res & clingo_solve_result_unsatisfiable) { lua_pushstring(L, "UNSAT"); }
        else                                              { lua_pushstring(L, "UNKNOWN"); }
        return 1;
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.SolveResult";
};

constexpr char const *SolveResult::typeName;

luaL_Reg const SolveResult::meta[] = {
    {"__tostring", toString},
    {nullptr, nullptr}
};

// {{{1 wrap TheoryTerm

struct TheoryTermType : Object<TheoryTermType> {
    clingo_theory_term_type_e type;
    TheoryTermType(clingo_theory_term_type_e type) : type(type) { }
    clingo_theory_term_type_e cmpKey() { return type; }
    static int addToRegistry(lua_State *L) {
        lua_createtable(L, 0, 6);
        for (auto t : { clingo_theory_term_type_function, clingo_theory_term_type_number, clingo_theory_term_type_symbol, clingo_theory_term_type_tuple, clingo_theory_term_type_list, clingo_theory_term_type_set}) {
            Object::new_(L, t);
            lua_setfield(L, -2, field_(t));
        }
        lua_setfield(L, -2, "TheoryTermType");
        return 0;
    }
    static char const *field_(clingo_theory_term_type_t t) {
        switch (static_cast<clingo_theory_term_type_e>(t)) {
            case clingo_theory_term_type_function: { return "Function"; }
            case clingo_theory_term_type_number:   { return "Number"; }
            case clingo_theory_term_type_symbol:   { return "Symbol"; }
            case clingo_theory_term_type_tuple:    { return "Tuple"; }
            case clingo_theory_term_type_list:     { return "List"; }
            case clingo_theory_term_type_set:      { return "Set"; }
        }
        return "";
    }
    static int new_(lua_State *L, clingo_theory_term_type_t t) {
        lua_getfield(L, LUA_REGISTRYINDEX, "clingo");
        lua_getfield(L, -1, "TheoryTermType");
        lua_replace(L, -2);
        lua_getfield(L, -1, field_(t));
        lua_replace(L, -2);
        return 1;
    }
    static int toString(lua_State *L) {
        lua_pushstring(L, field_(get_self(L).type));
        return 1;
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.TheoryTermType";
};

constexpr char const *TheoryTermType::typeName;

luaL_Reg const TheoryTermType::meta[] = {
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {"__tostring", toString},
    { nullptr, nullptr }
};

struct TheoryTerm : Object<TheoryTerm> {
    clingo_theory_atoms_t const *atoms;
    clingo_id_t id;
    TheoryTerm(clingo_theory_atoms_t const *atoms, clingo_id_t id) : atoms(atoms), id(id) { }
    clingo_id_t cmpKey() { return id; }
    static int name(lua_State *L) {
        auto &self = get_self(L);
        lua_pushstring(L, call_c(L, clingo_theory_atoms_term_name, self.atoms, self.id));
        return 1;
    }
    static int number(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, call_c(L, clingo_theory_atoms_term_number, self.atoms, self.id));
        return 1;
    }
    static int args(lua_State *L) {
        auto &self = get_self(L);
        auto ret = call_c(L, clingo_theory_atoms_term_arguments, self.atoms, self.id);
        lua_createtable(L, numeric_cast<int>(ret.second), 0);
        int i = 1;
        for (auto it = ret.first, ie = it + ret.second; it != ie; ++it) {
            new_(L, self.atoms, *it);
            lua_rawseti(L, -2, i++);
        }
        return 1;
    }
    static int toString(lua_State *L) {
        auto &self = get_self(L);
        size_t size = call_c(L, clingo_theory_atoms_term_to_string_size, self.atoms, self.id);
        char *buf = static_cast<char *>(lua_newuserdata(L, size * sizeof(*buf))); // +1
        handle_c_error(L, clingo_theory_atoms_term_to_string(self.atoms, self.id, buf, size));
        lua_pushstring(L, buf);                                                   // +1
        lua_replace(L, -2);                                                       // -1
        return 1;
    }
    static int type(lua_State *L) {
        auto &self = get_self(L);
        return TheoryTermType::new_(L, call_c(L, clingo_theory_atoms_term_type, self.atoms, self.id));
    }
    static int index(lua_State *L) {
        char const *field = luaL_checkstring(L, 2);
        if (strcmp(field, "type") == 0) { return type(L); }
        else if (strcmp(field, "name") == 0) { return name(L); }
        else if (strcmp(field, "arguments") == 0) { return args(L); }
        else if (strcmp(field, "number") == 0) { return number(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, field);
            return !lua_isnil(L, -1) ? 1 : luaL_error(L, "unknown field: %s", field);
        }
    }
    static constexpr char const *typeName = "clingo.TheoryTerm";
    static luaL_Reg const meta[];
};

constexpr char const *TheoryTerm::typeName;
luaL_Reg const TheoryTerm::meta[] = {
    {"__tostring", toString},
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {nullptr, nullptr}
};

// {{{1 wrap TheoryElement

struct TheoryElement : Object<TheoryElement> {
    TheoryElement(clingo_theory_atoms_t const *atoms, clingo_id_t id) : atoms(atoms) , id(id) { }
    clingo_theory_atoms_t const *atoms;
    clingo_id_t cmpKey() { return id; }
    clingo_id_t id;

    static int terms(lua_State *L) {
        auto &self = get_self(L);
        auto ret = call_c(L, clingo_theory_atoms_element_tuple, self.atoms, self.id);
        lua_createtable(L, numeric_cast<int>(ret.second), 0);
        int i = 1;
        for (auto it = ret.first, ie = it + ret.second; it != ie; ++it) {
            TheoryTerm::new_(L, self.atoms, *it);
            lua_rawseti(L, -2, i++);
        }
        return 1;
    }

    static int condition(lua_State *L) {
        auto &self = get_self(L);
        auto ret = call_c(L, clingo_theory_atoms_element_condition, self.atoms, self.id);
        lua_createtable(L, numeric_cast<int>(ret.second), 0);
        int i = 1;
        for (auto it = ret.first, ie = it + ret.second; it != ie; ++it) {
            lua_pushinteger(L, *it);
            lua_rawseti(L, -2, i++);
        }
        return 1;
    }

    static int conditionId(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, call_c(L, clingo_theory_atoms_element_condition_id, self.atoms, self.id));
        return 1;
    }

    static int toString(lua_State *L) {
        auto &self = get_self(L);
        auto size = call_c(L, clingo_theory_atoms_element_to_string_size, self.atoms, self.id);
        char *buf = static_cast<char *>(lua_newuserdata(L, size * sizeof(*buf))); // +1
        handle_c_error(L, clingo_theory_atoms_element_to_string(self.atoms, self.id, buf, size));
        lua_pushstring(L, buf);                                                   // +1
        lua_replace(L, -2);                                                       // -1
        return 1;
    }

    static int index(lua_State *L) {
        char const *field = luaL_checkstring(L, 2);
        if (strcmp(field, "terms") == 0) { return terms(L); }
        else if (strcmp(field, "condition") == 0) { return condition(L); }
        else if (strcmp(field, "condition_id") == 0) { return conditionId(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, field);
            return !lua_isnil(L, -1) ? 1 : luaL_error(L, "unknown field: %s", field);
        }
    }

    static constexpr char const *typeName = "clingo.TheoryElement";
    static luaL_Reg const meta[];
};

constexpr char const *TheoryElement::typeName;
luaL_Reg const TheoryElement::meta[] = {
    {"__tostring", toString},
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {nullptr, nullptr}
};

// {{{1 wrap TheoryAtom

struct TheoryAtom : Object<TheoryAtom> {
    clingo_theory_atoms_t const *atoms;
    clingo_id_t id;
    TheoryAtom(clingo_theory_atoms_t const *atoms, clingo_id_t id) : atoms(atoms) , id(id) { }
    clingo_id_t cmpKey() { return id; }

    static int elements(lua_State *L) {
        auto &self = get_self(L);
        auto ret = call_c(L, clingo_theory_atoms_atom_elements, self.atoms, self.id);
        lua_createtable(L, numeric_cast<int>(ret.second), 0);
        int i = 1;
        for (auto it = ret.first, ie = it + ret.second; it != ie; ++it) {
            TheoryElement::new_(L, self.atoms, *it);
            lua_rawseti(L, -2, i++);
        }
        return 1;
    }

    static int term(lua_State *L) {
        auto &self = get_self(L);
        return TheoryTerm::new_(L, self.atoms, call_c(L, clingo_theory_atoms_atom_term, self.atoms, self.id));
    }

    static int literal(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, call_c(L, clingo_theory_atoms_atom_literal, self.atoms, self.id));
        return 1;
    }

    static int guard(lua_State *L) {
        auto &self = get_self(L);
        if (!call_c(L, clingo_theory_atoms_atom_has_guard, self.atoms, self.id)) {
            lua_pushnil(L);
            return 1;
        }
        lua_createtable(L, 2, 0);
        auto ret = call_c(L, clingo_theory_atoms_atom_guard, self.atoms, self.id);
        lua_pushstring(L, ret.first);
        lua_rawseti(L, -2, 1);
        TheoryTerm::new_(L, self.atoms, ret.second);
        lua_rawseti(L, -2, 2);
        return 1;
    }

    static int toString(lua_State *L) {
        auto &self = get_self(L);
        auto size = call_c(L, clingo_theory_atoms_atom_to_string_size, self.atoms, self.id);
        char *buf = static_cast<char *>(lua_newuserdata(L, size * sizeof(*buf))); // +1
        handle_c_error(L, clingo_theory_atoms_atom_to_string(self.atoms, self.id, buf, size));
        lua_pushstring(L, buf);                                                   // +1
        lua_replace(L, -2);                                                       // -1
        return 1;
    }

    static int index(lua_State *L) {
        char const *field = luaL_checkstring(L, 2);
        if (strcmp(field, "elements") == 0) { return elements(L); }
        else if (strcmp(field, "term") == 0) { return term(L); }
        else if (strcmp(field, "guard") == 0) { return guard(L); }
        else if (strcmp(field, "literal") == 0) { return literal(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, field);
            return !lua_isnil(L, -1) ? 1 : luaL_error(L, "unknown field: %s", field);
        }
    }

    static constexpr char const *typeName = "clingo.TheoryAtom";
    static luaL_Reg const meta[];
};

constexpr char const *TheoryAtom::typeName;
luaL_Reg const TheoryAtom::meta[] = {
    {"__tostring", toString},
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {nullptr, nullptr}
};

// {{{1 wrap TheoryIter

struct TheoryIter {
    static int iter(lua_State *L, clingo_theory_atoms_t const *atoms) {
        lua_pushlightuserdata(L, const_cast<clingo_theory_atoms_t*>(atoms));
        lua_pushinteger(L, 0);
        lua_pushcclosure(L, iter_, 2);
        return 1;
    }

    static int iter_(lua_State *L) {
        auto atoms = (clingo_theory_atoms_t const *)lua_topointer(L, lua_upvalueindex(1));
        clingo_id_t idx = numeric_cast<clingo_id_t>(lua_tonumber(L, lua_upvalueindex(2)));
        if (idx < call_c(L, clingo_theory_atoms_size, atoms)) {
            lua_pushinteger(L, idx + 1);
            lua_replace(L, lua_upvalueindex(2));
            TheoryAtom::new_(L, atoms, idx);
        }
        else { lua_pushnil(L); }
        return 1;
    }
};

// {{{1 wrap Term

struct SymbolType : Object<SymbolType> {
    clingo_symbol_type_t type;
    SymbolType(clingo_symbol_type_t type) : type(type) { }
    clingo_symbol_type_t cmpKey() { return type; }
    static int addToRegistry(lua_State *L) {
        lua_createtable(L, 0, 5);
        for (auto t : {clingo_symbol_type_number, clingo_symbol_type_string, clingo_symbol_type_function, clingo_symbol_type_infimum, clingo_symbol_type_supremum}) {
            new_(L, t);
            lua_setfield(L, -2, field_(t));
        }
        lua_setfield(L, -2, "SymbolType");
        return 0;
    }
    static char const *field_(clingo_symbol_type_t type) {
        switch (static_cast<clingo_symbol_type_e>(type)) {
            case clingo_symbol_type_number:   { return "Number"; }
            case clingo_symbol_type_string:   { return "String"; }
            case clingo_symbol_type_function: { return "Function"; }
            case clingo_symbol_type_infimum:  { return "Infimum"; }
            case clingo_symbol_type_supremum: { break; }
        }
        return "Supremum";
    }

    static int toString(lua_State *L) {
        lua_pushstring(L, field_(get_self(L).type));
        return 1;
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.SymbolType";
};

constexpr char const *SymbolType::typeName;

luaL_Reg const SymbolType::meta[] = {
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {"__tostring", toString},
    { nullptr, nullptr }
};

struct Term : Object<Term> {
    clingo_symbol_t symbol;
    Term(clingo_symbol_t symbol) : symbol(symbol) { }
    static int new_(lua_State *L, clingo_symbol_t sym) {
        auto type = clingo_symbol_type(sym);
        if (type == clingo_symbol_type_supremum) {
            lua_getfield(L, LUA_REGISTRYINDEX, "clingo");
            lua_getfield(L, -1, "Supremum");
            lua_replace(L, -2);
        }
        else if (type == clingo_symbol_type_infimum) {
            lua_getfield(L, LUA_REGISTRYINDEX, "clingo");
            lua_getfield(L, -1, "Infimum");
            lua_replace(L, -2);
        }
        else { Object::new_(L, sym); }
        return 1;
    }
    static int addToRegistry(lua_State *L) {
        clingo_symbol_t sym;
        clingo_symbol_create_supremum(&sym);
        Object::new_(L, sym);
        lua_setfield(L, -2, "Supremum");
        clingo_symbol_create_infimum(&sym);
        Object::new_(L, sym);
        lua_setfield(L, -2, "Infimum");
        return 0;
    }
    static int newFun(lua_State *L) {
        char const *name = luaL_checklstring(L, 1, nullptr);
        bool positive = true;
        if (!lua_isnone(L, 3) && !lua_isnil(L, 3)) {
            luaToCpp(L, 3, positive);
        }
        if (name[0] == '\0' && !positive) { luaL_argerror(L, 2, "tuples must not have signs"); }
        if (lua_isnoneornil(L, 2)) {
            return new_(L, call_c(L, clingo_symbol_create_id, name, positive));
        }
        else {
            lua_pushvalue(L, 2);
            std::vector<clingo_symbol_t> *args = luaToVals(L, -1);
            new_(L, call_c(L, clingo_symbol_create_function, name, args->data(), args->size(), positive));
            lua_replace(L, -2);
            return 1;
        }
    }
    static int newTuple(lua_State *L) {
        lua_pushstring(L, "");
        lua_insert(L, 1);
        return newFun(L);
    }
    static int newNumber(lua_State *L) {
        clingo_symbol_t sym;
        clingo_symbol_create_number(numeric_cast<int>(luaL_checkinteger(L, 1)), &sym);
        return Term::new_(L, sym);
    }
    static int newString(lua_State *L) {
        return Term::new_(L, call_c(L, clingo_symbol_create_string, luaL_checkstring(L, 1)));
    }
    bool operator==(Term const &other) {
        return clingo_symbol_is_equal_to(symbol, other.symbol);
    }
    bool operator <(Term const &other) {
        return clingo_symbol_is_less_than(symbol, other.symbol);
    }
    bool operator<=(Term const &other) {
        return !clingo_symbol_is_less_than(other.symbol, symbol);
    }
    static int name(lua_State *L) {
        auto &self = get_self(L);
        if (clingo_symbol_type(self.symbol) == clingo_symbol_type_function) {
            lua_pushstring(L, call_c(L, clingo_symbol_name, self.symbol));
        }
        else {
            lua_pushnil(L);
        }
        return 1;
    }
    static int string(lua_State *L) {
        auto &self = get_self(L);
        if (clingo_symbol_type(self.symbol) == clingo_symbol_type_string) {
            lua_pushstring(L, call_c(L, clingo_symbol_string, self.symbol));
        }
        else {
            lua_pushnil(L);
        }
        return 1;
    }
    static int number(lua_State *L) {
        auto &self = get_self(L);
        if (clingo_symbol_type(self.symbol) == clingo_symbol_type_number) {
            lua_pushinteger(L, call_c(L, clingo_symbol_number, self.symbol));
        }
        else {
            lua_pushnil(L);
        }
        return 1;
    }
    static int negative(lua_State *L) {
        auto &self = get_self(L);
        if (clingo_symbol_type(self.symbol) == clingo_symbol_type_function) {
            lua_pushboolean(L, call_c(L, clingo_symbol_is_negative, self.symbol));
        }
        else {
            lua_pushnil(L);
        }
        return 1;
    }
    static int positive(lua_State *L) {
        auto &self = get_self(L);
        if (clingo_symbol_type(self.symbol) == clingo_symbol_type_function) {
            lua_pushboolean(L, call_c(L, clingo_symbol_is_positive, self.symbol));
        }
        else {
            lua_pushnil(L);
        }
        return 1;
    }
    static int args(lua_State *L) {
        auto &self = get_self(L);
        if (clingo_symbol_type(self.symbol) == clingo_symbol_type_function) {
            auto ret = call_c(L, clingo_symbol_arguments, self.symbol);
            lua_createtable(L, numeric_cast<int>(ret.second), 0);
            int i = 1;
            for (auto it = ret.first, ie = it + ret.second; it != ie; ++it) {
                Term::new_(L, *it);
                lua_rawseti(L, -2, i++);
            }
        }
        else {
            lua_pushnil(L);
        }
        return 1;
    }
    static int toString(lua_State *L) {
        auto &self = get_self(L);
        auto size = call_c(L, clingo_symbol_to_string_size, self.symbol);
        char *buf = static_cast<char *>(lua_newuserdata(L, size * sizeof(*buf))); // +1
        handle_c_error(L, clingo_symbol_to_string(self.symbol, buf, size));
        lua_pushstring(L, buf);                                                   // +1
        lua_replace(L, -2);                                                       // -1
        return 1;
    }
    static int type(lua_State *L) {
        auto &self = get_self(L);
        lua_getfield(L, LUA_REGISTRYINDEX, "clingo");
        lua_getfield(L, -1, "SymbolType");
        lua_getfield(L, -1, SymbolType::field_(clingo_symbol_type(self.symbol)));
        return 1;
    }
    static int index(lua_State *L) {
        char const *field = luaL_checkstring(L, 2);
        if      (strcmp(field, "positive") == 0)  { return positive(L); }
        else if (strcmp(field, "negative") == 0)  { return negative(L); }
        else if (strcmp(field, "arguments") == 0) { return args(L); }
        else if (strcmp(field, "name") == 0)      { return name(L); }
        else if (strcmp(field, "string") == 0)    { return string(L); }
        else if (strcmp(field, "number") == 0)    { return number(L); }
        else if (strcmp(field, "type") == 0)      { return type(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, field);
            return 1;
        }
    }
    static constexpr char const *typeName = "clingo.Symbol";
    static luaL_Reg const meta[];
};

constexpr char const *Term::typeName;
luaL_Reg const Term::meta[] = {
    {"__tostring", toString},
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {nullptr, nullptr}
};

clingo_symbol_t luaToVal(lua_State *L, int idx) {
    int type = lua_type(L, idx);
    switch (type) {
        case LUA_TSTRING: {
            return call_c(L, clingo_symbol_create_string, lua_tostring(L, idx));
        }
        case LUA_TNUMBER: {
            clingo_symbol_t ret;
            clingo_symbol_create_number(numeric_cast<int>(lua_tointeger(L, idx)), &ret);
            return ret;
        }
        case LUA_TUSERDATA: {
            bool check = false;
            if (lua_getmetatable(L, idx)) {                          // +1
                lua_getfield(L, LUA_REGISTRYINDEX, "clingo.Symbol"); // +1
                check = lua_rawequal(L, -1, -2) != 0;
                lua_pop(L, 2);                                       // -2
            }
            if (check) { return static_cast<Term*>(lua_touserdata(L, idx))->symbol; }
        }
        default: { luaL_error(L, "cannot convert to value"); }
    }
    return {};
}

struct LuaCallArgs_ {
    char const *name;
    clingo_symbol_t const *arguments;
    size_t size;
    clingo_symbol_callback_t symbol_callback;
    void *data;
};

int luacall_(lua_State *L) {
    auto &args = *static_cast<LuaCallArgs_*>(lua_touserdata(L, 1));
    int context = 0;
    if (!lua_isnil(L, 2)) {
        context = 1;
        lua_getfield(L, 2, args.name);
        lua_pushvalue(L, 2);
    }
    else { lua_getglobal(L, args.name); }
    for (auto it = args.arguments, ie = it + args.size; it != ie; ++it) {
        Term::new_(L, *it);
    }
    lua_call(L, numeric_cast<int>(args.size + context), 1);
    if (lua_type(L, -1) == LUA_TTABLE) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            clingo_symbol_t val = luaToVal(L, -1);
            handle_c_error(L, args.symbol_callback(&val, 1, args.data));
            lua_pop(L, 1);
        }
    }
    else {
        clingo_symbol_t val = luaToVal(L, -1);
        handle_c_error(L, args.symbol_callback(&val, 1, args.data));
    }
    return 0;
}

// TODO: also needs a version with a string location
bool luacall(lua_State *L, clingo_location_t const *location, int context, char const *name, clingo_symbol_t const *arguments, size_t arguments_size, clingo_symbol_callback_t symbol_callback, void *symbol_callback_data) {
    if (!lua_checkstack(L, 4)) {
        clingo_set_error(clingo_error_bad_alloc, "lua stack size exceeded");
        return false;
    }
    LuaCallArgs_ args{ name, arguments, arguments_size, symbol_callback, symbol_callback_data };
    lua_pushcfunction(L, luaTraceback); // +1
    int err = lua_gettop(L);
    lua_pushcfunction(L, luacall_);     // +1
    lua_pushlightuserdata(L, &args);    // +1
    if (context) { lua_pushvalue(L, context); }
    else         { lua_pushnil(L); }    // +1
    auto ret = lua_pcall(L, 2, 0, -4);  // -3|-2
    lua_remove(L, err);
    if (ret != 0) {
        std::string loc, desc;
        try {
            std::ostringstream oss;
            oss << *location;
            loc = oss.str();
            desc = "error calling ";
            desc += name;
        }
        catch (...) {
            lua_pop(L, 1); // |-1
            clingo_set_error(clingo_error_runtime, "error during error handling");
            return false;
        }
        return handle_lua_error(L, loc.c_str(), desc.c_str(), ret);
    }
    return true;
}

// {{{1 wrap SymbolicAtom

struct SymbolicAtom : Object<SymbolicAtom> {
    clingo_symbolic_atoms_t const *atoms;
    clingo_symbolic_atom_iterator_t iter;
    static luaL_Reg const meta[];
    static constexpr const char *typeName = "clingo.SymbolicAtom";
    SymbolicAtom(clingo_symbolic_atoms_t const *atoms, clingo_symbolic_atom_iterator_t iter)
    : atoms(atoms)
    , iter(iter) { }
    static int symbol(lua_State *L) {
        auto &self = get_self(L);
        return Term::new_(L, call_c(L, clingo_symbolic_atoms_symbol, self.atoms, self.iter));
    }
    static int literal(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, call_c(L, clingo_symbolic_atoms_literal, self.atoms, self.iter));
        return 1;
    }
    static int is_fact(lua_State *L) {
        auto &self = get_self(L);
        lua_pushboolean(L, call_c(L, clingo_symbolic_atoms_is_fact, self.atoms, self.iter));
        return 1;
    }
    static int is_external(lua_State *L) {
        auto &self = get_self(L);
        lua_pushboolean(L, call_c(L, clingo_symbolic_atoms_is_external, self.atoms, self.iter));
        return 1;
    }
    static int index(lua_State *L) {
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "is_fact") == 0) { return is_fact(L); }
        else if (strcmp(name, "is_external") == 0) { return is_external(L); }
        else if (strcmp(name, "symbol") == 0) { return symbol(L); }
        else if (strcmp(name, "literal") == 0) { return literal(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return !lua_isnil(L, -1) ? 1 : luaL_error(L, "unknown field: %s", name);
        }
    }
};

constexpr const char *SymbolicAtom::typeName;

luaL_Reg const SymbolicAtom::meta[] = {
    {nullptr, nullptr}
};

// {{{1 wrap SymbolicAtoms

struct SymbolicAtoms : Object<SymbolicAtoms> {
    clingo_symbolic_atoms_t const *atoms;
    static luaL_Reg const meta[];
    static constexpr const char *typeName = "clingo.SymbolicAtoms";
    SymbolicAtoms(clingo_symbolic_atoms_t const *atoms) : atoms(atoms) { }

    static int symbolicAtomIter(lua_State *L) {
        auto current = static_cast<SymbolicAtom *>(luaL_checkudata(L, lua_upvalueindex(1), SymbolicAtom::typeName));
        if (call_c(L, clingo_symbolic_atoms_is_valid, current->atoms, current->iter)) {
            lua_pushvalue(L, lua_upvalueindex(1));                                               // +1
            auto next = call_c(L, clingo_symbolic_atoms_next, current->atoms, current->iter);
            SymbolicAtom::new_(L, current->atoms, next);                                         // +1
            lua_replace(L, lua_upvalueindex(1));                                                 // -1
        }
        else { lua_pushnil(L); }                                                                 // +1
        return 1;
    }

    static int len(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, call_c(L, clingo_symbolic_atoms_size, self.atoms));
        return 1;
    }

    static int iter(lua_State *L) {
        auto &self = get_self(L);
        auto range = call_c(L, clingo_symbolic_atoms_begin, self.atoms, nullptr);
        SymbolicAtom::new_(L, self.atoms, range); // +1
        lua_pushcclosure(L, symbolicAtomIter, 1); // +0
        return 1;
    }

    static int lookup(lua_State *L) {
        auto &self = get_self(L);
        auto atom = luaToVal(L, 2);
        auto range = call_c(L, clingo_symbolic_atoms_find, self.atoms, atom);
        if (call_c(L, clingo_symbolic_atoms_is_valid, self.atoms, range)) {
            SymbolicAtom::new_(L, self.atoms, range); // +1
        }
        else { lua_pushnil(L); } // +1
        return 1;
    }

    static int by_signature(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        int arity = numeric_cast<int>(luaL_checkinteger(L, 3));
        bool positive = lua_isnone(L, 4) || lua_toboolean(L, 4);
        clingo_signature_t sig = call_c(L, clingo_signature_create, name, arity, positive);
        auto range = call_c(L, clingo_symbolic_atoms_begin, self.atoms, &sig);
        SymbolicAtom::new_(L, self.atoms, range); // +1
        lua_pushcclosure(L, symbolicAtomIter, 1); // +0
        return 1;
    }

    static int signatures(lua_State *L) {
        auto &self = get_self(L);
        auto size = call_c(L, clingo_symbolic_atoms_signatures_size, self.atoms);
        clingo_signature_t *ret = static_cast<clingo_signature_t*>(lua_newuserdata(L, sizeof(*ret) * size)); // +1
        handle_c_error(L, clingo_symbolic_atoms_signatures(self.atoms, ret, size));
        lua_createtable(L, numeric_cast<int>(size), 0);            // +1
        int i = 1;
        for (auto it = ret, ie = it + size; it != ie; ++it) {
            lua_createtable(L, 3, 0);                              // +1
            lua_pushstring(L, clingo_signature_name(*it));         // +1
            lua_rawseti(L, -2, 1);                                 // -1
            lua_pushinteger(L, clingo_signature_arity(*it));       // +1
            lua_rawseti(L, -2, 2);                                 // -1
            lua_pushboolean(L, clingo_signature_is_positive(*it)); // +1
            lua_rawseti(L, -2, 3);                                 // -1
            lua_rawseti(L, -2, i);                                 // -1
            ++i;
        }
        lua_replace(L, -2);                                        // -1
        return 1;
    }
    static int index(lua_State *L) {
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "signatures") == 0) { return signatures(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return 1;
        }
    }
};

constexpr const char *SymbolicAtoms::typeName;

luaL_Reg const SymbolicAtoms::meta[] = {
    {"__len", len},
    {"iter", iter},
    {"lookup", lookup},
    {"by_signature", by_signature},
    {nullptr, nullptr}
};

// {{{1 wrap SolveControl

static clingo_literal_t luaToAtom(lua_State *L, int idx, clingo_symbolic_atoms_t const *atoms) {
    if (lua_isnumber(L, idx)) {
        clingo_literal_t lit;
        luaToCpp(L, idx, lit);
        return lit;
    }
    else {
        symbol_wrapper sym;
        luaToCpp(L, idx, sym);
        auto it = call_c(L, clingo_symbolic_atoms_find, atoms, sym.symbol);
        if (call_c(L, clingo_symbolic_atoms_is_valid, atoms, it)) {
            auto ret = call_c(L, clingo_symbolic_atoms_literal, atoms, it);
            return ret;
        }
    }
    return 0;
}

/*
static clingo_literal_t luaToLit(lua_State *L, int idx, clingo_symbolic_atoms_t const *atoms, bool *positive = nullptr) {
    if (lua_isnumber(L, idx)) {
        clingo_literal_t lit;
        luaToCpp(L, idx, lit);
        if (positive) { *positive = lit > 0; }
        return lit;
    }
    else {
        symbolic_literal_t sym;
        luaToCpp(L, idx, sym);
        if (positive) { *positive = sym.positive; }
        auto it = call_c(L, clingo_symbolic_atoms_find, atoms, sym.symbol);
        if (call_c(L, clingo_symbolic_atoms_is_valid, atoms, it)) {
            auto ret = call_c(L, clingo_symbolic_atoms_literal, atoms, it);
            return sym.positive ? ret : -ret;
        }
    }
    return 0;
}
*/

// invert is just to turn clauses into nogoods, which was added to match the formal background in our papers
// disjunctive determines if the literals are going to be used as a disjunction or conjunction
// this function returns null if the literals are unnecessary, i.e., if
//   there is a true literal in a disjunction, or
//   there is a false literal in a conjunction
static std::vector<clingo_literal_t> *luaToLits(lua_State *L, int tableIdx, clingo_symbolic_atoms_t const *atoms, bool invert, bool disjunctive) {
    if (lua_type(L, tableIdx) != LUA_TTABLE) {
        luaL_error(L, "table expected");
    }
    tableIdx = lua_absindex(L, tableIdx);
    std::vector<clingo_literal_t> *lits = AnyWrap::new_<std::vector<clingo_literal_t>>(L); // +1
    lua_pushnil(L);                                                                        // +1
    while (lua_next(L, tableIdx) != 0) {                                                   // +1/-1
        if (lua_isnumber(L, -1)) {
            clingo_literal_t lit;
            luaToCpp(L, -1, lit);
            if (invert) { lit = -lit; }
            protect(L, [lits, lit]() { lits->emplace_back(lit); });
        }
        else {
            symbolic_literal_t sym;
            luaToCpp(L, -1, sym);
            if (invert) { sym.positive = !sym.positive; }
            auto it = call_c(L, clingo_symbolic_atoms_find, atoms, sym.symbol);
            if (call_c(L, clingo_symbolic_atoms_is_valid, atoms, it)) {
                clingo_literal_t lit = call_c(L, clingo_symbolic_atoms_literal, atoms, it);
                if (!sym.positive) { lit = -lit; }
                protect(L, [lits, lit]() { lits->emplace_back(lit); });
            }
            else if (sym.positive != disjunctive) {
                lua_pop(L, 3);
                return nullptr;
            }
        }
        lua_pop(L, 1); // -1
    }
    return lits;
}

struct SolveControl : Object<SolveControl> {
    clingo_solve_control_t *ctl;
    SolveControl(clingo_solve_control_t *ctl) : ctl(ctl) { }
    static int getClause(lua_State *L, bool invert) {
        auto &self = get_self(L);
        auto atoms = call_c(L, clingo_solve_control_symbolic_atoms, self.ctl);
        if (auto *lits = luaToLits(L, 2, atoms, invert, true)) { // +1/+0
            handle_c_error(L, clingo_solve_control_add_clause(self.ctl, lits->data(), lits->size()));
            lua_pop(L, 1); // -1
        }
        return 0;
    }
    static int add_clause(lua_State *L) {
        return getClause(L, false);
    }
    static int add_nogood(lua_State *L) {
        return getClause(L, true);
    }
    static int index(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "symbolic_atoms") == 0) { return SymbolicAtoms::new_(L, call_c(L, clingo_solve_control_symbolic_atoms, self.ctl)); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return 1;
        }
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.SolveControl";
};

constexpr char const *SolveControl::typeName;

luaL_Reg const SolveControl::meta[] = {
    {"add_clause", add_clause},
    {"add_nogood", add_nogood},
    {nullptr, nullptr}
};

// {{{1 wrap Model

struct ModelType : Object<ModelType> {
    using Type = clingo_model_type_t;
    Type type;
    ModelType(Type type) : type(type) { }
    Type cmpKey() { return type; }

    static int addToRegistry(lua_State *L) {
        lua_createtable(L, 0, 6);
        for (auto t : { clingo_model_type_stable_model, clingo_model_type_brave_consequences, clingo_model_type_cautious_consequences }) {
            Object::new_(L, t);
            lua_setfield(L, -2, field_(t));
        }
        lua_setfield(L, -2, "ModelType");
        return 0;
    }
    static char const *field_(Type t) {
        switch (static_cast<clingo_model_type_e>(t)) {
            case clingo_model_type_stable_model:          { return "StableModel"; }
            case clingo_model_type_brave_consequences:    { return "BraveConsequences"; }
            case clingo_model_type_cautious_consequences: { break; }
        }
        return "CautiousConsequences";
    }
    static int new_(lua_State *L, Type t) {
        lua_getfield(L, LUA_REGISTRYINDEX, "clingo");
        lua_getfield(L, -1, "ModelType");
        lua_replace(L, -2);
        lua_getfield(L, -1, field_(t));
        lua_replace(L, -2);
        return 1;
    }
    static int toString(lua_State *L) {
        lua_pushstring(L, field_(get_self(L).type));
        return 1;
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.ModelType";
};

constexpr char const *ModelType::typeName;

luaL_Reg const ModelType::meta[] = {
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {"__tostring", toString},
    { nullptr, nullptr }
};

struct Model : Object<Model> {
    clingo_model_t const *model;
    clingo_model_t *mut_model;
    Model(clingo_model_t *model) : model(model), mut_model{model} { }
    Model(clingo_model_t const *model) : model(model), mut_model{nullptr} { }
    static int contains(lua_State *L) {
        auto &self = get_self(L);
        clingo_symbol_t sym = luaToVal(L, 2);
        lua_pushboolean(L, call_c(L, clingo_model_contains, self.model, sym));
        return 1;
    }
    static int is_true(lua_State *L) {
        auto &self = get_self(L);
        clingo_literal_t lit;
        luaToCpp(L, 2, lit);
        lua_pushboolean(L, call_c(L, clingo_model_is_true, self.model, lit));
        return 1;
    }
    static int atoms(lua_State *L) {
        auto &self = get_self(L);
        clingo_show_type_bitset_t atomset = 0;
        luaL_checktype(L, 2, LUA_TTABLE);
        lua_getfield(L, 2, "atoms");
        if (lua_toboolean(L, -1)) { atomset |= clingo_show_type_atoms; }
        lua_pop(L, 1);
        lua_getfield(L, 2, "shown");
        if (lua_toboolean(L, -1)) { atomset |= clingo_show_type_shown; }
        lua_pop(L, 1);
        lua_getfield(L, 2, "terms");
        if (lua_toboolean(L, -1)) { atomset |= clingo_show_type_terms; }
        lua_pop(L, 1);
        lua_getfield(L, 2, "theory");
        if (lua_toboolean(L, -1)) { atomset |= clingo_show_type_theory; }
        lua_pop(L, 1);
        lua_getfield(L, 2, "complement");
        if (lua_toboolean(L, -1)) { atomset |= clingo_show_type_complement; }
        lua_pop(L, 1);
        auto size = call_c(L, clingo_model_symbols_size, self.model, atomset);
        clingo_symbol_t *symbols = static_cast<clingo_symbol_t *>(lua_newuserdata(L, size * sizeof(*symbols))); // +1
        handle_c_error(L, clingo_model_symbols(self.model, atomset, symbols, size));
        lua_createtable(L, numeric_cast<int>(size), 0); // +1
        int i = 1;
        for (auto it = symbols, ie = it + size; it != ie; ++it) {
            Term::new_(L, *it);      // +1
            lua_rawseti(L, -2, i++); // -1
        }
        lua_replace(L, -2); // -1
        return 1;
    }
    static int cost(lua_State *L) {
        auto &self = get_self(L);
        auto size = call_c(L, clingo_model_cost_size, self.model);
        int64_t *costs = static_cast<int64_t *>(lua_newuserdata(L, size * sizeof(*costs))); // +1
        handle_c_error(L, clingo_model_cost(self.model, costs, size));
        lua_createtable(L, numeric_cast<int>(size), 0); // +1
        int i = 1;
        for (auto it = costs, ie = it + size; it != ie; ++it) {
            lua_pushinteger(L, numeric_cast<lua_Integer>(*it)); // +1
            lua_rawseti(L, -2, i++); // -1
        }
        lua_replace(L, -2); // -1
        return 1;
    }
    static int thread_id(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, call_c(L, clingo_model_thread_id, self.model) + 1); // +1
        return 1;
    }
    static int toString(lua_State *L) {
        auto &self = get_self(L);
        std::vector<char> *buf = AnyWrap::new_<std::vector<char>>(L); // +1
        auto printSymbol = [buf, L](std::ostream &out, clingo_symbol_t val) {
            auto size = call_c(L, clingo_symbol_to_string_size, val);
            PROTECT(buf->resize(size));
            handle_c_error(L, clingo_symbol_to_string(val, buf->data(), size));
            PROTECT((out << buf->data(), 0));
        };
        auto printAtom = [L, printSymbol](std::ostream &out, clingo_symbol_t val) {
            if (clingo_symbol_type_function == clingo_symbol_type(val)) {
                auto name = call_c(L, clingo_symbol_name, val);
                auto args = call_c(L, clingo_symbol_arguments, val);
                if (args.second == 2 && strcmp(name, "$") == 0) {
                    printSymbol(out, args.first[0]);
                    out << "=";
                    printSymbol(out, args.first[1]);
                }
                else { printSymbol(out, val); }
            }
            else { printSymbol(out, val); }
        };
        std::ostringstream *oss = AnyWrap::new_<std::ostringstream>(L); // +1
        auto size = call_c(L, clingo_model_symbols_size, self.model, clingo_show_type_shown);
        clingo_symbol_t *symbols = static_cast<clingo_symbol_t *>(lua_newuserdata(L, size * sizeof(*symbols))); // +1
        handle_c_error(L, clingo_model_symbols(self.model, clingo_show_type_shown, symbols, size));
        bool comma = false;
        for (auto it = symbols, ie = it + size; it != ie; ++it) {
            if (comma) { *oss << " "; }
            else       { comma = true; }
            printAtom(*oss, *it);
        }
        std::string *str = AnyWrap::new_<std::string>(L); // +1
        lua_pushstring(L, PROTECT((*str = oss->str(), str->c_str()))); // +1
        lua_replace(L, -5); // -1
        lua_pop(L, 3); // -3
        return 1;
    }
    static int context(lua_State *L) {
        auto &self = get_self(L);
        return SolveControl::new_(L, call_c(L, clingo_model_context, self.model));
    }
    static int extend(lua_State *L) {
        auto &self = get_self(L);
        auto *symbols = luaToVals(L, 2); // +1
        if (!self.mut_model) {
            luaL_error(L, "models can only be extended from on_model callback");
        }
        handle_c_error(L, clingo_model_extend(self.mut_model, symbols->data(), symbols->size()));
        lua_pop(L, 1);                   // -1
        return 0;
    }
    static int index(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "cost") == 0) {
            return cost(L);
        }
        else if (strcmp(name, "context") == 0) {
            return context(L);
        }
        else if (strcmp(name, "thread_id") == 0) {
            return thread_id(L);
        }
        else if (strcmp(name, "number") == 0) {
            lua_pushinteger(L, numeric_cast<lua_Integer>(call_c(L, clingo_model_number, self.model)));
            return 1;
        }
        else if (strcmp(name, "optimality_proven") == 0) {
            lua_pushboolean(L, call_c(L, clingo_model_optimality_proven, self.model));
            return 1;
        }
        else if (strcmp(name, "type") == 0) {
            return ModelType::new_(L, call_c(L, clingo_model_type, self.model));
        }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return 1;
        }
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.Model";
};

constexpr char const *Model::typeName;
luaL_Reg const Model::meta[] = {
    {"__tostring", toString},
    {"symbols", atoms},
    {"contains", contains},
    {"extend", extend},
    {"is_true", is_true},
    {nullptr, nullptr}
};

// {{{1 wrap Statistics

int newStatistics(lua_State *L, clingo_statistics_t const *stats, uint64_t key) {
    switch (call_c(L, clingo_statistics_type, stats, key)) {
        case clingo_statistics_type_value: {
            lua_pushnumber(L, numeric_cast<lua_Number>(call_c(L, clingo_statistics_value_get, stats, key))); // +1
            return 1;
        }
        case clingo_statistics_type_array: {
            lua_newtable(L); // +1
            for (size_t i = 0, e = call_c(L, clingo_statistics_array_size, stats, key); i != e; ++i) {
                newStatistics(L, stats, call_c(L, clingo_statistics_array_at, stats, key, i)); // +1
                lua_rawseti(L, -2, numeric_cast<int>(i+1)); // -1
            }
            return 1;
        }
        case clingo_statistics_type_map: {
            lua_newtable(L); // +1
            for (size_t i = 0, e = call_c(L, clingo_statistics_map_size, stats, key); i != e; ++i) {
                auto name = call_c(L, clingo_statistics_map_subkey_name, stats, key, i);
                lua_pushstring(L, name); // +1
                newStatistics(L, stats, call_c(L, clingo_statistics_map_at, stats, key, name)); // +1
                lua_rawset(L, -3); // -2
            }
            return 1;
        }
        default: {
            return luaL_error(L, "cannot happen");
        }
    }
}

// {{{1 wrap SolveHandle

struct SolveHandle : Object<SolveHandle> {
    clingo_solve_handle_t *handle;
    clingo_solve_mode_bitset_t mode;
    bool hasMH, hasFH;
    clingo_control_t *ctl;
    std::vector<clingo_literal_t> *ass;
    SolveHandle(clingo_solve_handle_t *handle) : handle(handle) { }
    static SolveHandle &get_self(lua_State *L, int offset=1) {
        void *p = nullptr;
        if (lua_istable(L, offset)) {
            lua_rawgeti(L, offset, 1);              // +1
            p = lua_touserdata(L, -1);
            if (p) {
                if (lua_getmetatable(L, offset)) {  // +1
                    luaL_getmetatable(L, typeName); // +1
                    if (!lua_rawequal(L, -1, -2)) { p = nullptr; }
                    lua_pop(L, 2);                  // -2
                }
                else { p = nullptr; }
            }
            lua_pop(L, 1);                          // -1
        }
        if (!p) {
            const char *msg = lua_pushfstring(L, "%s expected, got %s", typeName, luaL_typename(L, 1)); // +1
            luaL_argerror(L, 1, msg);
        }
        return *static_cast<SolveHandle*>(p);
    }
    static SolveHandle *new_(lua_State *L) {
        lua_newtable(L);                         // +1
        auto *self = (SolveHandle*)lua_newuserdata(L, sizeof(SolveHandle)); // +1
        luaL_getmetatable(L, typeNameI);         // +1
        lua_setmetatable(L, -2);                 // +1
        lua_rawseti(L, -2, 1);                   // -1
        luaL_getmetatable(L, typeName);          // +1
        lua_setmetatable(L, -2);                 // -1
        self->handle = nullptr;
        self->mode = 0;
        self->hasFH = false;
        self->hasMH = false;
        self->ctl = nullptr;
        self->ass = nullptr;
        return self;
    }
    static void reg(lua_State *L) {
        lua_regMeta(L, typeName, meta, index, newindex);
        lua_regMeta(L, typeNameI, metaI, nullptr, nullptr);
    }
    static int gc(lua_State *L) {
        return close_(L, *(SolveHandle*)lua_touserdata(L, 1));
    }
    static int close(lua_State *L) {
        return close_(L, get_self(L));
    }
    static int close_(lua_State *L, SolveHandle &self) {
        if (self.handle) {
            auto h = self.handle;
            self.handle = nullptr;
            handle_c_error(L, clingo_solve_handle_close(h));
        }
        return 0;
    }
    static int next(lua_State *L) {
        auto &handle = get_self(L, lua_upvalueindex(1));
        call_c(L, clingo_solve_handle_resume, handle.handle);
        clingo_model_t const *model = call_c(L, clingo_solve_handle_model, handle.handle);
        if (model) { Model::new_(L, model); }
        else       { lua_pushnil(L); }
        return 1;
    }
    static int iter(lua_State *L) {
        get_self(L);
        lua_pushvalue(L,1);
        lua_pushcclosure(L, next, 1);
        return 1;
    }
    static int get(lua_State *L) {
        auto &self = get_self(L);
        SolveResult::new_(L, call_c(L, clingo_solve_handle_get, self.handle));
        return 1;
    }
    static int resume(lua_State *L) {
        auto &self = get_self(L);
        call_c(L, clingo_solve_handle_resume, self.handle);
        return 0;
    }
    static int cancel(lua_State *L) {
        auto &self = get_self(L);
        handle_c_error(L, clingo_solve_handle_cancel(self.handle));
        return 0;
    }
    static int on_model_(lua_State *L) {
        auto m = static_cast<clingo_model_t*>(lua_touserdata(L, 2));
        auto goon = static_cast<bool*>(lua_touserdata(L, 3));
        lua_pushstring(L, "on_model");
        lua_rawget(L, 1);
        Model::new_(L, m);
        lua_call(L, 1, 1);
        *goon = lua_isnil(L, -1) || lua_toboolean(L, -1);
        return 0;
    }
    static int core(lua_State *L) {
        auto core = call_c(L, clingo_solve_handle_core, get_self(L).handle);
        if (core.first == nullptr) {
            lua_pushnil(L);                       // +1
        }
        else {
            cppToLua(L, core.first, core.second); // +1
        }
        return 1;
    }
    static int on_finish_(lua_State *L) {
        lua_pushstring(L, "on_model");
        lua_rawget(L, 1);
        auto x = static_cast<clingo_solve_mode_bitset_t*>(lua_touserdata(L, 2));
        SolveResult::new_(L, *x);
        lua_call(L, 1, 0);
        return 0;
    }
    static bool on_event_(clingo_solve_event_type_t type, void *event, void *data, bool *goon) {
        auto *L = static_cast<lua_State*>(data);
        int top = lua_gettop(L);
        auto &handle = get_self(L, top);
        if (!lua_checkstack(L, 5)) {
            clingo_set_error(clingo_error_bad_alloc, "lua stack size exceeded");
            return false;
        }
        switch (type) {
            case clingo_solve_event_type_model: {
                if (!handle.hasMH) { return true; }
                lua_pushcfunction(L, luaTraceback); // +1
                lua_pushcfunction(L, on_model_);    // +1
                lua_pushvalue(L, top);              // +1
                lua_pushlightuserdata(L, event);    // +1
                lua_pushlightuserdata(L, goon);     // +1
                int code = lua_pcall(L, 3, 0, -5);  // -4|-3
                lua_remove(L, top + 1);             // -1
                return handle_lua_error(L, "on_model", "error in model callback", code); // |-1
            }
            case clingo_solve_event_type_finish: {
                if (!handle.hasFH) { return true; }
                lua_pushcfunction(L, luaTraceback); // +1
                lua_pushcfunction(L, on_finish_);   // +1
                lua_pushvalue(L, top);              // +1
                lua_pushlightuserdata(L, event);    // +1
                int code = lua_pcall(L, 2, 0, -4);  // -3|-2
                lua_remove(L, top + 1);             // -1
                bool ret = handle_lua_error(L, "on_finish", "error in finish callback", code); // |-1
                return ret;
            }
        }
        return true;
    }

    static int solve_(lua_State *L) {
        auto &handle = get_self(L);

        handle.handle = call_c(L, clingo_control_solve, handle.ctl, handle.mode, handle.ass->data(), handle.ass->size(), (handle.hasFH || handle.hasMH) ? on_event_ : nullptr, L);
        if (handle.mode == 0) {
            lua_pushcfunction(L, get); // +1
            lua_pushvalue(L, 1);       // +1
            lua_call(L, 1, 1);         // -1
            auto h = handle.handle;
            handle.handle = nullptr;
            call_c(L, clingo_solve_handle_close, h);
        }
        else {
            lua_pushvalue(L, 1);       // +1
        }

        return 1;
    }
    static luaL_Reg const meta[];
    static luaL_Reg const metaI[];
    static constexpr char const *typeName = "clingo.SolveHandle";
    static constexpr char const *typeNameI = "clingo._SolveHandle";
};

constexpr char const *SolveHandle::typeName;
constexpr char const *SolveHandle::typeNameI;

luaL_Reg const SolveHandle::meta[] = {
    {"iter",  iter},
    {"close", close},
    {"get", get},
    {"core", core},
    {"resume", resume},
    {"cancel", cancel},
    {nullptr, nullptr}
};
luaL_Reg const SolveHandle::metaI[] = {
    {"__gc", gc},
    {nullptr, nullptr}
};

// {{{1 wrap Configuration

struct Configuration : Object<Configuration> {
    clingo_configuration_t *conf;
    clingo_id_t key;
    Configuration(clingo_configuration_t *conf, clingo_id_t key) : conf(conf), key(key) { }

    static int keys(lua_State *L) {
        auto &self = get_self(L);
        auto type = call_c(L, clingo_configuration_type, self.conf, self.key);
        if (type & clingo_configuration_type_map) {
            size_t size = call_c(L, clingo_configuration_map_size, self.conf, self.key);
            lua_createtable(L, numeric_cast<int>(size), 0);
            for (size_t i = 0; i < size; ++i) {
                lua_pushstring(L, call_c(L, clingo_configuration_map_subkey_name, self.conf, self.key, i));
                lua_rawseti(L, -2, numeric_cast<int>(i+1));
            }
        }
        return 1;
    }

    bool get_subkey(lua_State *L, char const *name, clingo_id_t &subkey) {
        if ((call_c(L, clingo_configuration_type, conf, key) & clingo_configuration_type_map) &&
             call_c(L, clingo_configuration_map_has_subkey, conf, key, name)) {
            subkey = call_c(L, clingo_configuration_map_at, conf, key, name);
            return true;
        }
        return false;
    }

    static int description(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        clingo_id_t subkey;
        if (self.get_subkey(L, name, subkey)) {
            lua_pushstring(L, call_c(L, clingo_configuration_description, self.conf, subkey)); // +1
            return 1;
        }
        return luaL_error(L, "unknown option: %s", name);
    }
    static int index(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        lua_getmetatable(L, 1);
        lua_getfield(L, -1, name); // +1
        if (!lua_isnil(L, -1)) { return 1; }
        lua_pop(L, 1); // -1

        if (strcmp(name, "keys") == 0) {
            return keys(L);
        }

        bool desc = strncmp("__desc_", name, 7) == 0;
        if (desc) { name += 7; }

        clingo_id_t subkey;
        if (self.get_subkey(L, name, subkey)) {
            // NOTE: for backward compatibility should be removed in the future
            if (desc) {
                lua_pushstring(L, call_c(L, clingo_configuration_description, self.conf, subkey)); // +1
                return 1;
            }
            else if (call_c(L, clingo_configuration_type, self.conf, subkey) & clingo_configuration_type_value) {
                if (!call_c(L, clingo_configuration_value_is_assigned, self.conf, subkey)) { lua_pushnil(L); return 1; }
                size_t size = call_c(L, clingo_configuration_value_get_size, self.conf, subkey);
                char *ret = static_cast<char*>(lua_newuserdata(L, sizeof(*ret) * size)); // +1
                handle_c_error(L, clingo_configuration_value_get(self.conf, subkey, ret, size));
                lua_pushstring(L, ret); // +1
                lua_replace(L, -2); // -1
                return 1;
            }
            else { return Configuration::new_(L, self.conf, subkey); } // +1
        }

        lua_pushnil(L); // +1
        return 1;
    }

    static int newindex(lua_State *L) {
        auto &self = get_self(L);
        auto *name = luaL_checkstring(L, 2);
        auto subkey = call_c(L, clingo_configuration_map_at, self.conf, self.key, name);
        auto value = lua_tostring(L, 3);
        handle_c_error(L, clingo_configuration_value_set(self.conf, subkey, value));
        return 0;
    }

    static int next(lua_State *L) {
        auto &self = *(Configuration *)luaL_checkudata(L, lua_upvalueindex(1), typeName);
        size_t index = luaL_checkinteger(L, lua_upvalueindex(2));
        lua_pushinteger(L, numeric_cast<lua_Integer>(index + 1));
        lua_replace(L, lua_upvalueindex(2));
        size_t size = call_c(L, clingo_configuration_array_size, self.conf, self.key);
        if (index < size) {
            auto key = call_c(L, clingo_configuration_array_at, self.conf, self.key, index);
            return new_(L, self.conf, key);
        }
        else {
            lua_pushnil(L);
            return 1;
        }
    }

    static int iter(lua_State *L) {
        luaL_checkudata(L, 1, typeName);
        lua_pushvalue(L, 1);
        lua_pushinteger(L, 0);
        lua_pushcclosure(L, next, 2);
        return 1;
    }

    static int len(lua_State *L) {
        auto &self = get_self(L);
        size_t n = 0;
        if (call_c(L, clingo_configuration_type, self.conf, self.key) & clingo_configuration_type_array) {
            n = call_c(L, clingo_configuration_array_size, self.conf, self.key);
        }
        lua_pushinteger(L, numeric_cast<lua_Integer>(n));
        return 1;
    }

    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.Configuration";
};

constexpr char const *Configuration::typeName;
luaL_Reg const Configuration::meta[] = {
    {"__len", len},
    {"iter", iter},
    {"description", description},
    {nullptr, nullptr}
};

// {{{1 wrap wrap Backend

struct ExternalType : Object<ExternalType> {
    using Type = clingo_external_type_t;
    Type type;
    ExternalType(Type type) : type(type) { }
    Type cmpKey() { return type; }
    static int addToRegistry(lua_State *L) {
        lua_createtable(L, 0, 4);
        for (auto t : { clingo_external_type_true, clingo_external_type_false, clingo_external_type_free, clingo_external_type_release }) {
            Object::new_(L, t);
            lua_setfield(L, -2, field_(t));
        }
        lua_setfield(L, -2, "ExternalType");
        return 0;
    }
    static char const *field_(Type t) {
        switch (static_cast<clingo_external_type_e>(t)) {
            case clingo_external_type_true:    { return "True"; }
            case clingo_external_type_false:   { return "False"; }
            case clingo_external_type_free:    { return "Free"; }
            case clingo_external_type_release: { break; }
        }
        return "Release";
    }
    static int new_(lua_State *L, Type t) {
        lua_getfield(L, LUA_REGISTRYINDEX, "clingo");
        lua_getfield(L, -1, "ExternalType");
        lua_replace(L, -2);
        lua_getfield(L, -1, field_(t));
        lua_replace(L, -2);
        return 1;
    }
    static int toString(lua_State *L) {
        lua_pushstring(L, field_(get_self(L).type));
        return 1;
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.ExternalType";
};

constexpr char const *ExternalType::typeName;

luaL_Reg const ExternalType::meta[] = {
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {"__tostring", toString},
    { nullptr, nullptr }
};

struct Backend : Object<Backend> {
    clingo_backend_t *backend;

    Backend(clingo_backend_t *backend) : backend(backend) { }

    static constexpr const char *typeName = "clingo.Backend";
    static luaL_Reg const meta[];

    static int addAtom(lua_State *L) {
        symbol_wrapper sym;
        clingo_symbol_t *symp = nullptr;
        if (!lua_isnone(L, 2) && !lua_isnil(L, 2)) {
            luaToCpp(L, 2, sym);
            symp = &sym.symbol;
        }
        lua_pushinteger(L, call_c(L, clingo_backend_add_atom, get_self(L).backend, symp));
        return 1;
    }

    static int addRule(lua_State *L) {
        auto &self = get_self(L);
        auto *head = AnyWrap::new_<std::vector<clingo_atom_t>>(L);    // +1
        auto *body = AnyWrap::new_<std::vector<clingo_literal_t>>(L); // +1
        bool choice = false;
        luaL_checktype(L, 2, LUA_TTABLE);
        luaPushKwArg(L, 2, 1, "head", false);                         // +1
        luaToCpp(L, -1, *head);
        lua_pop(L, 1);                                                // -1
        luaPushKwArg(L, 2, 2, "body", true);                          // +1
        if (!lua_isnil(L, -1)) { luaToCpp(L, -1, *body); }
        lua_pop(L, 1);                                                // -1
        luaPushKwArg(L, 2, 3, "choice", true);                        // +1
        luaToCpp(L, -1, choice);                                      // -1
        lua_pop(L, 1);
        handle_c_error(L, clingo_backend_rule(self.backend, choice, head->data(), head->size(), body->data(), body->size()));
        lua_pop(L, 2);                                                // -2
        return 0;
    }

    static int addExternal(lua_State *L) {
        auto &self = get_self(L);
        clingo_atom_t atom;
        clingo_external_type_t value = clingo_external_type_false;
        luaToCpp(L, 2, atom);
        if (!lua_isnone(L, 3) && !lua_isnil(L, 3)) {
            value = static_cast<ExternalType*>(luaL_checkudata(L, 3, ExternalType::typeName))->type;
        }
        handle_c_error(L, clingo_backend_external(self.backend, atom, value));
        return 0;
    }

    static int addWeightRule(lua_State *L) {
        auto &self = get_self(L);
        auto *head = AnyWrap::new_<std::vector<clingo_atom_t>>(L);             // +1
        clingo_weight_t lower;
        auto *body = AnyWrap::new_<std::vector<clingo_weighted_literal_t>>(L); // +1
        bool choice = false;
        luaL_checktype(L, 2, LUA_TTABLE);
        luaPushKwArg(L, 2, 1, "head", false);                                  // +1
        luaToCpp(L, -1, *head);
        lua_pop(L, 1);                                                         // -1
        luaPushKwArg(L, 2, 2, "lower", false);                                 // +1
        luaToCpp(L, -1, lower);
        lua_pop(L, 1);                                                         // -1
        luaPushKwArg(L, 2, 3, "body", false);                                  // +1
        luaToCpp(L, -1, *body);
        lua_pop(L, 1);                                                         // -1
        luaPushKwArg(L, 2, 4, "choice", true);                                 // +1
        luaToCpp(L, -1, choice);
        lua_pop(L, 1);                                                         // -1
        handle_c_error(L, clingo_backend_weight_rule(self.backend, choice, head->data(), head->size(), lower, body->data(), body->size()));
        lua_pop(L, 2);                                                         // -2
        return 0;
    }

    static int addMinimize(lua_State *L) {
        auto &self = get_self(L);
        clingo_weight_t priority;
        auto *body = AnyWrap::new_<std::vector<clingo_weighted_literal_t>>(L); // +1
        luaL_checktype(L, 2, LUA_TTABLE);
        luaPushKwArg(L, 2, 1, "priority", false);                              // +1
        luaToCpp(L, -1, priority);
        lua_pop(L, 1);                                                         // -1
        luaPushKwArg(L, 2, 2, "body", false);                                  // +1
        luaToCpp(L, -1, *body);
        lua_pop(L, 1);                                                         // -1
        handle_c_error(L, clingo_backend_minimize(self.backend, priority, body->data(), body->size()));
        lua_pop(L, 1);                                                         // -1
        return 0;
    }

    static int close(lua_State *L) {
        auto &self = get_self(L);
        handle_c_error(L, clingo_backend_end(self.backend));
        return 0;
    }
};

luaL_Reg const Backend::meta[] = {
    {"add_atom", addAtom},
    {"add_rule", addRule},
    {"add_external", addExternal},
    {"add_weight_rule", addWeightRule},
    {"add_minimize", addMinimize},
    {"close", close},
    {nullptr, nullptr}
};

// {{{1 wrap Trail

struct Trail : Object<Trail> {
    clingo_assignment_t const *ass;
    Trail(clingo_assignment_t const *ass) : ass(ass) { }

    int32_t size_(lua_State *L) {
        return call_c(L, clingo_assignment_trail_size, ass);
    }

    clingo_literal_t at_(lua_State *L, uint32_t idx) {
        return call_c(L, clingo_assignment_trail_at, ass, idx);
    }

    static int size(lua_State *L) {
        lua_pushnumber(L, get_self(L).size_(L));
        return 1;
    }

    static int begin(lua_State *L) {
        auto &self = get_self(L);
        auto level = numeric_cast<uint32_t>(luaL_checkinteger(L, 2));
        lua_pushnumber(L, call_c(L, clingo_assignment_trail_begin, self.ass, level) + 1);
        return 1;
    }

    static int end(lua_State *L) {
        auto &self = get_self(L);
        auto level = numeric_cast<uint32_t>(luaL_checkinteger(L, 2));
        lua_pushnumber(L, call_c(L, clingo_assignment_trail_end, self.ass, level));
        return 1;
    }

    static int pairs_iter_(lua_State *L) {
        auto &self = get_self(L);
        auto idx = numeric_cast<int32_t>(luaL_checkinteger(L, 2));
        if (idx < self.size_(L)) {
            lua_pushinteger(L, idx + 1);
            lua_pushnumber(L, self.at_(L, idx));
            return 2;
        }
        return 0;
    }

    static int iter_(lua_State *L) {
        auto &self = *static_cast<Trail *>(luaL_checkudata(L, lua_upvalueindex(1), typeName));
        auto idx = numeric_cast<int32_t>(lua_tointeger(L, lua_upvalueindex(2)));
        if (idx < self.size_(L)) {
            lua_pushinteger(L, idx + 1);
            lua_replace(L, lua_upvalueindex(2));
            lua_pushnumber(L, self.at_(L, idx));
            return 1;
        }
        return 0;
    }

    static int iter(lua_State *L) {
        lua_pushvalue(L, 1);
        lua_pushinteger(L, 0);
        lua_pushcclosure(L, iter_, 2);
        return 1;
    }

    static int pairs(lua_State *L) {
        lua_pushcfunction(L, pairs_iter_);
        lua_pushvalue(L, 1);
        lua_pushinteger(L, 0);
        return 3;
    }

    static int at(lua_State *L) {
        auto &self = get_self(L);
        auto index = numeric_cast<int32_t>(luaL_checkinteger(L, 2)) - 1;
        if (index < self.size_(L)) {
            lua_pushnumber(L, self.at_(L, index));
            return 1;
        }
        return 0;
    }

    static int index(lua_State *L) {
        if (lua_isnumber(L, 2)) { return at(L); }
        char const *name = luaL_checkstring(L, 2);
        lua_getmetatable(L, 1);
        lua_getfield(L, -1, name);
        return 1;
    }

    static constexpr char const *typeName = "clingo.Trail";
    static luaL_Reg const meta[];
};

constexpr char const *Trail::typeName;
luaL_Reg const Trail::meta[] = {
    {"iter", iter},
    {"first", begin},
    {"last", end},
    {"__len", size},
    {"__pairs", pairs},
    {"__ipairs", pairs},
    {nullptr, nullptr}
};

// {{{1 wrap Assignment

struct Assignment : Object<Assignment> {
    clingo_assignment_t const *ass;
    Assignment(clingo_assignment_t const *ass) : ass(ass) { }

    static int hasConflict(lua_State *L) {
        lua_pushboolean(L, clingo_assignment_has_conflict(get_self(L).ass));
        return 1;
    }

    static int decisionLevel(lua_State *L) {
        lua_pushinteger(L, clingo_assignment_decision_level(get_self(L).ass));
        return 1;
    }

    static int rootLevel(lua_State *L) {
        lua_pushinteger(L, clingo_assignment_root_level(get_self(L).ass));
        return 1;
    }

    static int hasLit(lua_State *L) {
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        lua_pushboolean(L, clingo_assignment_has_literal(get_self(L).ass, lit));
        return 1;
    }

    static int level(lua_State *L) {
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        lua_pushinteger(L, call_c(L, clingo_assignment_level, get_self(L).ass, lit));
        return 1;
    }

    static int decision(lua_State *L) {
        auto level = numeric_cast<uint32_t>(luaL_checkinteger(L, 2));
        lua_pushinteger(L, call_c(L, clingo_assignment_decision, get_self(L).ass, level));
        return 1;
    }

    static int isFixed(lua_State *L) {
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        lua_pushboolean(L, call_c(L, clingo_assignment_is_fixed, get_self(L).ass, lit));
        return 1;
    }

    static int isTrue(lua_State *L) {
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        lua_pushboolean(L, call_c(L, clingo_assignment_is_true, get_self(L).ass, lit));
        return 1;
    }

    static int value(lua_State *L) {
        auto &self = get_self(L);
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        auto val = call_c(L, clingo_assignment_truth_value, self.ass, lit);
        if (val == clingo_truth_value_free) { lua_pushnil(L); }
        else { lua_pushboolean(L, val == clingo_truth_value_true); }
        return 1;
    }

    static int isFalse(lua_State *L) {
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        lua_pushboolean(L, call_c(L, clingo_assignment_is_false, get_self(L).ass, lit));
        return 1;
    }

    static int size(lua_State *L) {
        lua_pushnumber(L, clingo_assignment_size(get_self(L).ass));
        return 1;
    }

    static int isTotal(lua_State *L) {
        lua_pushboolean(L, clingo_assignment_is_total(get_self(L).ass));
        return 1;
    }

    int32_t size_() {
        return clingo_assignment_size(ass);
    }

    clingo_literal_t at_(lua_State *L, size_t idx) {
        return call_c(L, clingo_assignment_at, ass, idx);
    }

    static int pairs_iter_(lua_State *L) {
        auto &self = get_self(L);
        auto idx = numeric_cast<std::make_signed<size_t>::type>(luaL_checkinteger(L, 2));
        if (0 <= idx && idx < self.size_()) {
            lua_pushinteger(L, idx + 1);
            lua_pushnumber(L, self.at_(L, idx));
            return 2;
        }
        return 0;
    }

    static int iter_(lua_State *L) {
        auto &self = *static_cast<Assignment*>(luaL_checkudata(L, lua_upvalueindex(1), typeName));
        auto idx = numeric_cast<std::make_signed<size_t>::type>(lua_tointeger(L, lua_upvalueindex(2)));
        if (0 <= idx && idx < self.size_()) {
            lua_pushinteger(L, idx + 1);
            lua_replace(L, lua_upvalueindex(2));
            lua_pushnumber(L, self.at_(L, idx));
            return 1;
        }
        return 0;
    }

    static int iter(lua_State *L) {
        lua_pushvalue(L, 1);
        lua_pushinteger(L, 0);
        lua_pushcclosure(L, iter_, 2);
        return 1;
    }

    static int pairs(lua_State *L) {
        lua_pushcfunction(L, pairs_iter_);
        lua_pushvalue(L, 1);
        lua_pushinteger(L, 0);
        return 3;
    }

    static int at(lua_State *L) {
        auto idx = numeric_cast<std::make_signed<size_t>::type>(luaL_checkinteger(L, 2)) - 1;
        auto &self = get_self(L);
        if (0 <= idx && idx < self.size_()) {
            lua_pushnumber(L, self.at_(L, idx));
            return 1;
        }
        return 0;
    }

    static int trail(lua_State *L) {
        return Trail::new_(L, get_self(L).ass);
    }

    static int index(lua_State *L) {
        if (lua_isnumber(L, 2)) { return at(L); }
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "trail")          == 0) { return trail(L); }
        if (strcmp(name, "is_total")       == 0) { return isTotal(L); }
        if (strcmp(name, "has_conflict")   == 0) { return hasConflict(L); }
        if (strcmp(name, "decision_level") == 0) { return decisionLevel(L); }
        if (strcmp(name, "root_level")     == 0) { return rootLevel(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return 1;
        }
    }

    static constexpr char const *typeName = "clingo.Assignment";
    static luaL_Reg const meta[];
};

constexpr char const *Assignment::typeName;
luaL_Reg const Assignment::meta[] = {
    {"has_lit", hasLit},
    {"value", value},
    {"level", level},
    {"is_fixed", isFixed},
    {"is_true", isTrue},
    {"is_false", isFalse},
    {"decision", decision},
    {"iter", iter},
    {"__len", size},
    {"__pairs", pairs},
    {"__ipairs", pairs},
    {nullptr, nullptr}
};

// {{{1 wrap PropagateInit

struct PropagatorCheckMode : Object<PropagatorCheckMode> {
    clingo_propagator_check_mode_e type;
    PropagatorCheckMode(clingo_propagator_check_mode_e type) : type(type) { }
    clingo_propagator_check_mode_e cmpKey() { return type; }
    static int addToRegistry(lua_State *L) {
        lua_createtable(L, 0, 4);
        for (auto t : {clingo_propagator_check_mode_none, clingo_propagator_check_mode_total, clingo_propagator_check_mode_fixpoint, clingo_propagator_check_mode_both}) {
            new_(L, t);
            lua_setfield(L, -2, field_(t));
        }
        lua_setfield(L, -2, "PropagatorCheckMode");
        return 0;
    }
    static char const *field_(clingo_propagator_check_mode_e type) {
        switch (type) {
            case clingo_propagator_check_mode_none:     { return "Off"; }
            case clingo_propagator_check_mode_total:    { return "Total"; }
            case clingo_propagator_check_mode_fixpoint: { return "Fixpoint"; }
            case clingo_propagator_check_mode_both:     { return "Both"; }
        }
        return "";
    }

    static int toString(lua_State *L) {
        lua_pushstring(L, field_(get_self(L).type));
        return 1;
    }

    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.PropagatorCheckMode";
};

constexpr char const *PropagatorCheckMode::typeName;

luaL_Reg const PropagatorCheckMode::meta[] = {
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {"__tostring", toString},
    { nullptr, nullptr }
};

struct PropagateInit : Object<PropagateInit> {
    lua_State *T;
    clingo_propagate_init_t *init;
    PropagateInit(lua_State *T, clingo_propagate_init_t *init) : T(T), init(init) { }

    static int mapLit(lua_State *L) {
        auto &self = get_self(L);
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        lua_pushinteger(L, call_c(L, clingo_propagate_init_solver_literal, self.init, lit));
        return 1;
    }

    static int numThreads(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, clingo_propagate_init_number_of_threads(self.init));
        return 1;
    }
    static int addWatch(lua_State *L) {
        auto &self = get_self(L);
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        if (lua_isnone(L, 3) || lua_isnil(L, 3)) {
            handle_c_error(L, clingo_propagate_init_add_watch(self.init, lit));
        }
        else {
            auto thread_id = numeric_cast<uint32_t>(luaL_checkinteger(L, 3));
            handle_c_error(L, clingo_propagate_init_add_watch_to_thread(self.init, lit, thread_id-1));
        }
        return 0;
    }

    static int addLiteral(lua_State *L) {
        auto &self = get_self(L);
        bool freeze = lua_isnoneornil(L, 1) || lua_toboolean(L, 1);
        lua_pushinteger(L, call_c(L, clingo_propagate_init_add_literal, self.init, freeze));
        return 1;
    }

    static int addClause(lua_State *L) {
        auto &self = get_self(L);
        auto lits = AnyWrap::new_<std::vector<clingo_literal_t>>(L); // +1
        luaL_checktype(L, 2, LUA_TTABLE);
        luaToCpp(L, 2, *lits);
        lua_pushboolean(L, call_c(L, clingo_propagate_init_add_clause, self.init, lits->data(), lits->size()));
        lua_replace(L, -2);                                          // -1
        return 1;
    }

    static int addWeightConstraint(lua_State *L) {
        auto &self = get_self(L);
        luaL_checknumber(L, 2);
        luaL_checktype(L, 3, LUA_TTABLE);
        luaL_checknumber(L, 4);
        clingo_weight_constraint_type_t type = clingo_weight_constraint_type_equivalence;
        if (!lua_isnone(L, 5)) { type = luaL_checknumber(L, 5); }
        bool eq{!lua_isnone(L, 6) && lua_toboolean(L, 6)};
        clingo_literal_t lit;
        clingo_weight_t bound;
        auto lits = AnyWrap::new_<std::vector<clingo_weighted_literal_t>>(L);
                                    // +1
        luaToCpp(L, 2, lit);
        luaToCpp(L, 3, *lits);
        luaToCpp(L, 4, bound);
        lua_pushboolean(L, call_c(L, clingo_propagate_init_add_weight_constraint, self.init, lit, lits->data(), lits->size(), bound, type, eq));
                                    // +1
        lua_replace(L, -2);         // -1
        return 1;
    }

    static int addMinimize(lua_State *L) {
        auto &self = get_self(L);
        luaL_checknumber(L, 2);
        luaL_checknumber(L, 3);
        clingo_literal_t lit;
        clingo_weight_t weight;
        clingo_weight_t priority{0};
        luaToCpp(L, 2, lit);
        luaToCpp(L, 3, weight);
        if (!lua_isnone(L, 4)) {
            luaL_checknumber(L, 4);
            luaToCpp(L, 4, priority);
        }
        call_c(L, clingo_propagate_init_add_minimize, self.init, lit, weight, priority);
        return 0;
    }

    static int propagate(lua_State *L) {
        auto &self = get_self(L);
        lua_pushboolean(L, call_c(L, clingo_propagate_init_propagate, self.init));
        return 1;
    }

    static int getCheckMode(lua_State *L) {
        PropagatorCheckMode::new_(L, static_cast<clingo_propagator_check_mode_e>(clingo_propagate_init_get_check_mode(get_self(L).init)));
        return 1;
    }

    static int setCheckMode(lua_State *L) {
        auto init = get_self(L).init;
        auto mode = static_cast<PropagatorCheckMode*>(luaL_checkudata(L, 3, PropagatorCheckMode::typeName));
        clingo_propagate_init_set_check_mode(init, mode->type);
        return 1;
    }

    static int assignment(lua_State *L) {
        auto &self = get_self(L);
        Assignment::new_(L, clingo_propagate_init_assignment(self.init));
        return 1;
    }

    static int index(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "theory_atoms")               == 0) { return TheoryIter::iter(L, call_c(L, clingo_propagate_init_theory_atoms, self.init)); }
        else if (strcmp(name, "symbolic_atoms")        == 0) { return SymbolicAtoms::new_(L, call_c(L, clingo_propagate_init_symbolic_atoms, self.init)); }
        else if (strcmp(name, "number_of_threads")     == 0) { return numThreads(L); }
        else if (strcmp(name, "check_mode")            == 0) { return getCheckMode(L); }
        else if (strcmp(name, "assignment")            == 0) { return assignment(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return 1;
        }
    }

    static int newindex(lua_State *L) {
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "check_mode")   == 0) { return setCheckMode(L); }
        return luaL_error(L, "unknown field: %s", name);
    }

    static int setState(lua_State *L) {
        auto &self = get_self(L);
        auto id = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        luaL_checkany(L, 3);
        if (id < 1 || id > (int)clingo_propagate_init_number_of_threads(self.init)) {
            luaL_error(L, "invalid solver thread id %d", id);
        }
        lua_xmove(L, self.T, 1);
        lua_rawseti(self.T, 2, id);
        return 0;
    }

    static constexpr char const *typeName = "clingo.PropagateInit";
    static luaL_Reg const meta[];
};

constexpr char const *PropagateInit::typeName;
luaL_Reg const PropagateInit::meta[] = {
    {"solver_literal", mapLit},
    {"add_watch", addWatch},
    {"add_literal", addLiteral},
    {"add_clause", addClause},
    {"add_weight_constraint", addWeightConstraint},
    {"add_minimize", addMinimize},
    {"propagate", propagate},
    {"set_state", setState},
    {nullptr, nullptr}
};

// {{{1 wrap PropagateControl

struct PropagateControl : Object<PropagateControl> {
    clingo_propagate_control_t* ctl;
    PropagateControl(clingo_propagate_control_t* ctl) : ctl(ctl) { }

    static int id(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, numeric_cast<lua_Integer>(clingo_propagate_control_thread_id(self.ctl)) + 1);
        return 1;
    }

    static int assignment(lua_State *L) {
        auto &self = get_self(L);
        Assignment::new_(L, clingo_propagate_control_assignment(self.ctl));
        return 1;
    }

    static int addClauseOrNogood(lua_State *L, bool invert) {
        auto &self = get_self(L);
        lua_pushinteger(L, 1);                                       // +1
        lua_gettable(L, 2);                                          // +0
        luaL_checktype(L, -1, LUA_TTABLE);                           // +0
        int lits_index = lua_gettop(L);
        auto lits = AnyWrap::new_<std::vector<clingo_literal_t>>(L); // +1
        lua_pushnil(L);                                              // +1
        while (lua_next(L, -3)) {                                    // -1
            auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, -1)); // +0
            PROTECT(lits->emplace_back(lit));
            lua_pop(L, 1);
        }
        clingo_clause_type_t type = 0;
        lua_getfield(L, 2, "tag");                                   // +1
        if (lua_toboolean(L, -1)) {
            type |= clingo_clause_type_volatile;
        }
        lua_pop(L, 1);                                               // -1
        lua_getfield(L, 2, "lock");                                  // +1
        if (lua_toboolean(L, -1)) {
            type |= clingo_clause_type_static;
        }
        lua_pop(L, 1);                                               // -1
        if (invert) {
            for (auto &lit : *lits) { lit = -lit; }
        }
        lua_pushboolean(L, call_c(L, clingo_propagate_control_add_clause, self.ctl, lits->data(), lits->size(), type)); // +1
        lua_replace(L, lits_index);
        lua_settop(L, lits_index);
        return 1;
    }

    static int addLiteral(lua_State *L) {
        auto &self = get_self(L);
        lua_pushinteger(L, call_c(L, clingo_propagate_control_add_literal, self.ctl));
        return 1;
    }

    static int addWatch(lua_State *L) {
        auto &self = get_self(L);
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        handle_c_error(L, clingo_propagate_control_add_watch(self.ctl, lit));
        return 0;
    }

    static int removeWatch(lua_State *L) {
        auto &self = get_self(L);
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        clingo_propagate_control_remove_watch(self.ctl, lit);
        return 0;
    }

    static int hasWatch(lua_State *L) {
        auto &self = get_self(L);
        auto lit = numeric_cast<clingo_literal_t>(luaL_checkinteger(L, 2));
        lua_pushboolean(L, clingo_propagate_control_has_watch(self.ctl, lit));
        return 1;
    }

    static int addClause(lua_State *L) {
        return addClauseOrNogood(L, false);
    }

    static int addNogood(lua_State *L) {
        return addClauseOrNogood(L, true);
    }

    static int propagate(lua_State *L) {
        auto &self = get_self(L);
        lua_pushboolean(L, call_c(L, clingo_propagate_control_propagate, self.ctl)); // +1
        return 1;
    }

    static int index(lua_State *L) {
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "thread_id") == 0) { return id(L); }
        if (strcmp(name, "assignment") == 0) { return assignment(L); }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return 1;
        }
    }

    static constexpr char const *typeName = "clingo.PropagateControl";
    static luaL_Reg const meta[];
};

constexpr char const *PropagateControl::typeName;
luaL_Reg const PropagateControl::meta[] = {
    {"add_literal", addLiteral},
    {"add_watch", addWatch},
    {"has_watch", hasWatch},
    {"remove_watch", removeWatch},
    {"add_clause", addClause},
    {"add_nogood", addNogood},
    {"propagate", propagate},
    {nullptr, nullptr}
};

// }}}
// {{{1 wrap Propagator

class Propagator {
public:
    enum Indices : int { PropagatorIndex=1, StateIndex=2, ThreadIndex=3 };
    Propagator(lua_State *L, lua_State *T) : L(L), T(T) { }
    static int init_(lua_State *L) {
        auto *self = static_cast<Propagator*>(lua_touserdata(L, 1));
        auto *init = static_cast<clingo_propagate_init_t*>(lua_touserdata(L, 2));
        PROTECT(self->threads.reserve(clingo_propagate_init_number_of_threads(init)));
        while (self->threads.size() < static_cast<size_t>(clingo_propagate_init_number_of_threads(init))) {
            self->threads.emplace_back(lua_newthread(L));
            lua_xmove(L, self->T, 1);
            lua_rawseti(self->T, ThreadIndex, numeric_cast<int>(self->threads.size()));
        }
        lua_pushvalue(self->T, PropagatorIndex);         // +1
        lua_xmove(self->T, L, 1);                        // +0
        lua_getfield(L, -1, "init");                     // +1
        if (!lua_isnil(L, -1)) {
            lua_insert(L, -2);
            PropagateInit::new_(L, self->T, init);       // +1
            lua_call(L, 2, 0);                           // -3
        }
        else { lua_pop(L, 2); }                          // -2
        return 0;
    }
    static bool init(clingo_propagate_init_t *init, void *data) {
        auto *self = static_cast<Propagator*>(data);
        if (!lua_checkstack(self->L, 4)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        lua_pushcfunction(self->L, luaTraceback); // +1
        int err = lua_gettop(self->L);
        lua_pushcfunction(self->L, init_);        // +1
        lua_pushlightuserdata(self->L, self);     // +1
        lua_pushlightuserdata(self->L, init);     // +1
        auto ret = lua_pcall(self->L, 2, 0, err); // -3
        lua_remove(self->L, err);                 // -1
        return handle_lua_error(self->L, "Propagator::init", "initializing the propagator failed", ret);
    }
    static int getChanges(lua_State *L, clingo_literal_t const *changes, size_t size) {
        lua_newtable(L);
        for (size_t i = 0; i < size; ++i) {
            lua_pushinteger(L, *(changes + i));
            lua_rawseti(L, -2, numeric_cast<int>(i+1));
        }
        return 1;
    }
    static int getState(lua_State *L, lua_State *T, clingo_id_t id) {
        lua_rawgeti(T, StateIndex, id+1);
        lua_xmove(T, L, 1);
        return 1;
    }
    static int propagate_(lua_State *L) {
        auto *self = static_cast<Propagator*>(lua_touserdata(L, 1));
        auto *control = static_cast<clingo_propagate_control_t *>(lua_touserdata(L, 2));
        auto *changes = static_cast<clingo_literal_t const *>(lua_touserdata(L, 3));
        auto size = lua_tointeger(L, 4);
        lua_pushvalue(self->T, PropagatorIndex);         // +1
        lua_xmove(self->T, L, 1);                        // +0
        lua_getfield(L, -1, "propagate");                // +1
        if (!lua_isnil(L, -1)) {
            lua_insert(L, -2);
            PropagateControl::new_(L, control);          // +1
            getChanges(L, changes, size);                // +1
            getState(L, self->T, clingo_propagate_control_thread_id(control)); // +1
            lua_call(L, 4, 0);                           // -5
        }
        else {
            lua_pop(L, 2);                               // -2
        }
        return 0;
    }
    static bool propagate(clingo_propagate_control_t *control, clingo_literal_t const *changes, size_t size, void *data) {
        auto *self = static_cast<Propagator*>(data);
        lua_State *L = self->threads[clingo_propagate_control_thread_id(control)];
        if (!lua_checkstack(L, 6)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        LuaClear ll(self->T), lt(L);
        lua_pushcfunction(L, luaTraceback);
        lua_pushcfunction(L, propagate_);
        lua_pushlightuserdata(L, self);
        lua_pushlightuserdata(L, control);
        lua_pushlightuserdata(L, const_cast<clingo_literal_t *>(changes));
        lua_pushinteger(L, size);
        auto ret = lua_pcall(L, 4, 0, -6);
        return handle_lua_error(L, "Propagator::propagate", "propagate failed", ret);
    }
    static int undo_(lua_State *L) {
        auto *self = static_cast<Propagator*>(lua_touserdata(L, 1));
        auto *control = static_cast<clingo_propagate_control_t*>(lua_touserdata(L, 2));
        auto *changes = static_cast<clingo_literal_t const *>(lua_touserdata(L, 3));
        auto size = lua_tointeger(L, 4);
        lua_pushvalue(self->T, PropagatorIndex);         // +1
        lua_xmove(self->T, L, 1);                        // +0
        lua_getfield(L, -1, "undo");                     // +1
        if (!lua_isnil(L, -1)) {
            auto id = clingo_propagate_control_thread_id(control);
            lua_insert(L, -2);
            lua_pushinteger(L, id + 1);                  // +1
            Assignment::new_(L, clingo_propagate_control_assignment(control));  // +1
            getChanges(L, changes, size);                // +1
            getState(L, self->T, id);                    // +1
            lua_call(L, 5, 0);                           // -6
        }
        else {
            lua_pop(L, 2);                               // -2
        }
        return 0;
    }
    static void undo(clingo_propagate_control_t const *control, clingo_literal_t const *changes, size_t size, void *data) {
        auto *self = static_cast<Propagator*>(data);
        lua_State *L = self->threads[clingo_propagate_control_thread_id(control)];
        if (!lua_checkstack(L, 6)) {
            std::cerr << "propagator: error in undo going to abort:\n" << "lua stack size exceeded" << std::endl;
            std::abort();
        }
        LuaClear ll(self->T), lt(L);
        lua_pushcfunction(L, luaTraceback);
        lua_pushcfunction(L, undo_);
        lua_pushlightuserdata(L, self);
        lua_pushlightuserdata(L, const_cast<clingo_propagate_control_t*>(control));
        lua_pushlightuserdata(L, const_cast<clingo_literal_t*>(changes));
        lua_pushinteger(L, size);
        auto ret = lua_pcall(L, 4, 0, -6);
        if (ret != 0) {
            char const *msg = lua_tostring(L, -1);
            std::cerr << "propagator: error in undo going to abort:\n" << msg << std::endl;
            std::abort();
        }
    }
    static int check_(lua_State *L) {
        auto *self = static_cast<Propagator*>(lua_touserdata(L, 1));
        auto *solver = static_cast<clingo_propagate_control_t*>(lua_touserdata(L, 2));
        lua_pushvalue(self->T, PropagatorIndex);         // +1
        lua_xmove(self->T, L, 1);                        // +0
        lua_getfield(L, -1, "check");                    // +1
        if (!lua_isnil(L, -1)) {
            lua_insert(L, -2);                           // -1
            PropagateControl::new_(L, solver);           // +1
            getState(L, self->T, clingo_propagate_control_thread_id(solver)); // +1
            lua_call(L, 3, 0);                           // -4
        }
        else {
            lua_pop(L, 2);                               // -2
        }
        return 0;
    }
    static bool check(clingo_propagate_control_t *control, void *data) {
        auto *self = static_cast<Propagator*>(data);
        lua_State *L = self->threads[clingo_propagate_control_thread_id(control)];
        if (!lua_checkstack(L, 4)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        LuaClear ll(self->T), lt(L);
        lua_pushcfunction(L, luaTraceback);
        lua_pushcfunction(L, check_);
        lua_pushlightuserdata(L, self);
        lua_pushlightuserdata(L, control);
        auto ret = lua_pcall(L, 2, 0, -4);
        return handle_lua_error(L, "Propagator::check", "check failed", ret);
    }
    static int decide_(lua_State *L) {
        auto *self = static_cast<Propagator*>(lua_touserdata(L, 1));
        auto thread_id = lua_tointeger(L, 2);
        auto *assignment = static_cast<clingo_assignment_t const *>(lua_touserdata(L, 3));
        auto *decision = static_cast<clingo_literal_t *>(lua_touserdata(L, 5));
        lua_pushvalue(self->T, PropagatorIndex); // +1
        lua_xmove(self->T, L, 1);                // +0
        lua_getfield(L, -1, "decide");           // +1
        if (!lua_isnil(L, -1)) {
            lua_insert(L, -2);
            lua_pushinteger(L, thread_id+1);     // +1
            Assignment::new_(L, assignment);     // +1
            lua_pushvalue(L, 4);                 // +1
            getState(L, self->T, thread_id);     // +1
            lua_call(L, 5, 1);                   // -5
            *decision = lua_tointeger(L, -1);
            lua_pop(L, 1);                       // +1
        }
        else {
            lua_pop(L, 2);                       // -2
        }
        return 0;
    }
    static bool decide(clingo_id_t thread_id, clingo_assignment_t const *assignment, clingo_literal_t fallback, void *data, clingo_literal_t *decision) {
        auto *self = static_cast<Propagator*>(data);
        lua_State *L = self->threads[thread_id];
        if (!lua_checkstack(L, 7)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        LuaClear ll(self->T), lt(L);
        lua_pushcfunction(L, luaTraceback);
        lua_pushcfunction(L, decide_);
        lua_pushlightuserdata(L, self); // 1
        lua_pushnumber(L, thread_id);   // 2
        lua_pushlightuserdata(L, const_cast<clingo_assignment_t*>(assignment)); // 3
        lua_pushnumber(L, fallback); // 4
        lua_pushlightuserdata(L, decision); //5
        auto ret = lua_pcall(L, 5, 0, -7);
        return handle_lua_error(L, "Propagator::decide", "decide failed", ret);
    }

    virtual ~Propagator() noexcept = default;
private:
    lua_State *L;
    // global data for the executions stacks below
    // (something similar could be achieved using the registry index + luaL_(un)ref)
    lua_State *T;
    // execution threads for progagators (executed in lock step)
    std::vector<lua_State *> threads;
};

// {{{1 wrap GroundProgramObserver

struct HeuristicType : Object<HeuristicType> {
    using Type = clingo_heuristic_type_t;
    Type type;
    HeuristicType(Type type) : type(type) { }
    Type cmpKey() { return type; }

    static int addToRegistry(lua_State *L) {
        lua_createtable(L, 0, 6);
        for (auto t : { clingo_heuristic_type_level, clingo_heuristic_type_sign, clingo_heuristic_type_factor, clingo_heuristic_type_init, clingo_heuristic_type_true, clingo_heuristic_type_false }) {
            Object::new_(L, t);
            lua_setfield(L, -2, field_(t));
        }
        lua_setfield(L, -2, "HeuristicType");
        return 0;
    }
    static char const *field_(Type t) {
        switch (t) {
            case clingo_heuristic_type_level:  { return "Level"; }
            case clingo_heuristic_type_sign:   { return "Sign"; }
            case clingo_heuristic_type_factor: { return "Factor"; }
            case clingo_heuristic_type_init:   { return "Init"; }
            case clingo_heuristic_type_true:   { return "True"; }
            case clingo_heuristic_type_false:  { break; }
        }
        return "False";
    }
    static int new_(lua_State *L, Type t) {
        lua_getfield(L, LUA_REGISTRYINDEX, "clingo"); // +1
        lua_getfield(L, -1, "HeuristicType");         // +1
        lua_replace(L, -2);                           // -1
        lua_getfield(L, -1, field_(t));               // +1
        lua_replace(L, -2);                           // -1
        return 1;
    }
    static int toString(lua_State *L) {
        lua_pushstring(L, field_(get_self(L).type));
        return 1;
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.HeuristicType";
};

luaL_Reg const HeuristicType::meta[] = {
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {"__tostring", toString},
    { nullptr, nullptr }
};
constexpr char const *HeuristicType::typeName;

class GroundProgramObserver {
public:
    GroundProgramObserver(lua_State *L, lua_State *T) : L(L), T(T) { }

#   define S(fun) fun, "GroundProgramObserver::" fun, "calling " fun " failed"
    static bool init_program(bool incremental, void *data) {
        return call(data, S("init_program"), incremental);
    }
    static bool begin_step(void *data) {
        return call(data, S("begin_step"));
    }
    static bool end_step(void *data) {
        return call(data, S("end_step"));
    }

    static bool rule(bool choice, clingo_atom_t const *head, size_t head_size, clingo_literal_t const *body, size_t body_size, void *data) {
        return call(data, S("rule"), choice, range(head, head_size), range(body, body_size));
    }
    static bool weight_rule(bool choice, clingo_atom_t const *head, size_t head_size, clingo_weight_t lower_bound, clingo_weighted_literal_t const *body, size_t body_size, void *data) {
        return call(data, S("weight_rule"), choice, range(head, head_size), lower_bound, range(body, body_size));
    }
    static bool minimize(clingo_weight_t priority, clingo_weighted_literal_t const* literals, size_t size, void *data) {
        return call(data, S("minimize"), priority, range(literals, size));
    }
    static bool project(clingo_atom_t const *atoms, size_t size, void *data) {
        return call(data, S("project"), range(atoms, size));
    }
    static bool output_atom(clingo_symbol_t symbol, clingo_atom_t atom, void *data) {
        return call(data, S("output_atom"), symbol_wrapper{symbol}, atom);
    }
    static bool output_term(clingo_symbol_t symbol, clingo_literal_t const *condition, size_t size, void *data) {
        return call(data, S("output_term"), symbol_wrapper{symbol}, range(condition, size));
    }
    static bool external(clingo_atom_t atom, clingo_external_type_t type, void *data) {
        return call(data, S("external"), atom, static_cast<clingo_external_type_e>(type));
    }
    static bool assume(clingo_literal_t const *literals, size_t size, void *data) {
        return call(data, S("assume"), range(literals, size));
    }
    static bool heuristic(clingo_atom_t atom, clingo_heuristic_type_t type, int bias, unsigned priority, clingo_literal_t const *condition, size_t size, void *data) {
        return call(data, S("heuristic"), atom, static_cast<clingo_heuristic_type_e>(type), bias, priority, range(condition, size));
    }
    static bool acyc_edge(int node_u, int node_v, clingo_literal_t const *condition, size_t size, void *data) {
        return call(data, S("acyc_edge"), node_u, node_v, range(condition, size));
    }

    static bool theory_term_number(clingo_id_t term_id, int number, void *data) {
        return call(data, S("theory_term_number"), term_id, number);
    }
    static bool theory_term_string(clingo_id_t term_id, char const *name, void *data) {
        return call(data, S("theory_term_string"), term_id, name);
    }
    static bool theory_term_compound(clingo_id_t term_id, int name_id_or_type, clingo_id_t const *arguments, size_t size, void *data) {
        return call(data, S("theory_term_compound"), term_id, name_id_or_type, range(arguments, size));
    }
    static bool theory_element(clingo_id_t element_id, clingo_id_t const *terms, size_t terms_size, clingo_literal_t const *condition, size_t condition_size, void *data) {
        return call(data, S("theory_element"), element_id, range(terms, terms_size), range(condition, condition_size));
    }
    static bool theory_atom(clingo_id_t atom_id_or_zero, clingo_id_t term_id, clingo_id_t const *elements, size_t size, void *data) {
        return call(data, S("theory_atom"), atom_id_or_zero, term_id, range(elements, size));
    }
    static bool theory_atom_with_guard(clingo_id_t atom_id_or_zero, clingo_id_t term_id, clingo_id_t const *elements, size_t size, clingo_id_t operator_id, clingo_id_t right_hand_side_id, void *data) {
        return call(data, S("theory_atom_with_guard"), atom_id_or_zero, term_id, range(elements, size), operator_id, right_hand_side_id);
    }
#   undef S
private:
    template <typename T>
    struct Range {
        T const * first;
        size_t size;
    };
    template <typename T>
    static Range<T> range(T*first, size_t size) {
        return {first, size};
    }
    static void push(lua_State *L, bool b) {
        lua_pushboolean(L, b);
    }
    static void push(lua_State *L, symbol_wrapper b) {
        Term::new_(L, b.symbol);
    }
    static void push(lua_State *L, clingo_external_type_e x) {
        ExternalType::new_(L, x);
    }
    static void push(lua_State *L, clingo_heuristic_type_e x) {
        HeuristicType::new_(L, x);
    }
    static void push(lua_State *L, clingo_weighted_literal_t lit) {
        lua_newtable(L);
        push(L, lit.literal);
        lua_rawseti(L, -2, 1);
        push(L, lit.weight);
        lua_rawseti(L, -2, 2);
    }
    template <class T>
    static void push(lua_State *L, T n, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr) {
        lua_pushinteger(L, n);
    }
    template <class T>
    static void push(lua_State *L, Range<T> span) {
        lua_newtable(L);
        int i = 0;
        for (auto it = span.first, ie = it + span.size; it != ie; ++it) {
            push(L, *it);
            lua_rawseti(L, -2, ++i);
        }
    }
    static void push(lua_State *L, char const *s) {
        lua_pushstring(L, s);
    }

    static void push_args(GroundProgramObserver *) { }
    template <class T, class... U>
    static void push_args(GroundProgramObserver *self, T& arg, U&... args) {
        lua_pushlightuserdata(self->L, &arg);
        push_args(self, args...);
    }

    template <int>
    static void l_push_args(lua_State *, int) { }
    template <int, class T, class... U>
    static void l_push_args(lua_State *L, int i) {
        T *val = static_cast<T*>(lua_touserdata(L, lua_upvalueindex(i)));
        push(L, *val);
        l_push_args<0, U...>(L, i+1);
    }

    template <class... T>
    static int l_call(lua_State *L) {
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        l_push_args<0, T...>(L, 1);
        lua_call(L, (sizeof...(T)+1), 0);
        return 0;
    }

    template <class... Args>
    static bool call(void *data, char const *fun, char const *loc, char const *msg, Args... args) {
        auto self = static_cast<GroundProgramObserver*>(data);
        if (!lua_checkstack(self->L, 3)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        LuaClear t(self->L);
        // get observer on top of stack L
        lua_pushvalue(self->T, 1);
        lua_xmove(self->T, self->L, 1);                    // +1
        int observer = lua_gettop(self->L);
        lua_pushcfunction(self->L, luaTraceback);          // +1
        int handler = lua_gettop(self->L);
        lua_getfield(self->L, -2, fun);                    // +1
        if (!lua_isnil(self->L, -1)) {
            int function = lua_gettop(self->L);
            int n = sizeof...(Args);
            if (!lua_checkstack(self->L, std::max(3,n))) {
                clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
                return false;
            }
            push_args(self, args...);                      // +n
            lua_pushcclosure(self->L, l_call<Args...>, n); // +1-n
            lua_pushvalue(self->L, function);              // +1
            lua_pushvalue(self->L, observer);              // +1
            auto ret = lua_pcall(self->L, 2, 0, handler);
            return handle_lua_error(self->L, loc, msg, ret);
        }
        return true;
    }

private:
    lua_State *L;
    // state where the observer is stored
    lua_State *T;
};

// {{{1 wrap ControlWrap

struct MessageCode : Object<MessageCode> {
    clingo_warning_e type;
    MessageCode(clingo_warning_e type) : type(type) { }
    clingo_warning_e cmpKey() { return type; }
    static int addToRegistry(lua_State *L) {
        lua_createtable(L, 0, 7);
        for (auto t : {clingo_warning_operation_undefined, clingo_warning_runtime_error, clingo_warning_atom_undefined, clingo_warning_file_included, clingo_warning_variable_unbounded, clingo_warning_global_variable, clingo_warning_other}) {
            new_(L, t);
            lua_setfield(L, -2, field_(t));
        }
        lua_setfield(L, -2, "MessageCode");
        return 0;
    }
    static char const *field_(clingo_warning_e type) {
        switch (type) {
            case clingo_warning_operation_undefined: { return "OperationUndefined"; }
            case clingo_warning_runtime_error      : { return "RuntimeError"; }
            case clingo_warning_atom_undefined     : { return "AtomUndefined"; }
            case clingo_warning_file_included      : { return "FileIncluded"; }
            case clingo_warning_variable_unbounded : { return "VariableUnbounded"; }
            case clingo_warning_global_variable    : { return "GlobalVariable"; }
            case clingo_warning_other              : { return "Other"; }
        }
        return "";
    }

    static int toString(lua_State *L) {
        lua_pushstring(L, field_(get_self(L).type));
        return 1;
    }
    static luaL_Reg const meta[];
    static constexpr char const *typeName = "clingo.MessageCode";
};

constexpr char const *MessageCode::typeName;

luaL_Reg const MessageCode::meta[] = {
    {"__eq", eq},
    {"__lt", lt},
    {"__le", le},
    {"__tostring", toString},
    { nullptr, nullptr }
};
static int lua_logger_callback(lua_State *L) {
    char const *str = *static_cast<char const **>(lua_touserdata(L, 3));
    int code = lua_tointeger(L, 2);
    lua_pop(L, 2);                                // +1
    lua_getfield(L, LUA_REGISTRYINDEX, "clingo"); // +1
    lua_getfield(L, -1, "MessageCode");           // +1
    lua_replace(L, -2);                           // -1
    lua_getfield(L, -1, MessageCode::field_(static_cast<clingo_warning_e>(code))); // +1
    lua_replace(L, -2);                           // -1
    lua_pushstring(L, str);                       // +1
    lua_call(L, 2, 0);                            // -3
    return 0;
}

static void logger_callback(clingo_warning_t code, char const *message, void *data) {
    lua_State *L = static_cast<lua_State*>(data);
    if (!lua_checkstack(L, 4)) {
        std::cerr << "logger: stack size exceeded going to abort" << std::endl;
        std::abort();
    }
    lua_pushcfunction(L, luaTraceback);        // +1
    lua_pushcfunction(L, lua_logger_callback); // +1
    lua_pushvalue(L, 1);                       // +1
    lua_pushinteger(L, code);                  // +1
    lua_pushlightuserdata(L, &message);        // +1
    auto ret = lua_pcall(L, 3, 0, -5);         // -4
    if (ret != 0) {
        char const *msg = lua_tostring(L, -1);
        std::cerr << "logger: error in logger going to abort:\n" << msg << std::endl;
        std::abort();
    }
    lua_pop(L, 1);                             // -1
}

struct ControlWrap : Object<ControlWrap> {
    clingo_control_t *ctl;
    bool free;
    std::forward_list<GroundProgramObserver> observers;
    std::forward_list<Propagator> propagators;
    ControlWrap(clingo_control_t *ctl, bool free) : ctl(ctl), free(free) { }
    static ControlWrap &get_self(lua_State *L) {
        void *p = nullptr;
        if (lua_istable(L, 1)) {
            lua_rawgeti(L, 1, 1);                   // +1
            p = lua_touserdata(L, -1);
            if (p) {
                if (lua_getmetatable(L, 1)) {       // +1
                    luaL_getmetatable(L, typeName); // +1
                    if (!lua_rawequal(L, -1, -2)) { p = nullptr; }
                    lua_pop(L, 2);                  // -2
                }
                else { p = nullptr; }
            }
            lua_pop(L, 1);                          // -1
        }
        if (!p) {
            const char *msg = lua_pushfstring(L, "%s expected, got %s", typeName, luaL_typename(L, 1));
            luaL_argerror(L, 1, msg);
        }
        return *static_cast<ControlWrap*>(p);
    }
    struct Context {
        lua_State *L;
        int context;
    };
    static bool on_context(clingo_location_t const *location, char const *name, clingo_symbol_t const *arguments, size_t arguments_size, void *data, clingo_symbol_callback_t symbol_callback, void *symbol_callback_data) {
        auto &ctx = *static_cast<Context*>(data);
        return luacall(ctx.L, location, ctx.context, name, arguments, arguments_size, symbol_callback, symbol_callback_data);
    }
    static int ground(lua_State *L) {
        auto &ctl = get_self(L).ctl;
        luaL_checktype(L, 2, LUA_TTABLE);
        int context = !lua_isnone(L, 3) && !lua_isnil(L, 3) ? 3 : 0;
        using symbol_vector = std::vector<symbol_wrapper>;
        auto cpp_parts = AnyWrap::new_<std::vector<std::pair<std::string, symbol_vector>>>(L);
        luaToCpp(L, 2, *cpp_parts);
        clingo_part_t *parts = static_cast<decltype(parts)>(lua_newuserdata(L, sizeof(*parts) * cpp_parts->size()));
        auto it = parts;
        for (auto &part : *cpp_parts) {
            *it++ = clingo_part_t {
                part.first.c_str(),
                reinterpret_cast<clingo_symbol_t*>(part.second.data()),
                part.second.size()
            };
        }
        Context ctx{L, context};
        handle_c_error(L, clingo_control_ground(ctl, parts, cpp_parts->size(), context ? on_context : nullptr, context ? &ctx : nullptr));
        return 0;
    }
    static int add(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        luaL_checktype(L, 3, LUA_TTABLE);
        char const *prg = luaL_checkstring(L, 4);
        auto vals = AnyWrap::new_<std::vector<std::string>>(L); // +1
        lua_pushnil(L); // +1
        while (lua_next(L, 3) != 0) { // +1/-1
            char const *val = luaL_checkstring(L, -1);
            protect(L, [val,&vals](){ vals->push_back(val); });
            lua_pop(L, 1); // -1
        }
        size_t size = vals->size();
        char const ** params = static_cast<decltype(params)>(lua_newuserdata(L, size * sizeof(*params))); // +1
        auto it = params;
        for (auto &x : *vals) { *it++ = x.c_str(); }
        handle_c_error(L, clingo_control_add(self.ctl, name, params, size, prg));
        lua_pop(L, 2); // -2
        return 0;
    }
    static int load(lua_State *L) {
        auto &self = get_self(L);
        char const *filename = luaL_checkstring(L, 2);
        handle_c_error(L, clingo_control_load(self.ctl, filename));
        return 0;
    }
    static int get_const(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        if (call_c(L, clingo_control_has_const, self.ctl, name)) {
            clingo_symbol_t sym = call_c(L, clingo_control_get_const, self.ctl, name);
            Term::new_(L, sym);
        }
        else { lua_pushnil(L); }
        return 1;
    }
    static int cleanup(lua_State *L) {
        auto &self = get_self(L);
        handle_c_error(L, clingo_control_cleanup(self.ctl));
        return 0;
    }
    static int solve(lua_State *L) {
        auto &self = get_self(L);
        lua_pushstring(L, "statistics"); // +1
        lua_pushnil(L);                  // +1
        lua_rawset(L, 1);                // -2

        auto handle = SolveHandle::new_(L); // +1
        int handleIdx = lua_gettop(L);
        handle->ass = AnyWrap::new_<std::vector<clingo_literal_t>>(L); // +1
        handle->ctl = self.ctl;

        // this can be made more flexible by both accepting a table as well as normal arguments
        if (!lua_isnone(L, 2) && !lua_isnil(L, 2)) {
            luaL_checktype(L, 2, LUA_TTABLE);
            lua_getfield(L, 2, "assumptions"); // +1
            if (!lua_isnil(L, -1)) {
                auto atoms = call_c(L, clingo_control_symbolic_atoms, self.ctl);
                if (auto *lits = luaToLits(L, -1, atoms, false, false)) { // +1/+0
                    handle->ass->swap(*lits);
                    lua_pop(L, 1); // -1
                }
            }
            lua_pop(L, 1);                     // -1

            lua_getfield(L, 2, "yield");       // +1
            if (lua_toboolean(L, -1)) { handle->mode |= clingo_solve_mode_yield; }
            lua_pop(L, 1);                     // -1

            lua_getfield(L, 2, "async");       // +1
            if (lua_toboolean(L, -1)) { handle->mode |= clingo_solve_mode_async; }
            lua_pop(L, 1);                     // -1

            lua_pushstring(L, "on_model");     // +1
            lua_getfield(L, 2, "on_model");    // +1
            handle->hasMH = !lua_isnil(L, -1);
            lua_rawset(L, handleIdx);          // -2

            lua_pushstring(L, "on_finish");    // +1
            lua_getfield(L, 2, "on_finish");   // +1
            handle->hasFH = !lua_isnil(L, -1);
            lua_rawset(L, handleIdx);          // -2

        }

        // Note: This is fixable but unfortunately quite involved.
        if ((handle->hasFH || handle->hasMH) && (handle->mode & clingo_solve_mode_yield)) {
            return luaL_error(L, "callbacks and iterative solving cannot be used together at the moment.");
        }

        // Note: Asynchronous solving is possible; only callbacks are
        // troublesome. For simplicity it is disabled for now.
        if (handle->mode & clingo_solve_mode_async) {
            return luaL_error(L, "asynchronous solving not supported");
        }

        lua_settop(L, handleIdx + 1);

        if (!lua_checkstack(L, 3)) { luaL_error(L, "lua stack size exceeded"); }
        lua_pushcfunction(L, luaTraceback);        // +1
        lua_pushcfunction(L, SolveHandle::solve_); // +1
        lua_pushvalue(L, handleIdx);               // +1
        int code = lua_pcall(L, 1, 1, -3);         // -1
        if (code) {
            auto h = handle->handle;
            handle->handle = NULL;
            call_c(L, clingo_solve_handle_close, h);
        }
        if (code) { lua_error(L); }
        lua_replace(L, handleIdx);
        lua_settop(L, handleIdx);
        return 1;
    }
    static int assign_external(lua_State *L) {
        auto &self = get_self(L);
        auto atoms = call_c(L, clingo_control_symbolic_atoms, self.ctl);
        auto sym  = luaToAtom(L, 2, atoms);
        luaL_checkany(L, 3);
        clingo_truth_value_t truth;
        if (lua_isnil (L, 3)) { truth = clingo_truth_value_free; }
        else {
            luaL_checktype(L, 3, LUA_TBOOLEAN);
            truth = lua_toboolean(L, 3) ? clingo_truth_value_true : clingo_truth_value_false;
        }
        handle_c_error(L, clingo_control_assign_external(self.ctl, sym, truth));
        return 0;
    }
    static int release_external(lua_State *L) {
        auto &self = get_self(L);
        auto atoms = call_c(L, clingo_control_symbolic_atoms, self.ctl);
        auto sym  = luaToAtom(L, 2, atoms);
        handle_c_error(L, clingo_control_release_external(self.ctl, sym));
        return 0;
    }
    static int interrupt(lua_State *L) {
        clingo_control_interrupt(get_self(L).ctl);
        return 0;
    }
    static int newindex(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "enable_enumeration_assumption") == 0) {
            bool enabled = lua_toboolean(L, 3) != 0;
            handle_c_error(L, clingo_control_set_enable_enumeration_assumption(self.ctl, enabled));
            return 0;
        }
        else if (strcmp(name, "enable_cleanup") == 0) {
            bool enabled = lua_toboolean(L, 3) != 0;
            handle_c_error(L, clingo_control_set_enable_cleanup(self.ctl, enabled));
            return 0;
        }
        return luaL_error(L, "unknown field: %s", name);
    }

    static int index(lua_State *L) {
        auto &self = get_self(L);
        char const *name = luaL_checkstring(L, 2);
        if (strcmp(name, "statistics") == 0) {
            lua_pushstring(L, "statistics");            // +1
            lua_rawget(L, 1);                           // +0
            if (lua_isnil(L, -1)) {
                auto stats = call_c(L, clingo_control_statistics, self.ctl);
                auto root = call_c(L, clingo_statistics_root, stats);
                lua_pop(L, 1);                          // -1
                newStatistics(L, stats, root);          // +1
                lua_pushstring(L, "statistics");        // +1
                lua_pushvalue(L, -2);                   // +1
                lua_rawset(L, 1);                       // -2
            }
            return 1;
        }
        else if (strcmp(name, "configuration") == 0) {
            auto conf = call_c(L, clingo_control_configuration, self.ctl);
            auto key = call_c(L, clingo_configuration_root, conf);
            return Configuration::new_(L, conf, key);
        }
        else if (strcmp(name, "symbolic_atoms") == 0) {
            auto atoms = call_c(L, clingo_control_symbolic_atoms, self.ctl);
            return SymbolicAtoms::new_(L, atoms);
        }
        else if (strcmp(name, "theory_atoms") == 0) {
            auto atoms = call_c(L, clingo_control_theory_atoms, self.ctl);
            return TheoryIter::iter(L, atoms);
        }
        else if (strcmp(name, "is_conflicting") == 0) {
            lua_pushboolean(L, clingo_control_is_conflicting(self.ctl));
            return 1;
        }
        else if (strcmp(name, "enable_enumeration_assumption") == 0) {
            lua_pushboolean(L, clingo_control_get_enable_enumeration_assumption(self.ctl));
            return 1;
        }
        else if (strcmp(name, "enable_cleanup") == 0) {
            lua_pushboolean(L, clingo_control_get_enable_cleanup(self.ctl));
            return 1;
        }
        else {
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, name);
            return 1;
        }
    }
    static int new_(lua_State *L) {
        bool has_parameters = !lua_isnone(L, 1) && !lua_isnil(L, 1);
        bool has_logger = !lua_isnone(L, 2) && !lua_isnil(L, 2);
        bool has_limit = !lua_isnone(L, 3) && !lua_isnil(L, 3);
        std::vector<std::string> *args = AnyWrap::new_<std::vector<std::string>>(L);
        if (has_parameters) { luaToCpp(L, 1, *args); }
        std::vector<char const *> *cargs = AnyWrap::new_<std::vector<char const*>>(L);
        int message_limit = 20;
        if (has_limit) { luaToCpp(L, 3, message_limit); }
        for (auto &arg : *args) {
            protect(L, [&arg, &cargs](){ cargs->push_back(arg.c_str()); });
        }
        return new_(L, [&](void *mem){
            lua_State *T = nullptr;
            if (has_logger) {
                lua_pushstring(L, "logger");
                T = lua_newthread(L);
                lua_pushvalue(L, 2);
                lua_xmove(L, T, 1);
                lua_rawset(L, -3);
            }
            new (mem) ControlWrap(call_c(L, clingo_control_new, cargs->data(), cargs->size(), has_logger ? logger_callback : nullptr, T, message_limit), true);
        });
    }
    template <class F>
    static int new_(lua_State *L, F f) {
        lua_newtable(L);                                                   // +1
        auto self = (ControlWrap*)lua_newuserdata(L, sizeof(ControlWrap)); // +1
        // see: https://stackoverflow.com/questions/27426704/lua-5-1-workaround-for-gc-metamethod-for-tables
        luaL_getmetatable(L, typeNameI);                                   // +1
        lua_setmetatable(L, -2);                                           // -1
        lua_rawseti(L, -2, 1);                                             // -1
        protect(L, [self, f]() { f(self); });
        luaL_getmetatable(L, typeName);                                    // +1
        lua_setmetatable(L, -2);                                           // -1
        return 1;
    }
    static void reg(lua_State *L) {
        lua_regMeta(L, typeName, meta, index, newindex);
        lua_regMeta(L, typeNameI, metaI, nullptr, nullptr);
    }

    static int gc(lua_State *L) {
        auto &self = *(ControlWrap*)lua_touserdata(L, 1);
        if (self.free) { clingo_control_free(self.ctl); }
        self.~ControlWrap();
        return 0;
    }
    static bool hasField(lua_State *L, char const *name, int index) {
        lua_getfield(L, index, name); // +1
        bool ret = !lua_isnil(L, -1);
        lua_pop(L, 1);                // -1
        return ret;
    }
    static int registerPropagator(lua_State *L) {
        auto &self = get_self(L);
        lua_pushstring(L, "propagators");     // +1
        lua_rawget(L, 1);                     // +0
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);                    // -1
            lua_newtable(L);                  // +1
            lua_pushstring(L, "propagators"); // +1
            lua_pushvalue(L, -2);             // +1
            lua_rawset(L, 1);                 // -2
        }
        auto *T = lua_newthread(L);           // +1
        luaL_ref(L, -2);                      // -1
        lua_pop(L, 1);                        // -1
        lua_pushvalue(L, 2);                  // +1
        lua_xmove(L, T, 1);                   // -1
        lua_newtable(T);
        lua_newtable(T);

        clingo_propagator_t propagator = {
            hasField(L, "init", 2) ? Propagator::init : nullptr,
            hasField(L, "propagate", 2) ? Propagator::propagate : nullptr,
            hasField(L, "undo", 2) ? Propagator::undo : nullptr,
            hasField(L, "check", 2) ? Propagator::check : nullptr,
            hasField(L, "decide", 2) ? Propagator::decide : nullptr,
        };
        PROTECT(self.propagators.emplace_front(L, T));
        handle_c_error(L, clingo_control_register_propagator(self.ctl, &propagator, &self.propagators.front(), true));
        return 0;
    }

    static int backend(lua_State *L) {
        auto &self = get_self(L);
        auto backend = call_c(L, clingo_control_backend, self.ctl);
        if (!backend) { return luaL_error(L, "backend not available"); }
        call_c(L, clingo_backend_begin, backend);
        return Backend::new_(L, backend);
    }

    static int registerObserver(lua_State *L) {
        bool replace = lua_toboolean(L, 3) != 0;
        auto &self = get_self(L);
        lua_pushstring(L, "observers");     // +1
        lua_rawget(L, 1);                   // +0
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);                  // -1
            lua_newtable(L);                // +1
            lua_pushstring(L, "observers"); // +1
            lua_pushvalue(L, -2);           // +1
            lua_rawset(L, 1);               // -2
        }
        auto *T = lua_newthread(L);         // +1
        luaL_ref(L, -2);                    // -1
        lua_pop(L, 1);                      // -1
        lua_pushvalue(L, 2);                // +1
        lua_xmove(L, T, 1);                 // -1

        static clingo_ground_program_observer_t observer = {
            GroundProgramObserver::init_program,
            GroundProgramObserver::begin_step,
            GroundProgramObserver::end_step,
            GroundProgramObserver::rule,
            GroundProgramObserver::weight_rule,
            GroundProgramObserver::minimize,
            GroundProgramObserver::project,
            GroundProgramObserver::output_atom,
            GroundProgramObserver::output_term,
            GroundProgramObserver::external,
            GroundProgramObserver::assume,
            GroundProgramObserver::heuristic,
            GroundProgramObserver::acyc_edge,
            GroundProgramObserver::theory_term_number,
            GroundProgramObserver::theory_term_string,
            GroundProgramObserver::theory_term_compound,
            GroundProgramObserver::theory_element,
            GroundProgramObserver::theory_atom,
            GroundProgramObserver::theory_atom_with_guard
        };
        PROTECT(self.observers.emplace_front(L, T));
        handle_c_error(L, clingo_control_register_observer(self.ctl, &observer, replace, &self.observers.front()));
        return 0;
    }

    static luaL_Reg meta[];
    static luaL_Reg metaI[];
    static constexpr char const *typeName = "clingo.Control";
    static constexpr char const *typeNameI = "clingo._Control";
};

constexpr char const *ControlWrap::typeName;
luaL_Reg ControlWrap::meta[] = {
    {"ground",  ground},
    {"add", add},
    {"load", load},
    {"solve", solve},
    {"cleanup", cleanup},
    {"get_const", get_const},
    {"assign_external", assign_external},
    {"release_external", release_external},
    {"interrupt", interrupt},
    {"register_propagator", registerPropagator},
    {"register_observer", registerObserver},
    {"backend", backend},
    {nullptr, nullptr}
};
luaL_Reg ControlWrap::metaI[] = {
    {"__gc", gc},
    {nullptr, nullptr}
};

int luaMain(lua_State *L) {
    auto ctl = (clingo_control_t*)lua_touserdata(L, 1);
    lua_getglobal(L, "main");
    ControlWrap::new_(L, [ctl](void *mem) { new (mem) ControlWrap(ctl, false); });
    lua_call(L, 1, 0);
    return 0;
}

// {{{1 wrap module functions

int parseTerm(lua_State *L) {
    bool has_logger = !lua_isnone(L, 2) && !lua_isnil(L, 2);
    bool has_limit = !lua_isnone(L, 3) && !lua_isnil(L, 3);
    char const *str = luaL_checkstring(L, 1);
    int message_limit = 20;
    if (has_limit) { luaToCpp(L, 3, message_limit); }
    lua_State *T = nullptr;
    if (has_logger) {
        T = lua_newthread(L);
        lua_pushvalue(L, 2);
        lua_xmove(L, T, 1);
    }
    return Term::new_(L, call_c(L, clingo_parse_term, str, has_logger ? logger_callback : nullptr, T, message_limit));
}

// {{{1 clingo library

int luaopen_clingo(lua_State* L) {
    static luaL_Reg clingoLib[] = {
        {"Function", Term::newFun},
        {"Tuple", Term::newTuple},
        {"Number", Term::newNumber},
        {"String", Term::newString},
        {"Control", ControlWrap::new_},
        {"parse_term", parseTerm},
        {nullptr, nullptr}
    };

    Term::reg(L);
    SymbolType::reg(L);
    MessageCode::reg(L);
    Model::reg(L);
    SolveControl::reg(L);
    SolveHandle::reg(L);
    ControlWrap::reg(L);
    Configuration::reg(L);
    SolveResult::reg(L);
    SymbolicAtoms::reg(L);
    SymbolicAtom::reg(L);
    AnyWrap::reg(L);
    TheoryTermType::reg(L);
    ExternalType::reg(L);
    ModelType::reg(L);
    HeuristicType::reg(L);
    TheoryTerm::reg(L);
    TheoryElement::reg(L);
    TheoryAtom::reg(L);
    PropagateInit::reg(L);
    PropagateControl::reg(L);
    Trail::reg(L);
    Assignment::reg(L);
    Backend::reg(L);
    PropagatorCheckMode::reg(L);

#if LUA_VERSION_NUM < 502
    luaL_register(L, "clingo", clingoLib);
#else
    luaL_newlib(L, clingoLib);
#endif

    lua_pushstring(L, CLINGO_VERSION);
    lua_setfield(L, -2, "__version__");

    SymbolType::addToRegistry(L);
    MessageCode::addToRegistry(L);
    Term::addToRegistry(L);
    TheoryTermType::addToRegistry(L);
    ExternalType::addToRegistry(L);
    ModelType::addToRegistry(L);
    HeuristicType::addToRegistry(L);
    PropagatorCheckMode::addToRegistry(L);

    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "clingo");

    return 1;
}

int luarequire_clingo(lua_State *L) {
    luaL_openlibs(L);
#if LUA_VERSION_NUM < 502
    lua_pushcfunction(L, luaopen_clingo);
    lua_call(L, 0, 1);
#else
    luaL_requiref(L, "clingo", luaopen_clingo, true);
#endif
    return 1;
}

// {{{1 definition of Lua

struct LuaScriptC {
    lua_State *L;
    bool self_init_;
    LuaScriptC(lua_State *L)
    : L(L)
    , self_init_(false)
    { }
    ~LuaScriptC() {
        if (self_init_ && L) { lua_close(L); }
    }
    bool init() {
        if (L) { return true; }
        L = luaL_newstate();
        if (!L) {
            clingo_set_error(clingo_error_runtime, "could not initialize lua interpreter");
            return false;
        }
        self_init_ = true;
        if (!lua_checkstack(L, 2)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        LuaClear lc(L);
        lua_pushcfunction(L, luaTraceback);
        lua_pushcfunction(L, luarequire_clingo);
        int code = lua_pcall(L, 0, 0, -2);
        bool ret = handle_lua_error(L, "main", "could not load clingo module", code);
        return ret;
    }
    static bool execute(clingo_location_t const *loc, char const *code, void *data) {
        auto &self = *static_cast<LuaScriptC*>(data);
        if (!self.init()) { return false; }
        std::string name;
        try {
            std::stringstream oss;
            oss << *loc;
            name = oss.str();
        }
        catch (...) {
            clingo_set_error(clingo_error_bad_alloc, "bad alloc");
            return false;
        }
        if (!lua_checkstack(self.L, 2)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        LuaClear lc(self.L);
        lua_pushcfunction(self.L, luaTraceback);
        int ret = luaL_loadbuffer(self.L, code, std::strlen(code), name.c_str());
        if (!handle_lua_error(self.L, name.c_str(), "parsing lua script failed", ret)) { return false; }
        ret = lua_pcall(self.L, 0, 0, -2);
        return handle_lua_error(self.L, name.c_str(), "running lua script failed", ret);
    }
    static bool call(clingo_location_t const *loc, char const *name, clingo_symbol_t const *arguments, size_t size, clingo_symbol_callback_t symbol_callback, void *symbol_callback_data, void *data) {
        auto &self = *static_cast<LuaScriptC*>(data);
        return luacall(self.L, loc, 0, name, arguments, size, symbol_callback, symbol_callback_data);
    }
    static bool callable(char const * name, bool *ret, void *data) {
        auto &self = *static_cast<LuaScriptC*>(data);
        if (!self.L) {
            *ret = false;
            return true;
        }
        if (!lua_checkstack(self.L, 2)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        LuaClear lc(self.L);
        lua_getglobal(self.L, name);
        *ret = lua_type(self.L, -1) == LUA_TFUNCTION;
        return true;
    }
    static bool main(clingo_control_t *ctl, void *data) {
        auto &self = *static_cast<LuaScriptC*>(data);
        LuaClear lc(self.L);
        if (!lua_checkstack(self.L, 3)) {
            clingo_set_error(clingo_error_runtime, "lua stack size exceeded");
            return false;
        }
        lua_pushcfunction(self.L, luaTraceback);
        lua_pushcfunction(self.L, luaMain);
        lua_pushlightuserdata(self.L, ctl);
        auto ret = lua_pcall(self.L, 1, 0, -3);
        return handle_lua_error(self.L, "main", "error calling main", ret);
    }
    static void free(void *data) {
        delete static_cast<LuaScriptC*>(data);
    }
};

// }}}1

} // namespace

extern "C" int clingo_init_lua_(lua_State *L) {
    return luarequire_clingo(L);
}

extern "C" bool clingo_register_lua_(lua_State *L) {
    auto strip_lua = [](char const *str) {
        return strncmp("Lua ", str, 4) == 0 ? str + 4 : str;
    };
    try {
        clingo_script_t script = {
            LuaScriptC::execute,
            LuaScriptC::call,
            LuaScriptC::callable,
            LuaScriptC::main,
            LuaScriptC::free,
            strip_lua(LUA_RELEASE),
        };
        return clingo_register_script("lua", &script, new LuaScriptC(L));
    }
    catch (...) {
        clingo_set_error(clingo_error_runtime, "could not initialize lua interpreter");
        return false;
    }
}

// }}}1

