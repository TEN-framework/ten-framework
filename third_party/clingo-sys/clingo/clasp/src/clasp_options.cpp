//
// Copyright (c) 2006-present Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
#include <clasp/cli/clasp_options.h>
#include <clasp/minimize_constraint.h>
#include <clasp/lookahead.h>
#include <clasp/unfounded_check.h>
#include <potassco/program_opts/program_options.h>
#include <potassco/program_opts/typed_value.h>
#include <cstring>
#include <cfloat>
#include <fstream>
#include <cctype>
#ifdef _MSC_VER
#pragma warning (disable : 4996)
#endif
#if (defined(__cplusplus) && __cplusplus > 199711) || (defined(_MSC_VER) && _MSC_VER >= 1900)
#define CLASP_NOEXCEPT_X(X) noexcept(X)
#else
#define CLASP_NOEXCEPT_X(X)
#endif
/////////////////////////////////////////////////////////////////////////////////////////
// Helper MACROS
/////////////////////////////////////////////////////////////////////////////////////////
#define SET(x, v)           ( ((x)=(v)) == (v) )
#define SET_LEQ(x, v, m)    ( ((v)<=(m)) && SET((x), (v)) )
#define SET_GEQ(x, v, m)    ( ((v)>=(m)) && SET((x), (v)) )
#define SET_OR_FILL(x, v)   ( SET((x),(v)) || ((x) = 0, (x) = ~(x),true) )
#define SET_OR_ZERO(x,v)    ( SET((x),(v)) || SET((x),uint32(0)) )
#define SET_R(x, v, lo, hi) ( ((lo)<=(v)) && ((v)<=(hi)) && SET((x), (v)) )
#define ITE(c, a, b)        (!!(c) ? (a) : (b))
/////////////////////////////////////////////////////////////////////////////////////////
// Primitive types/functions for string <-> T conversions
/////////////////////////////////////////////////////////////////////////////////////////
namespace bk_lib {
template <class T>
static int xconvert(const char* x, pod_vector<T>& out, const char** errPos, int sep) {
	if (sep == 0) { sep = Potassco::def_sep; }
	typename pod_vector<T>::size_type sz = out.size();
	std::size_t t = Potassco::convert_seq<T>(x, out.max_size() - sz, std::back_inserter(out), static_cast<char>(sep), errPos);
	if (!t) { out.resize(sz); }
	return static_cast<int>(t);
}
template <class T>
static std::string& xconvert(std::string& out, const pod_vector<T>& x) { return Potassco::xconvert(out, x.begin(), x.end()); }
}
namespace Potassco {
struct KV { const char* key; int value; };
static const struct OffType {} off = {};
static int xconvert(const char* x, const OffType&, const char** errPos, int) {
	bool temp = true;
	const char* n = x;
	if (xconvert(n, temp, &n, 0) && !temp) { x = n; }
	if (errPos) { *errPos = x; }
	return int(temp == false);
}
static std::string& xconvert(std::string& out, const OffType&) { return out.append("no"); }

static const KV* findValue(const Span<KV>& map, const char* key, const char** next, const char* sep = ",") {
	std::size_t kLen = std::strcspn(key, sep);
	const KV* needle = 0;
	for (const KV* it = Potassco::begin(map), *end = Potassco::end(map); it != end; ++it) {
		if (strncasecmp(key, it->key, kLen) == 0 && !it->key[kLen]) {
			needle = it;
			key   += kLen;
			break;
		}
	}
	if (next) { *next = key; }
	return needle;
}
static const char* findKey(const Span<KV>& map, int x) {
	for (const KV* it = Potassco::begin(map), *end = Potassco::end(map); it != end; ++it) {
		if (it->value == x) { return it->key; }
	}
	return "";
}

struct ArgString {
	ArgString(const char* x) : in(x), skip(0) { }
	~ArgString() CLASP_NOEXCEPT_X(false) { POTASSCO_ASSERT(!ok() || !*in || off(), "Unused argument!"); }
	bool ok()       const { return in != 0; }
	bool off()      const { return ok() && stringTo(in, Potassco::off); }
	bool empty()    const { return ok() && !*in; }
	operator void*()const { return (void*)in; }
	char peek()     const { return ok() ? in[(*in == skip)] : 0; }
	template <class T>
	ArgString& get(T& x)  {
		if (ok()) {
			const char* next = in + (*in == skip);
			in = xconvert(next, x, &next, 0) != 0 ? next : 0;
			skip = ',';
		}
		return *this;
	}
	const char* in;
	char  skip;
	template <class T>
	struct Opt_t {
		Opt_t(T& x) : obj(&x) {}
		T* obj;
	};
};
template <class T>
inline ArgString::Opt_t<T> opt(T& x) { return ArgString::Opt_t<T>(x); }
template <class T>
inline ArgString& operator>>(ArgString& arg, T& x) { return arg.get(x); }
template <class T>
inline ArgString& operator>>(ArgString& arg, const ArgString::Opt_t<T>& x) { return !arg.empty() ? arg.get(*x.obj) : arg; }

struct StringRef {
	StringRef(std::string& o) : out(&o) {}
	std::string* out;
};
template <class T>
inline StringRef& operator<<(StringRef& str, const T& val) {
	if (!str.out->empty()) { str.out->append(1, ','); }
	xconvert(*str.out, val);
	return str;
}

template <class ET>
struct Set {
	Set(unsigned v = 0) : val(v) {}
	unsigned value() const { return val; }
	unsigned val;
};
// <list_of_keys>|<bitmask>
template <class ET>
static int xconvert(const char* x, Set<ET>& out, const char** errPos, int e) {
	const char* it = x, *next;
	unsigned n, len = 0u; ET v;
	if (xconvert(it, n, &next, e)) {
		const Potassco::Span<Potassco::KV> em = enumMap(static_cast<ET*>(0));
		for (size_t i = 0, sum = 0; i != em.size && !len; ++i) {
			sum |= static_cast<unsigned>(em[i].value);
			len += (n == static_cast<unsigned>(em[i].value)) || (n && (n & sum) == n);
		}
	}
	else {
		for (next = "", n = 0u; xconvert(it + int(*next == ','), v, &next, e); it = next, ++len) {
			n |= static_cast<unsigned>(v);
		}
	}
	if (len)    { out.val = n; it = next; }
	if (errPos) { *errPos = it; }
	return static_cast<int>(len);
}
template <class ET>
static std::string& xconvert(std::string& out, const Set<ET>& x) {
	const Potassco::Span<Potassco::KV> em = enumMap(static_cast<ET*>(0));
	if (unsigned bitset = x.val) {
		for (const KV* k = Potassco::begin(em), *kEnd = Potassco::end(em); k != kEnd; ++k) {
			unsigned ev = static_cast<unsigned>(k->value);
			if (bitset == ev || (ev && (ev & bitset) == ev)) {
				out.append(k->key);
				if ((bitset -= ev) == 0u) { return out; }
				out.append(1, ',');
			}
		}
		return xconvert(out, static_cast<ET>(bitset));
	}
	return xconvert(out, off);
}

}
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// Errors
/////////////////////////////////////////////////////////////////////////////////////////
typedef Potassco::ProgramOptions::ContextError OptionError;
typedef Potassco::ProgramOptions::ValueError   ValueError;
POTASSCO_ATTR_NORETURN void failOption(OptionError::Type type, const std::string& ctx, const std::string& opt,
                                       const std::string& desc = "") {
	using namespace Potassco::ProgramOptions;
	switch (type) {
		case OptionError::unknown_option: throw UnknownOption(ctx, opt);
		case OptionError::ambiguous_option: throw AmbiguousOption(ctx, opt, desc);
		default: throw ContextError(ctx, type, opt, desc);
	}
}

POTASSCO_ATTR_NORETURN void failValue(ValueError::Type type, const std::string& ctx, const std::string& opt,
                                      const std::string& value) {
	using namespace Potassco::ProgramOptions;
	throw ValueError(ctx, type, opt, value);
}
/////////////////////////////////////////////////////////////////////////////////////////
// Enum mappings for clasp types
/////////////////////////////////////////////////////////////////////////////////////////
#define MAP(x, y) {static_cast<const char*>(x), static_cast<int>(y)}
#define DEFINE_ENUM_MAPPING(X, ...) \
static Potassco::Span<Potassco::KV> enumMap(const X*) {\
	static const Potassco::KV map[] = {__VA_ARGS__};\
	return Potassco::toSpan(map, sizeof(map)/sizeof(map[0]));\
}\
static int xconvert(const char* x, X& out, const char** errPos, int) {\
	if (const Potassco::KV* it = Potassco::findValue(enumMap(&out), x, errPos)) { \
		out = static_cast<X>(it->value); \
		return 1;\
	}\
	return 0;\
}\
static std::string& xconvert(std::string& out, X x) { \
	return out.append(Potassco::findKey(enumMap(&x), static_cast<int>(x))); \
}
#define OPTION(k, e, a, d, ...) a
#define CLASP_ALL_GROUPS
#define ARG_EXT(a, X) X
#define ARG(a)
#define NO_ARG
#include <clasp/cli/clasp_cli_options.inl>
namespace Cli {
DEFINE_ENUM_MAPPING(ConfigKey, \
  MAP("auto",   config_default), MAP("frumpy", config_frumpy), MAP("jumpy",  config_jumpy), \
  MAP("tweety", config_tweety) , MAP("handy" , config_handy) ,\
  MAP("crafty", config_crafty) , MAP("trendy", config_trendy), MAP("many", config_many))
}
#undef MAP
#undef DEFINE_ENUM_MAPPING
/////////////////////////////////////////////////////////////////////////////////////////
// Conversion functions for complex clasp types
/////////////////////////////////////////////////////////////////////////////////////////
static int convertRet(int res, const char* in, const char** next) {
	if (next) *next = in;
	return res;
}

static int xconvert(const char* x, ScheduleStrategy& out, const char** errPos, int e) {
	using Potassco::xconvert;
	const char* next = std::strchr(x ? x : "", ',');
	uint32      base = 0;
	if (!next || !xconvert(next+1, base, &next, e) || base == 0) { return convertRet(0, x, errPos); }
	if (strncasecmp(x, "f,", 2) == 0 || strncasecmp(x, "fixed,", 6) == 0) {
		out = ScheduleStrategy::fixed(base);
	}
	else if (strncasecmp(x, "l,", 2) == 0 || strncasecmp(x, "luby,", 5) == 0) {
		uint32 lim = 0;
		if (*next == ',' && !xconvert(next+1, lim, &next, e)) { return convertRet(0, next, errPos); }
		out = ScheduleStrategy::luby(base, lim);
	}
	else if (strncmp(x, "+,", 2) == 0 || strncasecmp(x, "add,", 4) == 0) {
		std::pair<uint32, uint32> arg(0, 0);
		if (*next != ',' || !xconvert(next+1, arg, &next, e)) { return convertRet(0, next, errPos); }
		out = ScheduleStrategy::arith(base, arg.first, arg.second);
	}
	else if (strncmp(x, "x,", 2) == 0 || strncmp(x, "*,", 2) == 0) {
		std::pair<double, uint32> arg(0, 0);
		if (*next != ',' || !xconvert(next+1, arg, &next, e) || arg.first < 1.0) { return convertRet(0, next, errPos); }
		out = ScheduleStrategy::geom(base, arg.first, arg.second);
	}
	else {
		return convertRet(0, x, errPos);
	}
	return convertRet(1, next, errPos);
}
static std::string& xconvert(std::string& out, const ScheduleStrategy& sched) {
	using Potassco::xconvert;
	if (sched.defaulted()){ return xconvert(out, ScheduleStrategy()); }
	if (sched.disabled()) { return out.append("0"); }
	std::size_t t = out.size();
	out.append("f,");
	xconvert(out, sched.base);
	switch (sched.type) {
		case ScheduleStrategy::Geometric:
			out[t] = 'x';
			return xconvert(out.append(1, ','), std::make_pair((double)sched.grow, sched.len));
		case ScheduleStrategy::Arithmetic:
			if (sched.grow) { out[t] = '+'; return xconvert(out.append(1, ','), std::make_pair((uint32)sched.grow, sched.len)); }
			else            { out[t] = 'f'; return out; }
		case ScheduleStrategy::Luby:
			out[t] = 'l';
			if (sched.len) { return xconvert(out.append(1, ','), sched.len); }
			else           { return out; }
		default: POTASSCO_ASSERT(false, "xconvert(ScheduleStrategy): unknown type");
	}
}
static int xconvert(const char* x, RestartSchedule& out, const char** errPos, int e) {
	if (!x || (*x != 'd' && *x != 'D'))
		return xconvert(x, static_cast<ScheduleStrategy&>(out), errPos, e);
	using Potassco::xconvert;
	std::pair<uint32, double> req(0, 0);
	uint32             lim  = 0, sWin = 0;
	MovingAvg::Type    fast = MovingAvg::avg_sma;
	MovingAvg::Type    slow = MovingAvg::avg_sma;
	DynamicLimit::Keep keep = RestartSchedule::keep_never;
	const char*        next = 0;
	if (x[1] != ',' || !xconvert(x + 2, req, &x, e) || req.first == 0 || req.second <= 0.0)
		return convertRet(0, x, errPos);
	if (*x == ',' && !xconvert(x + 1, lim, &x, e))
		return convertRet(0, x, errPos);
	if (*x == ',' && !xconvert(x + 1, fast, &x, e))
		return convertRet(0, x, errPos);
	if (*x == ',' && fast != MovingAvg::avg_sma && xconvert(x + 1, keep, &next, e))
		x = next;
	if (*x == ',' && !xconvert(x + 1, slow, &x, e))
		return convertRet(0, x, errPos);
	if (*x == ',' && slow != MovingAvg::avg_sma && !xconvert(x + 1, sWin, &x, e))
		return convertRet(0, x, errPos);
	out = RestartSchedule::dynamic(req.first, static_cast<float>(req.second), lim, fast, keep, slow, sWin);
	return convertRet(1, x, errPos);
}
static std::string& xconvert(std::string& out, const RestartSchedule& in) {
	if (in.disabled() || !in.isDynamic())
		return xconvert(out, static_cast<const ScheduleStrategy&>(in));
	using Potassco::xconvert;
	xconvert(out.append("d,"), std::make_pair(in.base, in.grow));
	uint32 lbdLim = in.lbdLim();
	MovingAvg::Type fast = in.fastAvg();
	MovingAvg::Type slow = in.slowAvg();
	if (lbdLim || fast != MovingAvg::avg_sma || slow != MovingAvg::avg_sma)
		xconvert(out.append(1, ','), lbdLim);
	if (fast != MovingAvg::avg_sma || slow != MovingAvg::avg_sma)
		xconvert(out.append(1, ','), fast);
	if (fast != MovingAvg::avg_sma && in.keepAvg())
		xconvert(out.append(1, ','), static_cast<RestartSchedule::Keep>(in.keepAvg()));
	if (slow != MovingAvg::avg_sma) {
		xconvert(out.append(1, ','), slow);
		if (in.slowWin()) xconvert(out.append(1, ','), in.slowWin());
	}
	return out;
}

static bool setOptLegacy(OptParams& out, uint32 n) {
	if (n >= 20) { return false; }
	out.type = n < 4  ? OptParams::type_bb : OptParams::type_usc;
	out.algo = n < 4  ? n : 0;
	out.opts = 0u;
	out.kLim = 0u;
	if (n > 3 && (n -= 4u) != 0u) {
		if (test_bit(n, 0)) { out.opts |= OptParams::usc_disjoint; }
		if (test_bit(n, 1)) { out.opts |= OptParams::usc_succinct; }
		if (test_bit(n, 2)) { out.algo = OptParams::usc_pmr; }
		if (test_bit(n, 3)) { out.opts |= OptParams::usc_stratify; }
	}
	return true;
}
static int xconvert(const char* x, OptParams& out, const char** err, int e) {
	using Potassco::xconvert;
	using Potassco::toString;
	const char* it = x, *next;
	unsigned n = 0u, len = 0u;
	OptParams::Type t;
	// clasp-3.0: <n>
	if (xconvert(it, n, &next, e) && setOptLegacy(out, n)) {
		it = next; ++len;
	}
	else if (xconvert(it, t, &next, e)) {
		setOptLegacy(out, uint32(t)*4);
		it = next; ++len;
		if (*it == ',') {
			union { OptParams::BBAlgo bb; OptParams::UscAlgo usc; } algo;
			if (xconvert(it+1, n, &next, e) && setOptLegacy(out, n + (uint32(t)*4))) { // clasp-3.2: (bb|usc),<n>
				it = next; ++len;
			}
			else if (t == OptParams::type_bb && xconvert(it+1, algo.bb, &next, e)) {
				out.algo = algo.bb;
				it = next; ++len;
			}
			else if (t == OptParams::type_usc) {
				Potassco::Set<OptParams::UscOption> opts(0);
				if (xconvert(it+1, algo.usc, &next, e)) {
					out.algo = algo.usc;
					it = next; ++len;
					if (*it == ',' && algo.usc == OptParams::usc_k && xconvert(it + 1, n, &next)) {
						SET_OR_FILL(out.kLim, n);
						it = next; ++len;
					}
				}
				if (*it == ',' && (xconvert(it + 1, Potassco::off, &next, e) || xconvert(it + 1, opts, &next, e))) {
					out.opts = opts.value();
					it = next; ++len;
				}
			}
		}
	}
	if (err) { *err = it; }
	return static_cast<int>(len);
}
static std::string& xconvert(std::string& out, const OptParams& p) {
	xconvert(out, static_cast<OptParams::Type>(p.type));
	if (p.type == OptParams::type_usc) {
		xconvert(out.append(1, ','), static_cast<OptParams::UscAlgo>(p.algo));
		if (p.algo == OptParams::usc_k ) { Potassco::xconvert(out.append(1, ','), p.kLim); }
		if (p.opts) { Potassco::xconvert(out.append(1, ','), Potassco::Set<OptParams::UscOption>(p.opts)); }
	}
	else {
		xconvert(out.append(1, ','), static_cast<OptParams::BBAlgo>(p.algo));
	}
	return out;
}
static int xconvert(const char* x, SatPreParams& out, const char** err, int e) {
	using Potassco::xconvert;
	if (xconvert(x, Potassco::off, err, e)) {
		out = SatPreParams();
		return 1;
	}
	uint32 n, len = 0;
	const char *next;
	if (xconvert(x, n, &next, e) && SET(out.type, n)) {
		x = next; ++len;
		Potassco::KV kv[5] = {{"iter", 0}, {"occ", 0}, {"time", 0}, {"frozen", 0}, {"size", 4000}};
		Potassco::Span<Potassco::KV> map = Potassco::toSpan(kv, 5);
		for (uint32 id = 0; *x == ','; ++id, ++len) {
			const char* it = x;
			if (const Potassco::KV* val = Potassco::findValue(map, it + 1, &next, ":=")) {
				id = static_cast<uint32>(val - kv);
				it = next;
			}
			if (id > 4 || !xconvert(it + 1, kv[id].value, &next, e)) { break; }
			x = next;
		}
		SET_OR_ZERO(out.limIters,  unsigned(kv[0].value));
		SET_OR_ZERO(out.limOcc,    unsigned(kv[1].value));
		SET_OR_ZERO(out.limTime,   unsigned(kv[2].value));
		SET_OR_ZERO(out.limFrozen, unsigned(kv[3].value));
		SET_OR_ZERO(out.limClause, unsigned(kv[4].value));
	}
	if (err) { *err = x; }
	return static_cast<int>(len);
}
static std::string& xconvert(std::string& out, const SatPreParams& p) {
	if (p.type) {
		Potassco::xconvert(out, p.type);
		if (uint32 n = p.limIters)  { Potassco::xconvert(out.append(",iter="), n);   }
		if (uint32 n = p.limOcc)    { Potassco::xconvert(out.append(",occ="), n);    }
		if (uint32 n = p.limTime)   { Potassco::xconvert(out.append(",time="), n);   }
		if (uint32 n = p.limFrozen) { Potassco::xconvert(out.append(",frozen="), n); }
		if (uint32 n = p.limClause) { Potassco::xconvert(out.append(",size="), n); }
		return out;
	}
	else {
		return xconvert(out, Potassco::off);
	}
}
namespace Asp { using Clasp::xconvert; }
namespace mt  { using Clasp::xconvert; }
namespace Cli {
/////////////////////////////////////////////////////////////////////////////////////////
// Option -> Key mapping
/////////////////////////////////////////////////////////////////////////////////////////
namespace {
enum OptionKey {
	detail_before_options = -1,
	meta_config = 0,
#define CLASP_CONTEXT_OPTIONS  GRP(option_category_nodes_end,   option_category_context_begin),
#define CLASP_GLOBAL_OPTIONS   GRP(option_category_context_end, option_category_global_begin),
#define CLASP_SOLVER_OPTIONS   GRP(option_category_global_end,  option_category_solver_begin),
#define CLASP_SEARCH_OPTIONS   GRP(option_category_solver_end,  option_category_search_begin),
#define CLASP_ASP_OPTIONS      GRP(option_category_search_end,  option_category_asp_begin),
#define CLASP_SOLVE_OPTIONS    GRP(option_category_asp_end,     option_category_solve_begin),
#define OPTION(k,e,...) opt_##k,
#define GROUP_BEGIN(X) X
#define GRP(X, Y) X, Y = X, detail_before_##Y = X - 1
#include <clasp/cli/clasp_cli_options.inl>
#undef GRP
	option_category_solve_end,
	detail_num_options = option_category_solve_end,
	meta_tester = detail_num_options
};
#if CLASP_HAS_THREADS
#define MANY_DESC  "        many  : Use default portfolio to configure solver(s)\n"
#define MANY_ARG   "|many"
#else
#define MANY_DESC
#define MANY_ARG   ""
#endif
#define KEY_INIT_DESC(desc) \
desc "      <arg>: {auto|frumpy|jumpy|tweety|handy|crafty|trendy" MANY_ARG "|<file>}\n" \
"        auto  : Select configuration based on problem type\n"                          \
"        frumpy: Use conservative defaults\n"                                           \
"        jumpy : Use aggressive defaults\n"                                             \
"        tweety: Use defaults geared towards asp problems\n"                            \
"        handy : Use defaults geared towards large problems\n"                          \
"        crafty: Use defaults geared towards crafted problems\n"                        \
"        trendy: Use defaults geared towards industrial problems\n"                     \
         MANY_DESC                                                                      \
"        <file>: Use configuration file to configure solver(s)"
struct NodeKey {
	const char* name;
	const char* desc;
	int16       skBeg;
	uint16      skSize;
};
enum { key_leaf = 0, key_solver = -1, key_asp = -2, key_solve = -3, key_tester = -4, key_root = -5 };
struct Name2Id {
	const char *name;
	int key;
	bool operator<(const Name2Id &rhs) const { return *this < rhs.name; }
	bool operator<(const char *rhs) const { return std::strcmp(name, rhs) < 0; }
};
Name2Id index_g[detail_num_options + 1] = {
	{"configuration", meta_config},
#define OPTION(k, e, ...) { #k, opt_##k },
#define CLASP_ALL_GROUPS
#include <clasp/cli/clasp_cli_options.inl>
	{"tester", meta_tester}
};
bool init_index_g = (std::sort(index_g, index_g + detail_num_options + 1), true);
}
/// \cond
// Valid option keys.
static inline bool  isOption(int k)         { return k >= option_category_nodes_end && k < detail_num_options; }
static inline bool  isGlobalOption(int k)   { return k >= option_category_global_begin && k < option_category_global_end; }
static inline bool  isTesterOption(int k)   { return k >= option_category_nodes_end && k < option_category_search_end && !isGlobalOption(k); }
static inline bool  isSolverOption(int k)   { return k >= option_category_solver_begin && k < option_category_search_end; }
static inline int16 decodeKey(uint32 key)   { return static_cast<int16>(static_cast<uint16>(key)); }
static inline uint8 decodeMode(uint32 key)  { return static_cast<uint8>( (key >> 24) ); }
static inline uint8 decodeSolver(uint32 key){ return static_cast<uint8>( (key >> 16) ); }
static inline bool  isValidId(int16 id)     { return id >= key_root && id < detail_num_options; }
static inline bool  isLeafId(int16 id)      { return id >= key_leaf && id < detail_num_options; }
static inline uint32 makeKeyHandle(int16 kId, uint32 mode, uint32 sId) {
	assert(sId <= 255 && mode <= 255);
	return (mode << 24) | (sId << 16) | static_cast<uint16>(kId);
}
static const uint8 mode_solver = 1u;
static const uint8 mode_tester = 2u;
static const uint8 mode_relaxed= 4u;
static const uint8 mode_meta   = 8u;
static inline bool isTester(uint8 mode) { return (mode & mode_tester) != 0; }
static inline bool isSolver(uint8 mode) { return (mode & mode_solver) != 0; }
static inline BasicSatConfig* active(ClaspConfig* config, uint8 mode) {
	return !isTester(mode) ? config : config->testerConfig();
}
static inline const BasicSatConfig* active(const ClaspConfig* config, uint8 mode) {
	return active(const_cast<ClaspConfig*>(config), mode);
}
static int16 findOption(const char* needle, bool prefix) {
	const Name2Id* end = index_g + detail_num_options + 1;
	const Name2Id* it  = std::lower_bound(const_cast<const Name2Id*>(index_g), end, needle);
	int ret            = -1;
	if (it != end) {
		std::size_t len = std::strlen(needle);
		if (std::strncmp(it->name, needle, len) == 0 && (!it->name[len] || prefix)) {
			const Name2Id* next = it + 1;
			ret = !it->name[len] || next == end || std::strncmp(next->name, needle, len) != 0 ? it->key : -2;
		}
	}
	return static_cast<int16>(ret);
}
static NodeKey makeNode(const char* name, const char* desc, int16 skBeg = 0, int16 skEnd = 0) {
	NodeKey n = {name, desc, skBeg, static_cast<uint16>(skEnd - skBeg) };
	return n;
}
static NodeKey getNode(int16 id) {
	assert(isValidId(id));
	switch(id) {
		case key_root  : return makeNode("", "Options", key_tester, option_category_global_end);
		case key_tester: return makeNode("tester", "Tester Options", key_solver, option_category_context_end);
		case key_solve : return makeNode("solve", "Solve Options", option_category_solve_begin, option_category_solve_end);
		case key_asp   : return makeNode("asp", "Asp Options", option_category_asp_begin, option_category_asp_end);
		case key_solver: return makeNode("solver", "Solver Options", option_category_solver_begin, option_category_search_end);
		case key_leaf  : return makeNode("configuration", KEY_INIT_DESC("Initializes this configuration\n"));
		#define OPTION(k, e, a, d, x, v) case opt_##k: return makeNode(#k, d);
		#define CLASP_ALL_GROUPS
		#include <clasp/cli/clasp_cli_options.inl>
		default        : return makeNode("", "");
	}
}
const ClaspCliConfig::KeyType ClaspCliConfig::KEY_INVALID = static_cast<ClaspCliConfig::KeyType>(-1);
const ClaspCliConfig::KeyType ClaspCliConfig::KEY_ROOT    = makeKeyHandle(key_root, 0, 0);
const ClaspCliConfig::KeyType ClaspCliConfig::KEY_SOLVER  = makeKeyHandle(key_solver, 0, 0);
const ClaspCliConfig::KeyType ClaspCliConfig::KEY_TESTER  = makeKeyHandle(key_tester, mode_tester, 0);
/// \endcond
/////////////////////////////////////////////////////////////////////////////////////////
// Interface to ProgramOptions
/////////////////////////////////////////////////////////////////////////////////////////
// Converts option key to command-line option name.
static void keyToCliName(std::string& out, const char* n, const char* ext) {
	out.clear();
	for (const char* x; (x = std::strchr(n, '_')) != 0; n = ++x) {
		out.append(n, x-n);
		out.append(1, '-');
	}
	out.append(n).append(ext);
}
// Converts command-line option name to option key.
static void cliNameToKey(std::string& out, const char* n) {
	out.clear();
	for (const char* x; (x = std::strchr(n, '-')) != 0; n = ++x) {
		out.append(n, x-n);
		out.append(1, '_');
	}
	out.append(n);
}
// Type for storing one command-line option.
// Adapter for parsing a command string.
struct ClaspCliConfig::ParseContext : public Potassco::ProgramOptions::ParseContext{
	typedef Potassco::ProgramOptions::SharedOptPtr OptPtr;
	ParseContext(ClaspCliConfig& x, const char* c, const ParsedOpts* ex, uint8 m, uint32 s, ParsedOpts* o)
		: self(&x), prev(x.parseCtx_), config(c), exclude(ex), out(o), sId(s), mode(m) {
		seen[0] = seen[1] = 0;
		x.parseCtx_ = this;
	}
	~ParseContext() { self->parseCtx_ = this->prev; }
	OptPtr getOption(const char* name, FindType ft);
	OptPtr getOption(int, const char* key) { failOption(OptionError::unknown_option, config, key); }
	void   addValue(const OptPtr& key, const std::string& value);
	uint64            seen[2];
	std::string       temp;
	ClaspCliConfig*   self;
	ParseContext*     prev;
	const char*       config;
	const ParsedOpts* exclude;
	ParsedOpts*       out;
	uint32            sId;
	uint8             mode;
};
class ClaspCliConfig::ProgOption : public Potassco::ProgramOptions::Value {
public:
	ProgOption(ClaspCliConfig& c, int o) : Potassco::ProgramOptions::Value(0), config_(&c), option_(o) {}
	bool doParse(const std::string& opt, const std::string& value) {
		uint8 mode = config_->parseCtx_ ? config_->parseCtx_->mode : 0;
		uint32 sId = config_->parseCtx_ ? config_->parseCtx_->sId  : 0;
		int ret = isOption(option_) ? config_->setOption(option_, mode, sId, value.c_str()) : config_->setAppOpt(option_, mode, value.c_str());
		if (ret == -1) { failOption(OptionError::unknown_option, !isTester(mode) ? "<clasp>" : "<tester>", opt); }
		return ret > 0;
	}
	int option() const { return option_; }
private:
	ClaspCliConfig* config_;
	int             option_;
};
void ClaspCliConfig::ParseContext::addValue(const OptPtr& key, const std::string& value) {
	using namespace Potassco::ProgramOptions;
	if (exclude->count(key->name()) == 0) {
		ProgOption* v = static_cast<ProgOption*>(key->value());
		Value::State s= v->state();
		int        id = v->option();
		uint64&    xs = seen[id/64];
		uint64      m = static_cast<uint64>(1u) << (id & 63);
		if ((xs & m) != 0 && !v->isComposing()){ failValue(ValueError::multiple_occurrences, config, key->name(), value); }
		if (!v->parse(key->name(), value, s))  { failValue(ValueError::invalid_value, config, key->name(), value); }
		if (out) { out->add(key->name()); }
		xs |= m;
	}
}
Potassco::ProgramOptions::SharedOptPtr ClaspCliConfig::ParseContext::getOption(const char* cmdName, FindType ft) {
	Options::option_iterator end   = self->opts_->end(), it = end;
	OptionError::Type        error = OptionError::unknown_option;
	bool                     meta  = (mode & mode_meta) != 0;
	if (ft == OptionContext::find_alias) {
		char a = cmdName[*cmdName == '-'];
		for (it = self->opts_->begin(); it != end && it->get()->alias() != a; ++it) { ; }
	}
	else {
		const char* name = cmdName;
		if (std::strchr(cmdName, '-') != 0) { cliNameToKey(temp, cmdName); name = temp.c_str(); }
		int16 opt = findOption(name, (ft & OptionContext::find_prefix) != 0);
		if      (opt >= 0)  { it = self->opts_->begin() + opt; }
		else if (opt == -2) { error = OptionError::ambiguous_option; }
		assert(it == end || static_cast<const ProgOption*>(it->get()->value())->option() == opt);
	}
	if (it != end && (meta || isOption(static_cast<const ProgOption*>(it->get()->value())->option()))) {
		return *it;
	}
	failOption(error, config, cmdName);
}
/////////////////////////////////////////////////////////////////////////////////////////
// Default Configs
/////////////////////////////////////////////////////////////////////////////////////////
static inline const char* skipWs(const char* x) {
	while (*x == ' ' || *x == '\t') { ++x; }
	return x;
}
static inline const char* getIdent(const char* x, std::string& to) {
	for (x = skipWs(x); std::strchr(" \t:()[]", *x) == 0; ++x) { to += *x; }
	return x;
}
static inline bool matchSep(const char*& x, char c) {
	if (*(x = skipWs(x)) == c) { ++x; return true; }
	return false;
}
static bool appendConfig(std::string& to, const std::string& line) {
	const char* x = skipWs(line.c_str());
	const bool  p = matchSep(x, '[');
	to.append("/[", 2);
	// match name in optional square brackets
	bool ok = matchSep(x = getIdent(x, to), ']') == p;
	to.append("]\0/", 3);
	// match optional base in parentheses followed by start of option list
	if (ok && (!matchSep(x, '(') || matchSep((x = getIdent(x, to)), ')')) && matchSep(x, ':')) {
		to.append("\0/", 2);
		to.append(skipWs(x));
		to.erase(to.find_last_not_of(" \t") + 1);
		to.append(1, '\0');
		return true;
	}
	return false;
}
ConfigIter ClaspCliConfig::getConfig(ConfigKey k) {
	#define MAKE_CONFIG(n, o1, o2) "/[" n "]\0/\0/" o1 " " o2 "\0"
	switch(k) {
		#define CONFIG(id, n,c,s,p) case config_##n: return ConfigIter(MAKE_CONFIG(#n, s, c));
		#define CLASP_CLI_DEFAULT_CONFIGS
		#define CLASP_CLI_AUX_CONFIGS
		#include <clasp/cli/clasp_cli_configs.inl>
		case config_many:
		#define CONFIG(id,n,c,s,p) MAKE_CONFIG("solver." POTASSCO_STRING(id), c, p)
		#define CLASP_CLI_DEFAULT_CONFIGS
		#define CLASP_CLI_AUX_CONFIGS
			return ConfigIter(
				#include <clasp/cli/clasp_cli_configs.inl>
			);
		default: POTASSCO_REQUIRE(k == config_default, "Invalid config key '%d'", (int)k); return ConfigIter("/default\0/\0/\0");
	}
	#undef MAKE_CONFIG
}
ConfigIter ClaspCliConfig::getConfig(uint8 key, std::string& tempMem) const {
	POTASSCO_REQUIRE(key <= (config_max_value + 1), "Invalid key!");
	if (key < config_max_value) { return getConfig(static_cast<ConfigKey>(key)); }
	const char* name = config_[key - config_max_value].c_str();
	std::ifstream file(name);
	POTASSCO_EXPECT(file, "Could not open config file '%s'", name);
	uint32 lineNum = 0;
	tempMem.clear();
	for (std::string line, cont; std::getline(file, line); ) {
		++lineNum;
		line.erase(0, line.find_first_not_of(" \t"));
		if (line.empty() || line[0] == '#') { continue; }
		if (*line.rbegin() == '\\')         { *line.rbegin() = ' '; cont += line; continue; }
		if (!cont.empty()) { cont += line; cont.swap(line); cont.clear(); }
		POTASSCO_EXPECT(appendConfig(tempMem, line), "'%s@%u': Invalid configuration", name, lineNum);
	}
	tempMem.append(1, '\0');
	return ConfigIter(tempMem.data());
}
int ClaspCliConfig::getConfigKey(const char* k) {
	ConfigKey ret;
	return Potassco::string_cast(k, ret) ? ret : -1;
}
const char* ClaspCliConfig::getDefaults(ProblemType t) {
	if (t == Problem_t::Asp){ return "--configuration=tweety"; }
	else                    { return "--configuration=trendy"; }
}
ConfigIter::ConfigIter(const char* x) : base_(x) {}
const char* ConfigIter::name() const { return base_ + 1; }
const char* ConfigIter::base() const { return base_ + std::strlen(base_) + 2; }
const char* ConfigIter::args() const { const char* x = base(); return x + std::strlen(x) + 2; }
bool        ConfigIter::valid()const { return *base_ != 0; }
bool        ConfigIter::next()       {
	base_ = args();
	base_+= std::strlen(base_) + 1;
	return valid();
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspCliConfig
/////////////////////////////////////////////////////////////////////////////////////////
ClaspCliConfig::ClaspCliConfig() : parseCtx_(0), validate_(false) {
	static_assert(
		(option_category_context_begin< option_category_solver_begin) &&
		(option_category_solver_begin < option_category_search_begin) &&
		(option_category_search_begin < option_category_asp_begin)    &&
		(option_category_asp_begin    < option_category_solve_begin)  &&
		(option_category_solve_begin  < option_category_solve_end), "unexpected option order");
}
ClaspCliConfig::~ClaspCliConfig() {}
void ClaspCliConfig::reset() {
	config_[0] = config_[1] = "";
	validate_  = false;
	ClaspConfig::reset();
}
void ClaspCliConfig::prepare(SharedContext& ctx) {
	if (testerConfig()) {
		// Force init
		ClaspCliConfig::config("tester");
	}
	if (validate_) {
		ClaspCliConfig::validate();
	}
	ClaspConfig::prepare(ctx);
}
Configuration* ClaspCliConfig::config(const char* n) {
	if (n && std::strcmp(n, "tester") == 0) {
		if (!testerConfig()) {
			setAppOpt(meta_tester, 0, "");
		}
		return testerConfig();
	}
	return ClaspConfig::config(n);
}

ClaspCliConfig::ProgOption* ClaspCliConfig::createOption(int o) {  return new ProgOption(*this, o); }

void ClaspCliConfig::createOptions() {
	if (opts_.get()) { return; }
	opts_ = new Options();
	using namespace Potassco::ProgramOptions;
	opts_->addOptions()("configuration", createOption(meta_config)->defaultsTo("auto")->state(Value::value_defaulted), KEY_INIT_DESC("Set default configuration [%D]\n"));
	std::string cmdName;
#define CLASP_ALL_GROUPS
#define OPTION(k, e, a, d, ...) keyToCliName(cmdName, #k, e); opts_->addOptions()(cmdName.c_str(),static_cast<ProgOption*>( createOption(opt_##k)a ), d);
#define ARG(a) ->a
#define ARG_EXT(a, X) ARG(a)
#define NO_ARG
#include <clasp/cli/clasp_cli_options.inl>
	opts_->addOptions()("tester", createOption(meta_tester)->arg("<options>"), "Pass (quoted) string of %A to tester");
}
void ClaspCliConfig::addOptions(OptionContext& root) {
	createOptions();
	using namespace Potassco::ProgramOptions;
	OptionGroup configOpts("Clasp.Config Options");
	OptionGroup ctxOpts("Clasp.Context Options", Potassco::ProgramOptions::desc_level_e1);
	OptionGroup solving("Clasp.Solving Options");
	OptionGroup aspOpts("Clasp.ASP Options", Potassco::ProgramOptions::desc_level_e1);
	OptionGroup search("Clasp.Search Options", Potassco::ProgramOptions::desc_level_e1);
	OptionGroup lookback("Clasp.Lookback Options", Potassco::ProgramOptions::desc_level_e1);
	configOpts.addOption(*opts_->begin());
	configOpts.addOption(*(opts_->end()-1));
	for (Options::option_iterator it = opts_->begin() + 1, end = opts_->end() - 1; it != end; ++it) {
		int oId = static_cast<ProgOption*>(it->get()->value())->option();
		if      (isGlobalOption(oId))               { configOpts.addOption(*it);}
		else if (oId < option_category_context_end) { ctxOpts.addOption(*it); }
		else if (oId < opt_no_lookback)             { search.addOption(*it); }
		else if (oId < option_category_solver_end)  { lookback.addOption(*it); }
		else if (oId < opt_restarts)                { search.addOption(*it); }
		else if (oId < option_category_search_end)  { lookback.addOption(*it); }
		else if (oId < option_category_asp_end)     { aspOpts.addOption(*it); }
		else                                        { solving.addOption(*it); }
	}
	root.add(configOpts).add(ctxOpts).add(aspOpts).add(solving).add(search).add(lookback);
	root.addAlias("number", root.find("models")); // remove on next version
	root.addAlias("opt-sat", root.find("parse-maxsat")); // remove on next version
}
bool ClaspCliConfig::assignDefaults(const Potassco::ProgramOptions::ParsedOptions& exclude) {
	for (Options::option_iterator it = opts_->begin(), end = opts_->end(); it != end; ++it) {
		const Potassco::ProgramOptions::Option& o = **it;
		POTASSCO_REQUIRE(exclude.count(o.name()) != 0 || o.assignDefault(), "Option '%s': invalid default value '%s'\n", o.name().c_str(), o.value()->defaultsTo());
	}
	return true;
}
void ClaspCliConfig::releaseOptions() {
	opts_ = 0;
}
static bool matchPath(const char*& path, const char* what) {
	std::size_t wLen = std::strlen(what);
	if (strncmp(path, what, wLen) != 0 || (path[wLen] && path[wLen++] != '.')) {
		return false;
	}
	path += wLen;
	return true;
}
ClaspCliConfig::KeyType ClaspCliConfig::getKey(KeyType k, const char* path) const {
	int16 id = decodeKey(k);
	if (!isValidId(id) || !path || !*path || (*path == '.' && !*++path)) {
		return k;
	}
	if (isLeafId(id)){ return KEY_INVALID; }
	NodeKey nk = getNode(id);
	for (int16 sk = nk.skBeg; sk < 0; ++sk) {
		if (matchPath(path, getNode(sk).name)) {
			KeyType ret = makeKeyHandle(sk, (sk == key_tester ? mode_tester : 0) | decodeMode(k), 0);
			if (!*path) { return ret; }
			return getKey(ret, path);
		}
	}
	uint8 mode = decodeMode(k);
	if (id == key_solver) {
		uint32 solverId;
		if (!isSolver(mode) && *path != '.' && Potassco::xconvert(path, solverId, &path, 0) == 1) {
			return getKey(makeKeyHandle(id, mode | mode_solver, std::min(solverId, (uint32)uint8(-1))), path);
		}
		mode |= mode_solver;
	}
	int16 opt = findOption(path, false);
	// remaining name must be a valid option in our subkey range
	if (opt < 0 || opt < nk.skBeg || opt >= static_cast<int16>(nk.skBeg + nk.skSize)) {
		return KEY_INVALID;
	}
	return makeKeyHandle(opt, mode, decodeSolver(k));
}

ClaspCliConfig::KeyType ClaspCliConfig::getArrKey(KeyType k, unsigned i) const {
	int16 id = decodeKey(k);
	if (id != key_solver || isSolver(decodeMode(k)) || i >= solve.supportedSolvers()) { return KEY_INVALID; }
	return makeKeyHandle(id, decodeMode(k) | mode_solver, i);
}
int ClaspCliConfig::getKeyInfo(KeyType k, int* nSubkeys, int* arrLen, const char** help, int* nValues) const {
	int16 id = decodeKey(k);
	int ret  = 0;
	if (!isValidId(id)){ return -1; }
	if (isLeafId(id)){
		if (nSubkeys && ++ret) { *nSubkeys = 0;  }
		if (arrLen && ++ret)   { *arrLen   = -1; }
		if (nValues && ++ret)  { *nValues  = static_cast<int>( !isTester(decodeMode(k)) || testerConfig() != 0 ); }
		if (help && ++ret)     { *help = getNode(id).desc; }
		return ret;
	}
	NodeKey x = getNode(id);
	if (nSubkeys && ++ret) { *nSubkeys = x.skSize; }
	if (nValues && ++ret)  { *nValues = -1; }
	if (help && ++ret)     { *help = x.desc; }
	if (arrLen && ++ret)   {
		*arrLen = -1;
		if (id == key_solver && !isSolver(decodeMode(k))) {
			const UserConfig* c = active(this, decodeMode(k));
			*arrLen = c ? (int)c->numSolver() : 0;
		}
	}
	return ret;
}
bool ClaspCliConfig::isLeafKey(KeyType k) { return isLeafId(decodeKey(k)); }
const char* ClaspCliConfig::getSubkey(KeyType k, uint32 i) const {
	int16 id = decodeKey(k);
	if (!isValidId(id) || isLeafId(id)) { return 0; }
	NodeKey nk = getNode(id);
	if (i >= nk.skSize) { return 0; }
	return getNode(static_cast<int16>(nk.skBeg + i)).name;
}
int ClaspCliConfig::getValue(KeyType key, std::string& out) const {
	try {
		const UserConfig* base = active(this, decodeMode(key));
		int16 o = decodeKey(key);
		int   r = isLeafId(o) && base ? 1 : -1;
		if (r > 0 && isOption(o)) {
			POTASSCO_ASSERT(base == this || isTesterOption(o));
			uint32 sId = decodeSolver(key);
			const SolverParams* solver = &base->solver(sId);
			const SolveParams*  search = &base->search(sId);
			// helper macros used in get
			using Potassco::toString; using Potassco::off; using Potassco::Set;
			#define GET_FUN(x)     Potassco::StringRef x(out); if (!x.out);else
			#define GET(...)       out = toString( __VA_ARGS__ )
			#define GET_IF(c, ...) out = ITE((c), toString(__VA_ARGS__), toString(off))
			switch(static_cast<OptionKey>(o)) {
				default: POTASSCO_ASSERT(false, "invalid option");
				#define OPTION(k, e, a, h, _, GET) case opt_##k: { GET ; } break;
				#define CLASP_ALL_GROUPS
				#include <clasp/cli/clasp_cli_options.inl>
			}
			#undef GET_FUN
			#undef GET
			#undef GET_IF
		}
		else if (r > 0 && o == meta_config) {
			if (base->cliConfig < config_max_value) { xconvert(out, static_cast<ConfigKey>(base->cliConfig)); }
			else                                    { out.append(config_[base == testerConfig()]); }
		}
		return r > 0 ? static_cast<int>(out.length()) : r;
	}
	catch (...) { return -2; }
}
int ClaspCliConfig::getValue(KeyType key, char* buffer, std::size_t bufSize) const {
	std::string temp;
	int ret = getValue(key, temp);
	if (ret <= 0) { return ret; }
	if (buffer && bufSize) {
		std::size_t n = temp.length() >= bufSize ? bufSize - 1 : temp.length();
		std::memcpy(buffer, temp.c_str(), n * sizeof(char));
		buffer[n] = 0;
	}
	return static_cast<int>(temp.length());
}
std::string ClaspCliConfig::getValue(const char* path) const {
	std::string temp;
	POTASSCO_REQUIRE(getValue(getKey(KEY_ROOT, path), temp) >= 0, "Invalid key: '%s'", path);
	return temp;
}
bool ClaspCliConfig::hasValue(const char* path) const {
	int nVals;
	return getKeyInfo(getKey(KEY_ROOT, path), 0, 0, 0, &nVals) == 1 && nVals > 0;
}

int ClaspCliConfig::setValue(KeyType key, const char* value) {
	int16 id = decodeKey(key);
	if (!isLeafId(id)) { return -1; }
	try {
		uint8 mode = decodeMode(key);
		validate_  = true;
		if (isTester(mode)) {
			addTesterConfig();
		}
		if (isOption(id)) {
			return setOption(id, mode, decodeSolver(key), value);
		}
		else {
			int sz = setAppOpt(id, mode, value);
			if (sz <= 0) { return 0; }
			std::string m;
			UserConfig* act = active(this, mode);
			ConfigIter  it  = getConfig(act->cliConfig, m);
			act->hasConfig  = 0;
			mode |= mode_relaxed;
			act->resize(1, 1);
			for (uint32 sId = 0; it.valid(); it.next()) {
				if (!setConfig(it, mode, sId, ParsedOpts(), 0)){ return 0; }
				if (++sId == static_cast<uint32>(sz))          { break; }
				mode |= mode_solver;
			}
			if (sz < 65 && static_cast<uint32>(sz) > act->numSolver()) {
				for (uint32 sId = act->numSolver(), mod = sId, end = static_cast<uint32>(sz); sId != end; ++sId) {
					SolverParams& solver = act->addSolver(sId);
					SolveParams&  search = act->addSearch(sId);
					(solver = act->solver(sId % mod)).setId(sId);
					search = act->search(sId % mod);
				}
			}
			act->hasConfig = 1;
			return 1;
		}
	}
	catch (...) {
		return -2;
	}
}

bool ClaspCliConfig::setValue(const char* path, const char* value) {
	int ret = setValue(getKey(KEY_ROOT, path), value);
	POTASSCO_REQUIRE(ret >= 0, (ret == -1 ? "Invalid or incomplete key: '%s'" : "Value error in key: '%s'"), path);
	return ret != 0;
}
int ClaspCliConfig::setOption(int option, uint8 mode_, uint32 sId, const char* _val_) {
	if (!_val_ || (isTester(mode_) && !isTesterOption(option)) || (isSolver(mode_) && !isSolverOption(option))) {
		return mode_ & mode_relaxed ? 1 : -1;
	}
	BasicSatConfig* base = active(this, mode_);
	SolverParams* solver = isSolverOption(option) ? &base->addSolver(sId) : 0;
	SolveParams*  search = isSolverOption(option) ? &base->addSearch(sId) : 0;
	// action & helper macros used in set
	using Potassco::stringTo; using Potassco::opt; using Potassco::Set;
	int ret = 1;
	try {
		#define FUN(x)           for (Potassco::ArgString x = _val_;;)
		#define STORE(obj)       { return stringTo((_val_), obj); }
		#define STORE_LEQ(x, y)  { unsigned _n; return stringTo(_val_, _n) && SET_LEQ(x, _n, y); }
		#define STORE_FLAG(x)    { bool _b; return stringTo(_val_, _b) && SET(x, static_cast<unsigned>(_b)); }
		#define STORE_OR_FILL(x) { unsigned _n; return stringTo(_val_, _n) && SET_OR_FILL(x, _n); }
		#define STORE_U(E, x)    { E _e; return stringTo((_val_), _e) && SET(x, static_cast<unsigned>(_e));}
		switch (static_cast<OptionKey>(option)) {
			default: POTASSCO_ASSERT(false, "invalid option");
			#define OPTION(k, e, a, d, SET, ...) case opt_##k: { SET ; } break;
			#define CLASP_ALL_GROUPS
			#include <clasp/cli/clasp_cli_options.inl>
		}
		#undef FUN
		#undef STORE
		#undef STORE_LEQ
		#undef STORE_FLAG
		#undef STORE_OR_FILL
		#undef STORE_U
	}
	catch (...) {
		ret = 0;
	}
	return ret;
}

int ClaspCliConfig::setAppOpt(int o, uint8 mode, const char* val) {
	if (o == meta_config) {
		std::pair<ConfigKey, uint32> defC(config_default, INT_MAX);
		if (Potassco::stringTo(val, defC)) { active(this, mode)->cliConfig = (uint8)defC.first; }
		else {
			POTASSCO_EXPECT(std::ifstream(val).is_open(), "Could not open config file '%s'", val);
			config_[isTester(mode)] = val;
			active(this, mode)->cliConfig = config_max_value + isTester(mode);
		}
		return Range<uint32>(0, INT_MAX).clamp(defC.second);
	}
	else if (o == meta_tester && !isTester(mode)) {
		addTesterConfig();
		ParsedOpts ex;
		bool ret = setConfig("<tester>", val, mode_tester | mode_meta, 0, ParsedOpts(), &ex);
		return ret && finalizeAppConfig(mode_tester, finalizeParsed(mode_tester, ex, ex), Problem_t::Asp, true);
	}
	return -1; // invalid option
}
bool ClaspCliConfig::setAppDefaults(ConfigKey config, uint8 mode, const ParsedOpts& seen, ProblemType t) {
	std::string mem;
	if (t != Problem_t::Asp && !seen.count(getOptionName(opt_sat_prepro, mem))) {
		POTASSCO_REQUIRE(setOption(opt_sat_prepro, mode, 0, "2,iter=20,occ=25,time=120"));
	}
	if (!isTester(mode) && config == config_many && t == Problem_t::Asp) {
		POTASSCO_REQUIRE(seen.count(getOptionName(opt_eq, mem)) || setOption(opt_eq, mode, 0, "3"));
		POTASSCO_REQUIRE(seen.count(getOptionName(opt_trans_ext, mem)) || setOption(opt_trans_ext, mode, 0, "dynamic"));
	}
	if (config != config_nolearn && active(this, mode)->solver(0).search == SolverParams::no_learning) {
		POTASSCO_REQUIRE(setConfig(getConfig(config_nolearn), mode | mode_relaxed, 0, seen, 0));
	}
	return true;
}

bool ClaspCliConfig::setConfig(const char* name, const char* args, uint8 mode, uint32 sId, const ParsedOpts& exclude, ParsedOpts* out) {
	createOptions();
	ParseContext ctx(*this, name, &exclude, mode, sId, out);
	Potassco::ProgramOptions::parseCommandString(args, ctx, Potassco::ProgramOptions::command_line_allow_flag_value);
	return true;
}
bool ClaspCliConfig::setConfig(const ConfigIter& config, uint8 mode, uint32 sId, const ParsedOpts& exclude, ParsedOpts* out) {
	if (*config.base()) {
		ConfigKey baseK = config_default;
		POTASSCO_REQUIRE(Potassco::stringTo(config.base(), baseK), "%s: '%s': Invalid base config!", config.name(), config.base());
		ConfigIter base = getConfig(baseK);
		if (!setConfig(base.name(), base.args(), mode | mode_solver, sId, exclude, out)) {
			return false;
		}
	}
	return setConfig(config.name(), config.args(), mode, sId, exclude, out);
}
bool ClaspCliConfig::validate() {
	UserConfiguration* arr[3] = { this, testerConfig(), 0 };
	UserConfiguration** c     = arr;
	const char* ctx = *c == this ? "config":"tester";
	const char* err = 0;
	do {
		for (uint32 i = 0; i != (*c)->numSolver(); ++i) {
			POTASSCO_REQUIRE((err = Clasp::Cli::validate((*c)->solver(i), (*c)->search(i))) == 0, "<%s>.%u: %s", ctx, i, err);
		}
	} while (*++c);
	validate_ = false;
	return true;
}

bool ClaspCliConfig::finalize(const ParsedOpts& x, ProblemType t, bool defs) {
	ParsedOpts temp;
	return finalizeAppConfig(0, finalizeParsed(0, x, temp), t, defs)
		&& finalizeAppConfig(mode_tester, ParsedOpts(), Problem_t::Asp, true);
}

void ClaspCliConfig::addDisabled(ParsedOpts& parsed) {
	finalizeParsed(0, parsed, parsed);
}

const std::string& ClaspCliConfig::getOptionName(int o, std::string& mem) const {
	POTASSCO_ASSERT(isOption(o));
	if (opts_.get()) {
		return (opts_->begin() + o)->get()->name();
	}
	keyToCliName(mem, getNode(static_cast<int16>(o)).name, "");
	return mem;
}

const ClaspCliConfig::ParsedOpts& ClaspCliConfig::finalizeParsed(uint8 mode, const ParsedOpts& parsed, ParsedOpts& exclude) const {
	const ParsedOpts* ret = &parsed;
	std::string mem;
	if (active(this, mode)->search(0).reduce.fReduce() == 0 && parsed.count(getOptionName(opt_deletion, mem)) != 0) {
		if (ret != &exclude) { exclude = parsed; }
		exclude.add(getOptionName(opt_del_cfl, mem));
		exclude.add(getOptionName(opt_del_max, mem));
		exclude.add(getOptionName(opt_del_grow, mem));
		ret = &exclude;
	}
	return *ret;
}

bool ClaspCliConfig::finalizeAppConfig(uint8 mode, const ParsedOpts& parsed, ProblemType t, bool defs) {
	UserConfig* config = active(this, mode);
	if (!config || config->hasConfig) { return true; }
	SolverParams defSolver = config->solver(0);
	SolveParams  defSearch = config->search(0);
	uint8 c = config->cliConfig;
	if (c == config_many && solve.numSolver() == 1) { c = config_default; }
	if (c == config_default) {
		if      (defSolver.search == SolverParams::no_learning)       { c = config_nolearn; }
		else if (isTester(mode))                                      { c = config_tester_default; }
		else if (solve.numSolver() == 1 || !solve.defaultPortfolio()) { c = t == Problem_t::Asp ? (uint8)config_asp_default : (uint8)config_sat_default; }
		else                                                          { c = config_many; }
	}
	if (defs && !setAppDefaults(static_cast<ConfigKey>(c), mode, parsed, t)) { return false; }
	std::string m;
	ConfigIter conf = getConfig(c, m);
	mode           |= mode_relaxed;
	const char* ctx = isTester(mode) ? "tester" : "config", *err = 0;
	for (uint32 i = 0; i != solve.numSolver() && conf.valid(); ++i) {
		SolverParams& solver = (config->addSolver(i) = defSolver).setId(i);
		SolveParams&  search = (config->addSearch(i) = defSearch);
		if (!setConfig(conf, mode, i, parsed, 0)) {
			return false;
		}
		POTASSCO_REQUIRE((err = Clasp::Cli::validate(solver, search)) == 0, "<%s>.%s : %s", ctx, conf.name(), err);
		conf.next();
		mode |= mode_solver;
	}
	config->hasConfig = 1;
	return true;
}

bool ClaspCliConfig::setAppConfig(const std::string& args, ProblemType t) {
	Potassco::ProgramOptions::ParsedOptions exclude;
	reset();
	return setConfig("setConfig", args.c_str(), mode_meta, 0, exclude, &exclude) && assignDefaults(exclude) && finalize(exclude, t, true);
}

const char* validate(const SolverParams& solver, const SolveParams& search) {
	const ReduceParams& reduce = search.reduce;
	if (solver.search == SolverParams::no_learning) {
		if (Heuristic_t::isLookback(solver.heuId)) return "Heuristic requires lookback strategy!";
		if (!search.restart.disabled()) return "'no-lookback': restart options disabled!";
		if (!reduce.cflSched.disabled() || (!reduce.growSched.disabled() && !reduce.growSched.defaulted()) || search.reduce.fReduce() != 0) return "'no-lookback': deletion options disabled!";
	}
	bool hasSched = !reduce.cflSched.disabled() || !reduce.growSched.disabled() || reduce.maxRange != UINT32_MAX;
	if (hasSched  && reduce.fReduce() == 0.0f && !reduce.growSched.defaulted()) return "'no-deletion': deletion strategies disabled!";
	if (!hasSched && reduce.fReduce() != 0.0f && !reduce.growSched.defaulted()) return "'deletion': deletion strategy required!";
	return 0;
}
}}
