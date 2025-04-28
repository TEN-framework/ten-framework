//
// Copyright (c) 2013-2017 Benjamin Kaufmann
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
/*!
 * \file
 * \brief Supermacros for describing clasp's options.
 * \code
 * OPTION(key, "ext", ARG(...), "help", set, get)
 * \endcode
 * An option consists of:
 *  - a key  (valid and unique c identifier in 'snake_case')
 *  - a key extension ("[!][,<alias>][,@<level>]") as understood by ProgramOptions::OptionInitHelper
 *  - an arg description (ARG macro) as understood by ProgramOptions::Value
 *  - a description (string)
 *  - a set action to be executed when a value (string) for the option is found in a source
 *  - a get action to be executed when the current value for an option is requested
 *  .
 *
 * \note In the implementation of ClaspCliConfig, each key is mapped to an enumeration constant and
 * the stringified version of key (i.e. \#key) is used to identify options.
 * Furthermore, the key is also used for generating command-line option names.
 * As a convention, compound keys using 'snake_case' to separate words
 * are mapped to dash-separated command-line option names.
 * E.g. an \<option_like_this\> is mapped to the command-line option "option-like-this".
 *
 * \note ClaspCliConfig assumes a certain option order. In particular, context options shall
 * precede all solver/search options, which in turn shall precede global asp/solving options.
 *
 * \note The following set actions may be used:
 *  - STORE(obj): converts the string value to the type of obj and stores the result in obj.
 *  - STORE_U(E, n): converts the string value to type E and stores it as unsigned int in n.
 *  - STORE_LEQ(n, max): converts the string value to an unsinged int and stores the result in n if it is <= max.
 *  - STORE_FLAG(n): converts the string value to a bool and stores the result in n as either 0 or 1.
 *  - STORE_OR_FILL(n): converts the string value to an unsinged int t and sets n to std::min(t, maxValue(n)).
 *  - FUN(arg): anonymous function of type bool (ArgStream& arg), where arg provides the following interface:
 *    - arg.ok()     : returns whether arg is still valid
 *    - arg.empty()  : returns whether arg is empty (i.e. all tokens were extracted)
 *    - arg.off()    : returns whether arg contains a single token representing a valid off value
 *    - arg >> x     : extracts and converts the current token and stores it in x. Sets failbit if conversion fails.
 *    - arg >> opt(x): extracts an optional argument, shorthand for (!arg.empty() ? arg>>obj : arg)
 *    .
 *
 * \note The following get actions may be used:
 *  - GET_FUN(str)  : anonymous function of type void (OutputStream& str)
 *  - GET(obj...)   : shorthand for GET_FUN(str) { (str << obj)...; }
 *  - GET_IF(C, obj): shorthand for GET_FUN(str) { ITE(C, str << obj, str << off); }
 *  .
 *
 * \note The following primitives may be used in the set/get arguments:
 *  - off                  : singleton object representing a valid off value ("no", "off", "false", "0")
 *  - ITE(C, x, y)         : if-then-else expression, i.e. behaves like (C ? x : y).
 *  - SET(x, y)            : shorthand for (x=y) == y.
 *  - SET_LEQ(x, v, m)     : shorthand for (x <= m && SET(x, v)).
 *  - SET_GEQ(x, v, m)     : shorthand for (x >= m && SET(x, v)).
 *  - SET_OR_FILL(x, v)    : behaves like SET(x, min(v, maxValue(x)))
 *  - SET_OR_ZERO(x,v)     : behaves like ITE(v <= maxValue(x), SET(x, v), SET(x, 0)).
 *  .
 */
#if !defined(OPTION) || defined(SELF) || !defined(CLASP_HAS_THREADS)
#error Invalid include context
#endif

#if !defined(GROUP_BEGIN)
#define GROUP_BEGIN(X)
#endif

#if !defined(GROUP_END)
#define GROUP_END(X)
#endif

//! Options for configuring a SharedContext object stored in a Clasp::ContextParams object.
#if defined(CLASP_CONTEXT_OPTIONS)
#define SELF CLASP_CONTEXT_OPTIONS
GROUP_BEGIN(SELF)
OPTION(share, "!,@1", ARG_EXT(defaultsTo("auto")->state(Value::value_defaulted), DEFINE_ENUM_MAPPING(ContextParams::ShareMode, \
       MAP("no"  , ContextParams::share_no)  , MAP("all", ContextParams::share_all),\
       MAP("auto", ContextParams::share_auto), MAP("problem", ContextParams::share_problem),\
       MAP("learnt", ContextParams::share_learnt))),\
       "Configure physical sharing of constraints [%D]\n"\
       "      %A: {auto|problem|learnt|all}", FUN(arg) {ContextParams::ShareMode x; return arg>>x && SET(SELF.shareMode, (uint32)x);}, GET((ContextParams::ShareMode)SELF.shareMode))
OPTION(learn_explicit, ",@2" , ARG(flag()), "Do not use Short Implication Graph for learning", STORE_FLAG(SELF.shortMode), GET(SELF.shortMode))
OPTION(sat_prepro    , "!,@1", ARG(arg("<arg>")->implicit("2")),                     \
       "Run SatELite-like preprocessing (Implicit: %I)\n"                            \
       "      %A: <level>[,<limit>...]\n"                                            \
       "        <level> : Set preprocessing level to <level  {1..3}>\n"              \
       "          1: Variable elimination with subsumption (VE)\n"                   \
       "          2: VE with limited blocked clause elimination (BCE)\n"             \
       "          3: Full BCE followed by VE\n"                                      \
       "        <limit> : [<key {iter|occ|time|frozen|clause}>=]<n> (0=no limit)\n"  \
       "          iter  : Set iteration limit to <n>           [0]\n"                \
       "          occ   : Set variable occurrence limit to <n> [0]\n"                \
       "          time  : Set time limit to <n> seconds        [0]\n"                \
       "          frozen: Set frozen variables limit to <n>%%   [0]\n"               \
       "          size  : Set size limit to <n>*1000 clauses   [4000]", STORE(SELF.satPre), GET(SELF.satPre))
GROUP_END(SELF)
#undef CLASP_CONTEXT_OPTIONS
#undef SELF
#endif

//! Global options only valid in facade.
#if defined(CLASP_GLOBAL_OPTIONS)
#define SELF CLASP_GLOBAL_OPTIONS
GROUP_BEGIN(SELF)
OPTION(stats, ",s", ARG(implicit("1")->arg("<n>[,<t>]")), "Enable {1=basic|2=full} statistics (<t> for tester)",\
    FUN(arg) { uint32 s = 0; uint32 t = 0;\
      return (arg.off() || (arg >> s >> opt(t) && s > 0))
        && SET(SELF.stats, s) && ((!SELF.testerConfig() && t == 0) || SET(SELF.addTesterConfig()->stats, t));
    },\
    GET_FUN(str) { ITE(!SELF.testerConfig() || !SELF.testerConfig()->stats, str << SELF.stats, str << SELF.stats << SELF.testerConfig()->stats); })
OPTION(parse_ext, "!", ARG(flag()), "Enable extensions in non-aspif input",\
    FUN(arg) { bool b = false; return (arg.off() || arg >> b) && (SELF.parse.assign(ParserOptions::parse_full, b), true); }, \
    GET((SELF.parse.anyOf(ParserOptions::parse_full))))
OPTION(parse_maxsat, "!", ARG(flag()), "Treat dimacs input as MaxSAT problem", \
    FUN(arg) { bool b = false; return (arg.off() || arg >> b) && (SELF.parse.assign(ParserOptions::parse_maxsat, b), true); }, \
    GET(static_cast<bool>(SELF.parse.isEnabled(ParserOptions::parse_maxsat))))
GROUP_END(SELF)
#undef CLASP_GLOBAL_OPTIONS
#undef SELF
#endif

//! Solver options (see SolverParams).
#if defined(CLASP_SOLVER_OPTIONS)
#define SELF CLASP_SOLVER_OPTIONS
GROUP_BEGIN(SELF)
OPTION(opt_strategy , ""  , ARG_EXT(arg("<arg>"),\
       DEFINE_ENUM_MAPPING(OptParams::Type, MAP("bb", OptParams::type_bb), MAP("usc", OptParams::type_usc))\
       DEFINE_ENUM_MAPPING(OptParams::BBAlgo, MAP("lin", OptParams::bb_lin), MAP("hier", OptParams::bb_hier), MAP("inc", OptParams::bb_inc), MAP("dec", OptParams::bb_dec))\
       DEFINE_ENUM_MAPPING(OptParams::UscAlgo, MAP("oll", OptParams::usc_oll), MAP("one", OptParams::usc_one), MAP("k", OptParams::usc_k), MAP("pmres", OptParams::usc_pmr))\
       DEFINE_ENUM_MAPPING(OptParams::UscOption, MAP("disjoint", OptParams::usc_disjoint), MAP("succinct", OptParams::usc_succinct), MAP("stratify", OptParams::usc_stratify))),\
       "Configure optimization strategy\n" \
       "      %A: {bb|usc}[,<tactics>]\n" \
       "        bb : Model-guided optimization with <tactics {lin|hier|inc|dec}> [lin]\n" \
       "          lin : Basic lexicographical descent\n"                                  \
       "          hier: Hierarchical (highest priority criteria first) descent \n"        \
       "          inc : Hierarchical descent with exponentially increasing steps\n"       \
       "          dec : Hierarchical descent with exponentially decreasing steps\n"       \
       "        usc: Core-guided optimization with <tactics>: <relax>[,<opts>]\n"         \
       "          <relax>: Relaxation algorithm {oll|one|k|pmres}                [oll]\n" \
       "            oll    : Use strategy from unclasp\n"                                 \
       "            one    : Add one cardinality constraint per core\n"                   \
       "            k[,<n>]: Add cardinality constraints of bounded size ([0]=dynamic)\n" \
       "            pmres  : Add clauses of size 3\n"                                     \
       "          <opts> : Tactics <list {disjoint|succinct|stratify}>|<mask {0..7}>\n"   \
       "            disjoint: Disjoint-core preprocessing                    (1)\n"       \
       "            succinct: No redundant (symmetry) constraints            (2)\n"       \
       "            stratify: Stratification heuristic for handling weights  (4)",        \
       STORE(SELF.opt), GET(SELF.opt))
OPTION(opt_usc_shrink, "", ARG_EXT(arg("<arg>"), DEFINE_ENUM_MAPPING(OptParams::UscTrim, \
       MAP("lin", OptParams::usc_trim_lin), MAP("rgs", OptParams::usc_trim_rgs), MAP("min", OptParams::usc_trim_min),\
       MAP("exp", OptParams::usc_trim_exp), MAP("inv", OptParams::usc_trim_inv), MAP("bin", OptParams::usc_trim_bin))),\
       "Enable core-shrinking in core-guided optimization\n"        \
       "      %A: <algo>[,<limit> (0=no limit)]\n"                  \
       "        <algo> : Use algorithm {lin|inv|bin|rgs|exp|min}\n" \
       "          lin  : Forward linear search unsat\n"             \
       "          inv  : Inverse linear search not unsat\n"         \
       "          bin  : Binary search\n"                           \
       "          rgs  : Repeated geometric sequence until unsat\n" \
       "          exp  : Exponential search until unsat\n"          \
       "          min  : Linear search for subset minimal core\n"   \
       "        <limit>: Limit solve calls to 2^<n> conflicts [10]",\
      FUN(arg) {\
        OptParams::UscTrim t = (OptParams::UscTrim)0; uint32 n = 0; \
        return (arg.off() || arg >> t >> opt(n=10)) && SET(SELF.opt.trim, uint32(t)) && SET(SELF.opt.tLim, uint32(n)); },\
      GET_IF(SELF.opt.trim, (OptParams::UscTrim)SELF.opt.trim, SELF.opt.tLim))
OPTION(opt_heuristic, "", ARG_EXT(arg("<list>"), DEFINE_ENUM_MAPPING(OptParams::Heuristic, \
       MAP("sign", OptParams::heu_sign), MAP("model", OptParams::heu_model))),\
       "Enable optimization heuristic\n"\
       "      %A: {sign|model}\n"\
       "        sign : Prefer signs minimizing objective\n"\
       "        model: Assume literals minimizing objective after each model",\
       FUN(arg) { Set<OptParams::Heuristic> h; return (arg.off() || arg >> h) && SET(SELF.opt.heus, h.value());},\
       GET(Set<OptParams::Heuristic>(SELF.opt.heus)))
OPTION(restart_on_model, "!", ARG(flag()), "Restart after each model\n", STORE_FLAG(SELF.restartOnModel), GET(SELF.restartOnModel))
OPTION(lookahead    , "!", ARG_EXT(implicit("atom"), DEFINE_ENUM_MAPPING(VarType, \
       MAP("atom", Var_t::Atom), MAP("body", Var_t::Body), MAP("hybrid", Var_t::Hybrid))),\
       "Configure failed-literal detection (fld)\n" \
       "      %A: <type>[,<limit>] / Implicit: %I\n" \
       "        <type> : Run fld via {atom|body|hybrid} lookahead\n" \
       "        <limit>: Disable fld after <limit> applications ([0]=no limit)\n" \
       "      --lookahead=atom is default if --no-lookback is used\n", FUN(arg) { \
       VarType type = Var_t::Atom; uint32 limit = (SELF.lookOps = 0u);\
       return ITE(arg.off(), SET(SELF.lookType, 0u), arg>>type>>opt(limit) && SET(SELF.lookType, (uint32)type)) && SET_OR_ZERO(SELF.lookOps, limit);},\
       GET_IF(SELF.lookType, (VarType)SELF.lookType, SELF.lookOps))
OPTION(heuristic, "", ARG_EXT(arg("<heu>"), DEFINE_ENUM_MAPPING(Heuristic_t::Type, \
       MAP("berkmin", Heuristic_t::Berkmin), MAP("vmtf"  , Heuristic_t::Vmtf), \
       MAP("vsids"  , Heuristic_t::Vsids)  , MAP("domain", Heuristic_t::Domain), \
       MAP("unit"   , Heuristic_t::Unit)   , MAP("auto", Heuristic_t::Default), MAP("none"  , Heuristic_t::None))), \
       "Configure decision heuristic\n"  \
       "      %A: {Berkmin|Vmtf|Vsids|Domain|Unit|None}[,<n>]\n" \
       "        Berkmin: Use BerkMin-like heuristic (Check last <n> nogoods [0]=all)\n" \
       "        Vmtf   : Use Siege-like heuristic (Move <n> literals to the front [8])\n" \
       "        Vsids  : Use Chaff-like heuristic (Use 1.0/0.<n> as decay factor  [95])\n"\
       "        Domain : Use domain knowledge in Vsids-like heuristic\n"\
       "        Unit   : Use Smodels-like heuristic (Default if --no-lookback)\n" \
       "        None   : Select the first free variable", FUN(arg) { Heuristic_t::Type h = Heuristic_t::Berkmin; uint32 n = 0u; \
       return arg>>h>>opt(n) && SET(SELF.heuId, (uint32)h) && (Heuristic_t::isLookback(h) || !n) && SET_OR_FILL(SELF.heuristic.param, n);},\
       GET((Heuristic_t::Type)SELF.heuId, SELF.heuristic.param))
OPTION(init_moms  , "!,@2", ARG(flag())    , "Initialize heuristic with MOMS-score", STORE_FLAG(SELF.heuristic.moms), GET(SELF.heuristic.moms))
OPTION(score_res  , ",@2" , ARG_EXT(arg("<score>"), DEFINE_ENUM_MAPPING(HeuParams::Score, \
       MAP("auto", HeuParams::score_auto), MAP("min", HeuParams::score_min), MAP("set", HeuParams::score_set), MAP("multiset", HeuParams::score_multi_set))),\
       "Resolution score {auto|min|set|multiset}", STORE_U(HeuParams::Score, SELF.heuristic.score), GET(static_cast<HeuParams::Score>(SELF.heuristic.score)))
OPTION(score_other, ",@2" , ARG_EXT(arg("<arg>"), DEFINE_ENUM_MAPPING(HeuParams::ScoreOther, \
       MAP("auto", HeuParams::other_auto), MAP("no", HeuParams::other_no), MAP("loop", HeuParams::other_loop), MAP("all", HeuParams::other_all))),\
       "Score other learnt nogoods: {auto|no|loop|all}", STORE_U(HeuParams::ScoreOther, SELF.heuristic.other), GET(static_cast<HeuParams::ScoreOther>(SELF.heuristic.other)))
OPTION(sign_def   , ",@1" , ARG_EXT(arg("<sign>"),\
       DEFINE_ENUM_MAPPING(SolverStrategies::SignHeu, MAP("asp", SolverStrategies::sign_atom), MAP("pos", SolverStrategies::sign_pos), MAP("neg", SolverStrategies::sign_neg), MAP("rnd", SolverStrategies::sign_rnd))),\
       "Default sign: {asp|pos|neg|rnd}", STORE_U(SolverStrategies::SignHeu, SELF.signDef), GET((SolverStrategies::SignHeu)SELF.signDef))
OPTION(sign_fix   , "!,@2", ARG(flag())    , "Disable sign heuristics and use default signs only", STORE_FLAG(SELF.signFix), GET(SELF.signFix))
OPTION(berk_huang , "!,@2", ARG(flag())    , "Enable Huang-scoring in Berkmin", STORE_FLAG(SELF.heuristic.huang), GET(SELF.heuristic.huang))
OPTION(vsids_acids, "!,@2", ARG(flag())    , "Enable acids-scheme in Vsids/Domain", STORE_FLAG(SELF.heuristic.acids), GET(SELF.heuristic.acids))
OPTION(vsids_progress, ",@2", NO_ARG, "Enable dynamic decaying scheme in Vsids/Domain\n"\
       "      %A: <n>[,<i {1..100}>][,<c>]|(0=disable)\n"\
       "        <n> : Set initial decay factor to 1.0/0.<n>\n"\
       "        <i> : Set decay update to <i>/100.0      [1]\n"\
       "        <c> : Decrease decay every <c> conflicts [5000]", \
       FUN(arg) { uint32 n = 80; uint32 i = 1; uint32 c = 5000; \
       return ITE(arg.off(), SET(SELF.heuristic.decay.init, 0), (arg >> n >> opt(i) >> opt(c))\
         && SET(SELF.heuristic.decay.init, n) && SET_LEQ(SELF.heuristic.decay.bump, i, 100) && SET(SELF.heuristic.decay.freq, c));}, \
       GET_IF(SELF.heuristic.decay.init, SELF.heuristic.decay.init, SELF.heuristic.decay.bump, SELF.heuristic.decay.freq))
OPTION(nant       , "!,@2", ARG(flag())    , "Prefer negative antecedents of P in heuristic", STORE_FLAG(SELF.heuristic.nant), GET(SELF.heuristic.nant))
OPTION(dom_mod    , ",@1" , ARG_EXT(arg("<arg>"), DEFINE_ENUM_MAPPING(HeuParams::DomMod, \
       MAP("level", HeuParams::mod_level), MAP("pos", HeuParams::mod_spos), MAP("true", HeuParams::mod_true),\
       MAP("neg", HeuParams::mod_sneg), MAP("false", HeuParams::mod_false), MAP("init", HeuParams::mod_init), MAP("factor", HeuParams::mod_factor))\
       DEFINE_ENUM_MAPPING(HeuParams::DomPref, MAP("all", HeuParams::pref_atom), MAP("scc", HeuParams::pref_scc), MAP("hcc", HeuParams::pref_hcc),\
       MAP("disj", HeuParams::pref_disj), MAP("opt", HeuParams::pref_min), MAP("show", HeuParams::pref_show))),\
       "Default modification for domain heuristic\n"\
       "      %A: (no|<mod>[,<pick>])\n"\
       "        <mod>  : Modifier {level|pos|true|neg|false|init|factor}\n"\
       "        <pick> : Apply <mod> to (all | <list {scc|hcc|disj|opt|show}>) atoms", \
       FUN(arg) { HeuParams::DomMod modK; unsigned modN = 0; Set<HeuParams::DomPref> k; bool ok = true;\
         if (!arg.off()) { ok = ITE(arg.peek() >= 'A', arg >> modK && SET(modN, uint32(modK)), arg >> modN && modN > 0u && modN < 8u); }\
         return ok && (arg.off() || arg >> opt(k)) && SET(SELF.heuristic.domMod, modN) && SET(SELF.heuristic.domPref, k.value());},\
       GET_FUN(str) { Set<HeuParams::DomMod> mod(SELF.heuristic.domMod); Set<HeuParams::DomPref> pick(SELF.heuristic.domPref); \
         ITE(mod.value() && pick.value(), str << mod << pick, str << mod); })
OPTION(save_progress, "", ARG(implicit("1")->arg("<n>")), "Use RSat-like progress saving on backjumps > %A", STORE_OR_FILL(SELF.saveProgress), GET(SELF.saveProgress))
OPTION(init_watches , ",@2", ARG_EXT(arg("<arg>"), DEFINE_ENUM_MAPPING(SolverStrategies::WatchInit, \
       MAP("rnd", SolverStrategies::watch_rand), MAP("first", SolverStrategies::watch_first), MAP("least", SolverStrategies::watch_least))),\
       "Watched literal initialization: {rnd|first|least}", STORE_U(SolverStrategies::WatchInit, SELF.initWatches), GET(static_cast<SolverStrategies::WatchInit>(SELF.initWatches)))
OPTION(update_mode   , ",@2", ARG_EXT(arg("<mode>"), DEFINE_ENUM_MAPPING(SolverStrategies::UpdateMode, \
       MAP("propagate", SolverStrategies::update_on_propagate), MAP("conflict", SolverStrategies::update_on_conflict))),\
       "Process messages on {propagate|conflict}", STORE_U(SolverStrategies::UpdateMode, SELF.upMode), GET(static_cast<SolverStrategies::UpdateMode>(SELF.upMode)))
OPTION(acyc_prop, ",@2", ARG(implicit("1")->arg("{0..1}")), "Use backward inference in acyc propagation", \
       FUN(arg) { uint32 x; return arg>>x && SET_LEQ(SELF.acycFwd, (1u-x), 1u); }, GET(1u-SELF.acycFwd))
OPTION(seed          , ""   , ARG(arg("<n>")),"Set random number generator's seed to %A", STORE(SELF.seed), GET(SELF.seed))
OPTION(no_lookback   , ""   , ARG(flag()), "Disable all lookback strategies\n", STORE_FLAG(SELF.search),GET(static_cast<bool>(SELF.search == SolverStrategies::no_learning)))
OPTION(forget_on_step, ""   , ARG_EXT(arg("<opts>"), DEFINE_ENUM_MAPPING(SolverParams::Forget, \
       MAP("varScores", SolverParams::forget_heuristic), MAP("signs", SolverParams::forget_signs), MAP("lemmaScores", SolverParams::forget_activities), MAP("lemmas", SolverParams::forget_learnts))),\
       "Configure forgetting on (incremental) step\n"\
       "      %A: <list {varScores|signs|lemmaScores|lemmas}>|<mask {0..15}>\n",\
       FUN(arg) { Set<SolverParams::Forget> s; return (arg.off() || arg >> s) && SET(SELF.forgetSet, s.value()); },\
       GET(Set<SolverParams::Forget>(SELF.forgetSet)))
OPTION(strengthen    , "!"  , ARG_EXT(arg("<X>"),\
       DEFINE_ENUM_MAPPING(SolverStrategies::CCMinType, MAP("local", SolverStrategies::cc_local), MAP("recursive", SolverStrategies::cc_recursive))\
       DEFINE_ENUM_MAPPING(SolverStrategies::CCMinAntes, MAP("all", SolverStrategies::all_antes), MAP("short", SolverStrategies::short_antes), MAP("binary", SolverStrategies::binary_antes))), \
       "Use MiniSAT-like conflict nogood strengthening\n"                      \
       "      %A: <mode>[,<type>][,<bump {yes|no}>]\n"                         \
       "        <mode>: Use {local|recursive} self-subsumption check\n"        \
       "        <type>: Follow {all|short|binary} antecedents [all]\n"         \
       "        <bump>: Bump activities of antecedents        [yes]", FUN(arg) {\
       SolverStrategies::CCMinType m = SolverStrategies::cc_local; SolverStrategies::CCMinAntes t = SolverStrategies::no_antes; bool b = true; \
       return (arg.off() || arg>>m>>opt(t = SolverStrategies::all_antes)>>opt(b)) && SET(SELF.ccMinAntes, (uint32)t) && SET(SELF.ccMinRec, (uint32)m) && SET(SELF.ccMinKeepAct, uint32(!b)); }, \
       GET_IF(SELF.ccMinAntes != SolverStrategies::no_antes, (SolverStrategies::CCMinType)SELF.ccMinRec, (SolverStrategies::CCMinAntes)SELF.ccMinAntes, ITE(SELF.ccMinKeepAct, "no", "yes")))
OPTION(otfs        , ""   , ARG(implicit("1")->arg("{0..2}")), "Enable {1=partial|2=full} on-the-fly subsumption", STORE_LEQ(SELF.otfs, 2u), GET(SELF.otfs))
OPTION(update_lbd  , "!,@2" , ARG_EXT(arg("<arg>"), DEFINE_ENUM_MAPPING(SolverStrategies::LbdMode, \
       MAP("less", SolverStrategies::lbd_updated_less), MAP("glucose", SolverStrategies::lbd_update_glucose), MAP("pseudo", SolverStrategies::lbd_update_pseudo))),\
       "Configure LBD updates during conflict resolution\n"                 \
       "      %A: <mode {less|glucose|pseudo}>[,<n {0..127}>]\n"            \
       "        less   : update to X = new LBD   iff X   < previous LBD\n"  \
       "        glucose: update to X = new LBD   iff X+1 < previous LBD\n"  \
       "        pseudo : update to X = new LBD+1 iff X   < previous LBD\n"  \
       "           <n> : Protect updated nogoods on next reduce if X <= <n>",\
       FUN(arg) { SolverStrategies::LbdMode n = SolverStrategies::lbd_fixed; uint32 m = 0; \
         return (arg.off() || (arg >> n >> opt(m) && n > 0)) && SET(SELF.updateLbd, uint32(n)) && SET_LEQ(search->reduce.strategy.protect, m, uint32(Clasp::LBD_MAX));},\
       GET_IF(SELF.updateLbd, (SolverStrategies::LbdMode)SELF.updateLbd, search->reduce.strategy.protect))
OPTION(update_act  , ",@2", ARG(flag()), "Enable LBD-based activity bumping", STORE_FLAG(SELF.bumpVarAct), GET(SELF.bumpVarAct))
OPTION(reverse_arcs, ""   , ARG(implicit("1")->arg("{0..3}")), "Enable ManySAT-like inverse-arc learning", STORE_LEQ(SELF.reverseArcs, 3u), GET(SELF.reverseArcs))
OPTION(contraction , "!,@2", ARG_EXT(arg("<arg>"),\
       DEFINE_ENUM_MAPPING(SolverStrategies::CCRepMode, MAP("no", SolverStrategies::cc_no_replace), MAP("decisionSeq", SolverStrategies::cc_rep_decision), MAP("allUIP", SolverStrategies::cc_rep_uip), MAP("dynamic", SolverStrategies::cc_rep_dynamic))),\
       "Configure handling of long learnt nogoods\n"
       "      %A: <n>[,<rep>]\n"\
       "        <n>  : Contract nogoods if size > <n> (0=disable)\n"\
       "        <rep>: Nogood replacement {no|decisionSeq|allUIP|dynamic} [no]\n", FUN(arg) { uint32 n = 0; SolverStrategies::CCRepMode r = SolverStrategies::cc_no_replace;\
       return (arg.off() || (arg>>n>>opt(r) && n != 0u)) && SET_OR_FILL(SELF.compress, n) && SET(SELF.ccRepMode, uint32(r));},\
       GET_IF(SELF.compress, SELF.compress, (SolverStrategies::CCRepMode)SELF.ccRepMode))
OPTION(loops, "" , ARG_EXT(arg("<type>"), DEFINE_ENUM_MAPPING(DefaultUnfoundedCheck::ReasonStrategy,\
       MAP("common"  , DefaultUnfoundedCheck::common_reason)  , MAP("shared", DefaultUnfoundedCheck::shared_reason), \
       MAP("distinct", DefaultUnfoundedCheck::distinct_reason), MAP("no", DefaultUnfoundedCheck::only_reason))),\
       "Configure learning of loop nogoods\n" \
       "      %A: {common|distinct|shared|no}\n" \
       "        common  : Create loop nogoods for atoms in an unfounded set\n" \
       "        distinct: Create distinct loop nogood for each atom in an unfounded set\n" \
       "        shared  : Create loop formula for a whole unfounded set\n" \
       "        no      : Do not learn loop formulas\n", FUN(arg) {DefaultUnfoundedCheck::ReasonStrategy x; return arg>>x && SET(SELF.loopRep, (uint32)x);},\
       GET(static_cast<DefaultUnfoundedCheck::ReasonStrategy>(SELF.loopRep)))
GROUP_END(SELF)
#undef CLASP_SOLVER_OPTIONS
#undef SELF
#endif

//! Search-related options (see SolveParams).
#if defined(CLASP_SEARCH_OPTIONS)
#define SELF CLASP_SEARCH_OPTIONS
GROUP_BEGIN(SELF)
OPTION(partial_check, "", ARG(implicit("50")), "Configure partial stability tests\n" \
       "      %A: <p>[,<h>] / Implicit: %I\n" \
       "        <p>: Partial check skip percentage\n"    \
       "        <h>: Init/update value for high bound ([0]=umax)", FUN(arg) {\
       uint32 p = 0; uint32 h = 0; \
       return (arg.off() || (arg>>p>>opt(h) && p)) && SET_LEQ(SELF.fwdCheck.highPct, p, 100u) && SET_OR_ZERO(SELF.fwdCheck.highStep, h);},\
       GET_IF(SELF.fwdCheck.highPct, SELF.fwdCheck.highPct, SELF.fwdCheck.highStep))
OPTION(sign_def_disj, ",@2", ARG(arg("<sign>")), "Default sign for atoms in disjunctions", STORE_U(SolverStrategies::SignHeu, SELF.fwdCheck.signDef), GET((SolverStrategies::SignHeu)SELF.fwdCheck.signDef))
OPTION(rand_freq, "!", ARG(arg("<p>")), "Make random decisions with probability %A", FUN(arg) {\
       double f = 0.0; \
       return (arg.off() || arg>>f) && SET_R(SELF.randProb, (float)f, 0.0f, 1.0f);}, GET(SELF.randProb))
OPTION(rand_prob, "", ARG(arg("<n>[,<m>]")), "Do <n> random searches with [<m>=100] conflicts",\
       FUN(arg) { uint32 n1 = 0; uint32 n2 = 100;\
       return (arg.off() || (arg>>n1>>opt(n2) && n1)) && SET_OR_FILL(SELF.randRuns, n1) && SET_OR_FILL(SELF.randConf, n2);},\
       GET_IF(SELF.randRuns, SELF.randRuns,SELF.randConf))
#undef SELF
//! Options for configuring the restart strategy of a solver.
#define SELF CLASP_SEARCH_RESTART_OPTIONS
#if defined(NOTIFY_SUBGROUPS)
GROUP_BEGIN(SELF)
#endif
OPTION(restarts, "!,r", ARG(arg("<sched>")), "Configure restart policy\n" \
       "      %A: <type {D|F|L|x|+}>,<n {1..umax}>[,<args>][,<lim>]\n"                    \
       "        F,<n>    : Run fixed sequence of <n> conflicts\n"                         \
       "        L,<n>    : Run Luby et al.'s sequence with unit length <n>\n"             \
       "        x,<n>,<f>: Run geometric seq. of <n>*(<f>^i) conflicts  (<f> >= 1.0)\n"   \
       "        +,<n>,<m>: Run arithmetic seq. of <n>+(<m>*i) conflicts (<m {0..umax}>)\n"\
       "        ...,<lim>: Repeat seq. every <lim>+j restarts           (<type> != F)\n"  \
       "        D,<n>,<f>: Restart based on moving LBD average over last <n> conflicts\n" \
       "                   Mavg(<n>,LBD)*<f> > avg(LBD)\n"                                \
       "                   use conflict level average if <lim> > 0 and avg(LBD) > <lim>\n"\
       "      no|0       : Disable restarts", FUN(arg) { return ITE(arg.off(), (SELF.disable(),true), \
       arg>>SELF.sched && SET(SELF.dynRestart, uint32(SELF.sched.type == ScheduleStrategy::User)));}, GET(SELF.sched))
OPTION(reset_restarts  , ",@2", ARG_EXT(arg("<arg>"), DEFINE_ENUM_MAPPING(RestartParams::SeqUpdate, \
       MAP("no",RestartParams::seq_continue), MAP("repeat", RestartParams::seq_repeat), MAP("disable", RestartParams::seq_disable))),\
       "Update restart seq. on model {no|repeat|disable}",\
       STORE_U(RestartParams::SeqUpdate, SELF.upRestart), GET(static_cast<RestartParams::SeqUpdate>(SELF.upRestart)))
OPTION(local_restarts  , "!"  , ARG(flag()), "Use Ryvchin et al.'s local restarts", STORE_FLAG(SELF.cntLocal), GET(SELF.cntLocal))
OPTION(counter_restarts, ""   , ARG(arg("<arg>")), "Use counter implication restarts\n" \
       "      %A: (<rate>[,<bump>] | {0|no})\n" \
       "      <rate>: Interval in number of restarts\n" \
       "      <bump>: Bump factor applied to indegrees", \
       FUN(arg) { uint32 n = 0; uint32 m = SELF.counterBump; \
       return (arg.off() || (arg >> n >> opt(m) && n > 0)) && SET_OR_FILL(SELF.counterRestart, n) && SET_OR_FILL(SELF.counterBump, m); },\
       GET_IF(SELF.counterRestart, SELF.counterRestart, SELF.counterBump))
OPTION(block_restarts  , ""   , ARG(arg("<arg>")), "Use glucose-style blocking restarts\n" \
       "      %A: <n>[,<R {1.0..5.0}>][,<c>]\n" \
       "        <n>: Window size for moving average (0=disable blocking)\n" \
       "        <R>: Block restart if assignment > average * <R>  [1.4]\n"             \
       "        <c>: Disable blocking for the first <c> conflicts [10000]\n", FUN(arg) { \
       uint32 n = 0; double R = 0.0; uint32 x = 0;\
       return (arg.off() || (arg>>n>>opt(R = 1.4)>>opt(x = 10000) && n && R >= 1.0 && R <= 5.0))\
         && SET(SELF.blockWindow, n) && SET(SELF.blockScale, (float)R) && SET_OR_FILL(SELF.blockFirst, x);},\
       GET_IF(SELF.blockWindow, SELF.blockWindow, SELF.blockScale, SELF.blockFirst))
OPTION(shuffle         , "!"  , ARG(arg("<n1>,<n2>")), "Shuffle problem after <n1>+(<n2>*i) restarts\n", FUN(arg) { uint32 n1 = 0; uint32 n2 = 0;\
       return (arg.off() || (arg>>n1>>opt(n2) && n1)) && SET_OR_FILL(SELF.shuffle, n1) && SET_OR_FILL(SELF.shuffleNext, n2);},\
       GET_IF(SELF.shuffle, SELF.shuffle, SELF.shuffleNext))
#if defined(NOTIFY_SUBGROUPS)
GROUP_END(SELF)
#endif
#undef SELF
#undef CLASP_SEARCH_RESTART_OPTIONS
//! Options for configuring the deletion strategy of a solver.
#define SELF CLASP_SEARCH_REDUCE_OPTIONS
#if defined(NOTIFY_SUBGROUPS)
GROUP_BEGIN(SELF)
#endif
OPTION(deletion    , "!,d", ARG_EXT(defaultsTo("basic,75,activity")->state(Value::value_defaulted), DEFINE_ENUM_MAPPING(ReduceStrategy::Algorithm,\
       MAP("basic", ReduceStrategy::reduce_linear), MAP("sort", ReduceStrategy::reduce_stable),\
       MAP("ipSort", ReduceStrategy::reduce_sort) , MAP("ipHeap", ReduceStrategy::reduce_heap))\
       DEFINE_ENUM_MAPPING(ReduceStrategy::Score, MAP("activity",ReduceStrategy::score_act), MAP("lbd", ReduceStrategy::score_lbd), MAP("mixed", ReduceStrategy::score_both))),\
       "Configure deletion algorithm [%D]\n"                                    \
       "      %A: <algo>[,<n {1..100}>][,<sc>]\n"                               \
       "        <algo>: Use {basic|sort|ipSort|ipHeap} algorithm\n"             \
       "        <n>   : Delete at most <n>%% of nogoods on reduction    [75]\n" \
       "        <sc>  : Use {activity|lbd|mixed} nogood scores    [activity]\n" \
       "      no      : Disable nogood deletion", FUN(arg){\
       ReduceStrategy::Algorithm algo = ReduceStrategy::reduce_linear; uint32 n; ReduceStrategy::Score sc;\
       return ITE(arg.off(), (SELF.disable(), true), arg>>algo>>opt(n = 75)>>opt(sc = ReduceStrategy::score_act)\
         && SET(SELF.strategy.algo, (uint32)algo) && SET_R(SELF.strategy.fReduce, n, 1, 100) && SET(SELF.strategy.score, (uint32)sc));},\
       GET_IF(SELF.strategy.fReduce, (ReduceStrategy::Algorithm)SELF.strategy.algo, SELF.strategy.fReduce,(ReduceStrategy::Score)SELF.strategy.score))
OPTION(del_grow    , "!", NO_ARG, "Configure size-based deletion policy\n" \
       "      %A: <f>[,<g>][,<sched>] (<f> >= 1.0)\n"          \
       "        <f>     : Keep at most T = X*(<f>^i) learnt nogoods with X being the\n"\
       "                  initial limit and i the number of times <sched> fired\n"     \
       "        <g>     : Stop growth once T > P*<g> (0=no limit)      [3.0]\n"        \
       "        <sched> : Set grow schedule (<type {F|L|x|+}>) [grow on restart]", FUN(arg){ double f; double g; ScheduleStrategy sc = ScheduleStrategy::def();\
       return ITE(arg.off(), (SELF.growSched = ScheduleStrategy::none(), SELF.fGrow = 0.0f, true),\
         arg>>f>>opt(g = 3.0)>>opt(sc) && SET_R(SELF.fGrow, (float)f, 1.0f, FLT_MAX) && SET_R(SELF.fMax, (float)g, 0.0f, FLT_MAX)\
           && (sc.defaulted() || sc.type != ScheduleStrategy::User) && (SELF.growSched=sc, true));},\
       GET_FUN(str) { if (SELF.fGrow == 0.0f) str<<off; else { str<<SELF.fGrow<<SELF.fMax; if (!SELF.growSched.disabled()) str<<SELF.growSched;}})
OPTION(del_cfl     , "!", ARG(arg("<sched>")), "Configure conflict-based deletion policy\n" \
       "      %A:   <type {F|L|x|+}>,<args>... (see restarts)", FUN(arg){\
       return ITE(arg.off(), (SELF.cflSched=ScheduleStrategy::none()).disabled(), arg>>SELF.cflSched && SELF.cflSched.type != ScheduleStrategy::User);}, GET(SELF.cflSched))
OPTION(del_init  , ""  , ARG(defaultsTo("3.0")->state(Value::value_defaulted)), "Configure initial deletion limit\n"\
       "      %A: <f>[,<n>,<o>] (<f> > 0)\n" \
       "        <f>    : Set initial limit to P=estimated problem size/<f> [%D]\n" \
       "        <n>,<o>: Clamp initial limit to the range [<n>,<n>+<o>]" , FUN(arg) { double f; uint32 lo = 10; uint32 hi = UINT32_MAX;\
       return arg>>f>>opt(lo)>>opt(hi) && f > 0.0 && (SELF.fInit = float(1.0 / f)) > 0 && SET_OR_FILL(SELF.initRange.lo, lo) && SET_OR_FILL(SELF.initRange.hi, (uint64(hi)+SELF.initRange.lo));},\
       GET_IF(SELF.fInit, 1.0/SELF.fInit, SELF.initRange.lo, SELF.initRange.hi - SELF.initRange.lo))
OPTION(del_estimate, "", ARG(arg("0..3")->implicit("1")), "Use estimated problem complexity in limits", STORE_LEQ(SELF.strategy.estimate, 3u), GET(SELF.strategy.estimate))
OPTION(del_max     , "!", ARG(arg("<n>,<X>")), "Keep at most <n> learnt nogoods taking up to <X> MB", FUN(arg) { uint32 n = UINT32_MAX; uint32 mb = 0; \
       return (arg.off() || arg>>n>>opt(mb)) && SET_GEQ(SELF.maxRange, n, 1u) && SET(SELF.memMax, mb);}, GET(SELF.maxRange, SELF.memMax))
OPTION(del_glue    , "", NO_ARG, "Configure glue clause handling\n" \
       "      %A: <n {0..15}>[,<m {0|1}>]\n"                                    \
       "        <n>: Do not delete nogoods with LBD <= <n>\n"                    \
       "        <m>: Count (0) or ignore (1) glue clauses in size limit [0]", FUN(arg) {uint32 lbd; uint32 m = 0; \
       return arg>>lbd>>opt(m) && SET(SELF.strategy.glue, lbd) && SET(SELF.strategy.noGlue, m);}, GET(SELF.strategy.glue, SELF.strategy.noGlue))
OPTION(del_on_restart, "", ARG(arg("<n>")), "Delete %A%% of learnt nogoods on each restart", STORE_LEQ(SELF.strategy.fRestart, 100u), GET(SELF.strategy.fRestart))
#if defined(NOTIFY_SUBGROUPS)
GROUP_END(SELF)
#endif
#undef SELF
#undef CLASP_SEARCH_REDUCE_OPTIONS
GROUP_END(CLASP_SEARCH_OPTIONS)
#undef CLASP_SEARCH_OPTIONS
#endif

//! ASP-front-end options stored in an Clasp::Asp::LogicProgram::AspOptions object.
#if defined(CLASP_ASP_OPTIONS)
#define SELF CLASP_ASP_OPTIONS
GROUP_BEGIN(SELF)
OPTION(trans_ext  , "!", ARG_EXT(arg("<mode>"), DEFINE_ENUM_MAPPING(Asp::LogicProgram::ExtendedRuleMode,\
       MAP("no"    , Asp::LogicProgram::mode_native)          , MAP("all" , Asp::LogicProgram::mode_transform),\
       MAP("choice", Asp::LogicProgram::mode_transform_choice), MAP("card", Asp::LogicProgram::mode_transform_card),\
       MAP("weight", Asp::LogicProgram::mode_transform_weight), MAP("scc" , Asp::LogicProgram::mode_transform_scc),\
       MAP("integ" , Asp::LogicProgram::mode_transform_integ) , MAP("dynamic", Asp::LogicProgram::mode_transform_dynamic))),\
       "Configure handling of extended rules\n"\
       "      %A: {all|choice|card|weight|integ|dynamic}\n"\
       "        all    : Transform all extended rules to basic rules\n"\
       "        choice : Transform choice rules, but keep cardinality and weight rules\n"\
       "        card   : Transform cardinality rules, but keep choice and weight rules\n"\
       "        weight : Transform cardinality and weight rules, but keep choice rules\n"\
       "        scc    : Transform \"recursive\" cardinality and weight rules\n"\
       "        integ  : Transform cardinality integrity constraints\n"\
       "        dynamic: Transform \"simple\" extended rules, but keep more complex ones", STORE(SELF.erMode), GET((Asp::LogicProgram::ExtendedRuleMode)SELF.erMode))
OPTION(eq, "", ARG(arg("<n>")), "Configure equivalence preprocessing\n" \
       "      Run for at most %A iterations (-1=run to fixpoint)", STORE_OR_FILL(SELF.iters), GET(SELF.iters))
OPTION(backprop    ,"!,@1", ARG(flag()), "Use backpropagation in ASP-preprocessing", STORE_FLAG(SELF.backprop), GET(SELF.backprop))
OPTION(supp_models , ",@1", ARG(flag()), "Compute supported models", STORE_FLAG(SELF.suppMod), GET(SELF.suppMod))
OPTION(no_ufs_check, ",@1", ARG(flag()), "Disable unfounded set check", STORE_FLAG(SELF.noSCC), GET(SELF.noSCC))
OPTION(no_gamma    , ",@1", ARG(flag()), "Do not add gamma rules for non-hcf disjunctions", STORE_FLAG(SELF.noGamma), GET(SELF.noGamma))
OPTION(eq_dfs      , ",@2", ARG(flag()), "Enable df-order in eq-preprocessing", STORE_FLAG(SELF.dfOrder), GET(SELF.dfOrder))
OPTION(dlp_old_map , ",@3", ARG(flag()), "Enable old mapping for disjunctive LPs", STORE_FLAG(SELF.oldMap), GET(SELF.oldMap))
GROUP_END(SELF)
#undef CLASP_ASP_OPTIONS
#undef SELF
#endif

//! Options for the solving algorithm (see Clasp::SolveOptions)
#if defined(CLASP_SOLVE_OPTIONS)
#define SELF CLASP_SOLVE_OPTIONS
GROUP_BEGIN(SELF)
OPTION(solve_limit , ",@1", ARG(arg("<n>[,<m>]")), "Stop search after <n> conflicts or <m> restarts\n", FUN(arg) {\
       uint32 n = UINT32_MAX; uint32 m = UINT32_MAX;\
       return ((arg.off() && arg.peek() != '0') || arg>>n>>opt(m)) && (SELF.limit=SolveLimits(n == UINT32_MAX ? UINT64_MAX : n, m == UINT32_MAX ? UINT64_MAX : m), true);},\
       GET((uint32)Range<uint64>(0u,UINT32_MAX).clamp(SELF.limit.conflicts),(uint32)Range<uint64>(0u,UINT32_MAX).clamp(SELF.limit.restarts)))
#if CLASP_HAS_THREADS
OPTION(parallel_mode, ",t", ARG_EXT(arg("<arg>"), DEFINE_ENUM_MAPPING(SolveOptions::Algorithm::SearchMode,\
       MAP("compete", SolveOptions::Algorithm::mode_compete), MAP("split", SolveOptions::Algorithm::mode_split))),\
       "Run parallel search with given number of threads\n" \
       "      %A: <n {1..64}>[,<mode {compete|split}>]\n"   \
       "        <n>   : Number of threads to use in search\n"\
       "        <mode>: Run competition or splitting based search [compete]\n", FUN(arg){\
       uint32 n; SolveOptions::Algorithm::SearchMode mode;\
       return arg>>n>>opt(mode = SolveOptions::Algorithm::mode_compete) && SET_R(SELF.algorithm.threads, n, 1u, 64u) && SET(SELF.algorithm.mode, mode);},\
       GET(SELF.algorithm.threads, (SolveOptions::Algorithm::SearchMode)SELF.algorithm.mode))
OPTION(global_restarts, ",@1", ARG(arg("<X>")), "Configure global restart policy\n" \
       "      %A: <n>[,<sched>]\n"                                        \
       "        <n> : Maximal number of global restarts (0=disable)\n"    \
       "     <sched>: Restart schedule [x,100,1.5] (<type {F|L|x|+}>)\n", FUN(arg) {\
       return ITE(arg.off(), (SELF.restarts = SolveOptions::GRestarts(), true), arg>>SELF.restarts.maxR>>opt(SELF.restarts.sched = ScheduleStrategy())\
         && SELF.restarts.maxR && SELF.restarts.sched.type != ScheduleStrategy::User);},\
       GET_IF(SELF.restarts.maxR, SELF.restarts.maxR, SELF.restarts.sched))
OPTION(distribute, "!,@1", ARG_EXT(defaultsTo("conflict,global,4"), \
       DEFINE_ENUM_MAPPING(Distributor::Policy::Types, MAP("all", Distributor::Policy::all), MAP("short", Distributor::Policy::implicit), MAP("conflict", Distributor::Policy::conflict), MAP("loop" , Distributor::Policy::loop))\
       DEFINE_ENUM_MAPPING(SolveOptions::Distribution::Mode, MAP("global", SolveOptions::Distribution::mode_global), MAP("local", SolveOptions::Distribution::mode_local))),\
       "Configure nogood distribution [%D]\n" \
       "      %A: <type>[,<mode>][,<lbd {0..127}>][,<size>]\n"            \
       "        <type> : Distribute {all|short|conflict|loop} nogoods\n"  \
       "        <mode> : Use {global|local} distribution   [global]\n"    \
       "        <lbd>  : Distribute only if LBD  <= <lbd>  [4]\n"         \
       "        <size> : Distribute only if size <= <size> [-1]",         \
       FUN(arg) { Distributor::Policy::Types type; SolveOptions::Distribution::Mode mode = SolveOptions::Distribution::mode_global; uint32 lbd; uint32 size; \
         if (arg.off()) { SELF.distribute.policy() = Distributor::Policy(0, 0, 0); return true; }
         return (arg >> type) && (arg.peek() < 'A' || arg >> opt(mode)) && arg >> opt(lbd = 4) >> opt(size = UINT32_MAX)
           && SET(SELF.distribute.types, (uint32)type) && SET(SELF.distribute.mode, (uint32)mode) && SET(SELF.distribute.lbd, lbd) && SET_OR_FILL(SELF.distribute.size, size);},\
       GET_FUN(str) { ITE(!SELF.distribute.types, str << off,\
         str << (Distributor::Policy::Types)SELF.distribute.types << (SolveOptions::Distribution::Mode)SELF.distribute.mode << SELF.distribute.lbd << SELF.distribute.size); })
OPTION(integrate, ",@1", ARG_EXT(defaultsTo("gp")->state(Value::value_defaulted),\
       DEFINE_ENUM_MAPPING(SolveOptions::Integration::Filter, \
       MAP("all", SolveOptions::Integration::filter_no), MAP("gp", SolveOptions::Integration::filter_gp),\
       MAP("unsat", SolveOptions::Integration::filter_sat), MAP("active", SolveOptions::Integration::filter_heuristic))\
       DEFINE_ENUM_MAPPING(SolveOptions::Integration::Topology, \
       MAP("all" , SolveOptions::Integration::topo_all) , MAP("ring" , SolveOptions::Integration::topo_ring),\
       MAP("cube", SolveOptions::Integration::topo_cube), MAP("cubex", SolveOptions::Integration::topo_cubex))),\
       "Configure nogood integration [%D]\n" \
       "      %A: <pick>[,<n>][,<topo>]\n"                                           \
       "        <pick>: Add {all|unsat|gp(unsat wrt guiding path)|active} nogoods\n" \
       "        <n>   : Always keep at least last <n> integrated nogoods   [1024]\n" \
       "        <topo>: Accept nogoods from {all|ring|cube|cubex} peers    [all]\n", FUN(arg) {\
       SolveOptions::Integration::Filter pick = SolveOptions::Integration::filter_no; uint32 n; SolveOptions::Integration::Topology topo;
       return arg>>pick>>opt(n = 1024)>>opt(topo = SolveOptions::Integration::topo_all) && SET(SELF.integrate.filter, (uint32)pick) && SET_OR_FILL(SELF.integrate.grace, n) && SET(SELF.integrate.topo, (uint32)topo);},\
       GET((SolveOptions::Integration::Filter)SELF.integrate.filter, SELF.integrate.grace, (SolveOptions::Integration::Topology)SELF.integrate.topo))
#endif
OPTION(enum_mode   , ",e", ARG_EXT(defaultsTo("auto")->state(Value::value_defaulted), DEFINE_ENUM_MAPPING(SolveOptions::EnumType,\
       MAP("bt", SolveOptions::enum_bt), MAP("record", SolveOptions::enum_record), MAP("domRec", SolveOptions::enum_dom_record),\
       MAP("brave", SolveOptions::enum_brave), MAP("cautious", SolveOptions::enum_cautious), MAP("query", SolveOptions::enum_query),\
       MAP("auto", SolveOptions::enum_auto), MAP("user", SolveOptions::enum_user))),\
       "Configure enumeration algorithm [%D]\n" \
       "      %A: {bt|record|brave|cautious|auto}\n" \
       "        bt      : Backtrack decision literals from solutions\n" \
       "        record  : Add nogoods for computed solutions\n" \
       "        domRec  : Add nogoods over true domain atoms\n" \
       "        brave   : Compute brave consequences (union of models)\n" \
       "        cautious: Compute cautious consequences (intersection of models)\n" \
       "        auto    : Use bt for enumeration and record for optimization", STORE(SELF.enumMode), GET(SELF.enumMode))
OPTION(project, "!", ARG_EXT(arg("<arg>")->implicit("auto,3"), DEFINE_ENUM_MAPPING(ProjectMode_t::Mode,\
       MAP("auto", ProjectMode_t::Implicit), MAP("show", ProjectMode_t::Output), MAP("project", ProjectMode_t::Explicit))),\
       "Enable projective solution enumeration\n"                            \
       "      %A: {show|project|auto}[,<bt {0..3}>] (Implicit: %I)\n"        \
       "        Project to atoms in show or project directives, or\n"        \
       "        select depending on the existence of a project directive\n"  \
       "      <bt> : Additional options for enumeration algorithm 'bt'\n"    \
       "        Use activity heuristic (1) when selecting backtracking literal\n" \
       "        and/or progress saving (2) when retracting solution literals", \
       FUN(arg) { ProjectMode m = ProjectMode_t::Implicit; uint32 p = 0; \
         return (arg.off() || (arg.peek() < '9' && arg >> p) || ((arg >> m >> opt(p)) && (p = (p<<1)|1) != 0u)) && SET(SELF.proMode, m) && SET_LEQ(SELF.project, p, 7u);},\
       GET_IF(SELF.project, SELF.proMode, SELF.project >> 1))
OPTION(models, ",n", ARG(arg("<n>")), "Compute at most %A models (0 for all)\n", STORE(SELF.numModels), GET(SELF.numModels))
OPTION(opt_mode   , "", ARG_EXT(arg("<arg>"), DEFINE_ENUM_MAPPING(MinimizeMode_t::Mode,\
       MAP("opt" , MinimizeMode_t::optimize), MAP("enum"  , MinimizeMode_t::enumerate),\
       MAP("optN", MinimizeMode_t::enumOpt) , MAP("ignore", MinimizeMode_t::ignore))),\
       "Configure optimization algorithm\n"\
       "      %A: <mode {opt|enum|optN|ignore}>[,<bound>...]\n"       \
       "        opt   : Find optimal model\n"                         \
       "        enum  : Find models with costs <= <bound>\n"          \
       "        optN  : Find optimum, then enumerate optimal models\n"\
       "        ignore: Ignore optimize statements\n"                 \
       "      <bound> : Set initial bound for objective function(s)", \
       FUN(arg) { MinimizeMode_t::Mode m = MinimizeMode_t::optimize; SumVec B; return (arg >> m >> opt(B)) && SET(SELF.optMode, m) && (SELF.optBound.swap(B), true); }, \
       GET_FUN(str) { str << SELF.optMode; if (!SELF.optBound.empty()) str << SELF.optBound; })
GROUP_END(SELF)
#undef CLASP_SOLVE_OPTIONS
#undef SELF
#endif

#undef GROUP_BEGIN
#undef GROUP_END
#undef OPTION
#undef NOTIFY_SUBGROUPS
#undef ARG
#undef ARG_EXT
#undef NO_ARG
