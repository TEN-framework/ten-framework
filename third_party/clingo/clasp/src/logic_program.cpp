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
#include <clasp/logic_program.h>
#include <clasp/shared_context.h>
#include <clasp/solver.h>
#include <clasp/minimize_constraint.h>
#include <clasp/util/misc_types.h>
#include <clasp/asp_preprocessor.h>
#include <clasp/clause.h>
#include <clasp/dependency_graph.h>
#include <clasp/parser.h>
#include <potassco/theory_data.h>
#include <potassco/string_convert.h>
#include <stdexcept>
#include <cctype>
#include <cstdio>
namespace Clasp { namespace Asp {
/////////////////////////////////////////////////////////////////////////////////////////
// Statistics
/////////////////////////////////////////////////////////////////////////////////////////
#define RK(x) RuleStats::x
const char* RuleStats::toStr(int k) {
	POTASSCO_ASSERT(k >= 0 && uint32(k) <= numKeys(), "Invalid key");
	switch (k) {
		case Normal   : return "Normal";
		case Choice   : return "Choice";
		case Minimize : return "Minimize";
		case Acyc     : return "Acyc";
		case Heuristic: return "Heuristic";
		default:        return "None";
	}
}
uint32 RuleStats::sum() const {
	return std::accumulate(key, key + numKeys(), uint32(0));
}
const char* BodyStats::toStr(int t) {
	POTASSCO_ASSERT(t >= 0 && uint32(t) < numKeys(), "Invalid body type!");
	switch (t) {
		default           : return "Normal";
		case Body_t::Count: return "Count";
		case Body_t::Sum  : return "Sum";
	}
}
uint32 BodyStats::sum() const {
	return std::accumulate(key, key + numKeys(), uint32(0));
}

namespace {
template <unsigned i>
double sumBodies(const LpStats* self) { return self->bodies[i].sum(); }
template <unsigned i>
double sumRules(const LpStats* self) { return self->rules[i].sum(); }
double sumEqs(const LpStats* self)   { return self->eqs(); }
}
#define LP_STATS( APPLY )                                        \
	APPLY("atoms"               , VALUE(atoms))                  \
	APPLY("atoms_aux"           , VALUE(auxAtoms))               \
	APPLY("disjunctions"        , VALUE(disjunctions[0]))        \
	APPLY("disjunctions_non_hcf", VALUE(disjunctions[1]))        \
	APPLY("bodies"              , FUNC(sumBodies<0>))            \
	APPLY("bodies_tr"           , FUNC(sumBodies<1>))            \
	APPLY("sum_bodies"          , VALUE(bodies[0][Body_t::Sum])) \
	APPLY("sum_bodies_tr"       , VALUE(bodies[1][Body_t::Sum])) \
	APPLY("count_bodies"        , VALUE(bodies[0][Body_t::Count]))\
	APPLY("count_bodies_tr"     , VALUE(bodies[1][Body_t::Count]))\
	APPLY("sccs"                , VALUE(sccs))                   \
	APPLY("sccs_non_hcf"        , VALUE(nonHcfs))                \
	APPLY("gammas"              , VALUE(gammas))                 \
	APPLY("ufs_nodes"           , VALUE(ufsNodes))               \
	APPLY("rules"               , FUNC(sumRules<0>))             \
	APPLY("rules_normal"        , VALUE(rules[0][RK(Normal)]))   \
	APPLY("rules_choice"        , VALUE(rules[0][RK(Choice)]))   \
	APPLY("rules_minimize"      , VALUE(rules[0][RK(Minimize)])) \
	APPLY("rules_acyc"          , VALUE(rules[0][RK(Acyc)]))     \
	APPLY("rules_heuristic"     , VALUE(rules[0][RK(Heuristic)]))\
	APPLY("rules_tr"            , FUNC(sumRules<1>))             \
	APPLY("rules_tr_normal"     , VALUE(rules[1][RK(Normal)]))   \
	APPLY("rules_tr_choice"     , VALUE(rules[1][RK(Choice)]))   \
	APPLY("rules_tr_minimize"   , VALUE(rules[1][RK(Minimize)])) \
	APPLY("rules_tr_acyc"       , VALUE(rules[1][RK(Acyc)]))     \
	APPLY("rules_tr_heuristic"  , VALUE(rules[1][RK(Heuristic)]))\
	APPLY("eqs"                 , FUNC(sumEqs))                  \
	APPLY("eqs_atom"            , VALUE(eqs_[Var_t::Atom-1]))    \
	APPLY("eqs_body"            , VALUE(eqs_[Var_t::Body-1]))    \
	APPLY("eqs_other"           , VALUE(eqs_[Var_t::Hybrid-1]))

void LpStats::reset() {
	std::memset(this, 0, sizeof(LpStats));
}

void LpStats::accu(const LpStats& o) {
	atoms    += o.atoms;
	auxAtoms += o.auxAtoms;
	ufsNodes += o.ufsNodes;
	if (sccs == PrgNode::noScc || o.sccs == PrgNode::noScc) {
		sccs    = o.sccs;
		nonHcfs = o.nonHcfs;
	}
	else {
		sccs   += o.sccs;
		nonHcfs+= o.nonHcfs;
	}
	for (int i = 0; i != 2; ++i) {
		disjunctions[i] += o.disjunctions[i];
		for (uint32 k = 0; k != BodyStats::numKeys(); ++k) {
			bodies[i][k] += o.bodies[i][k];
		}
		for (uint32 k = 0; k != RuleStats::numKeys(); ++k) {
			rules[i][k] += o.rules[i][k];
		}
	}
	for (int i = 0; i != sizeof(eqs_)/sizeof(eqs_[0]); ++i) {
		eqs_[i] += o.eqs_[i];
	}
}

static const char* lpStats_s[] = {
#define KEY(x, y) x,
	LP_STATS(KEY)
#undef KEY
	"lp"
};
uint32 LpStats::size() {
	return (sizeof(lpStats_s)/sizeof(const char*))-1;
}
const char* LpStats::key(uint32 i) {
	return i < size() ? lpStats_s[i] : throw std::out_of_range(POTASSCO_FUNC_NAME);
}
StatisticObject LpStats::at(const char* k) const {
#define MAP_IF(x, A) if (std::strcmp(k, x) == 0)  return A;
#define VALUE(X) StatisticObject::value(&(X))
#define FUNC(F) StatisticObject::value<LpStats, F>(this)
	LP_STATS(MAP_IF)
#undef VALUE
#undef FUNC
#undef MAP_IF
	throw std::out_of_range(POTASSCO_FUNC_NAME);
}
#undef LP_STATS
/////////////////////////////////////////////////////////////////////////////////////////
// class LogicProgram
/////////////////////////////////////////////////////////////////////////////////////////
static PrgAtom trueAtom_g(0, false);
static const uint32 falseId = PrgNode::noNode;
static const uint32 bodyId  = PrgNode::noNode + 1;
static bool init_trueAtom_g = (trueAtom_g.setEq(0), true);
inline bool isAtom(Id_t uid) { return Potassco::atom(Potassco::lit(uid)) < bodyId; }
inline bool isBody(Id_t uid) { return Potassco::atom(Potassco::lit(uid)) >= bodyId; }
inline Id_t nodeId(Id_t uid) { return Potassco::atom(Potassco::lit(uid)) - (isAtom(uid) ? 0 : bodyId); }
inline bool signId(Id_t uid) { return Potassco::lit(uid) < 0; }

namespace {
typedef std::pair<Atom_t, Potassco::Value_t> AtomVal;
inline uint32 encodeExternal(Atom_t a, Potassco::Value_t value) {
	return (a << 2) | static_cast<uint32>(value);
}
inline AtomVal decodeExternal(uint32 x) {
	return AtomVal(x >> 2, static_cast<Potassco::Value_t>(x & 3u));
}
struct LessBodySize {
	LessBodySize(const BodyList& bl) : bodies_(&bl) {}
	bool operator()(Id_t b1, Id_t b2 ) const {
		return (*bodies_)[b1]->size() < (*bodies_)[b2]->size()
			|| ((*bodies_)[b1]->size() == (*bodies_)[b2]->size() && (*bodies_)[b1]->type() < (*bodies_)[b2]->type());
	}
private:
	const BodyList* bodies_;
};
// Adds nogoods representing this node to the solver.
template <class NT>
bool toConstraint(NT* node, const LogicProgram& prg, ClauseCreator& c) {
	if (node->value() != value_free && !prg.ctx()->addUnary(node->trueLit())) {
    	return false;
	}
	return !node->relevant() || node->addConstraints(prg, c);
}
}

LogicProgram::LogicProgram() : theory_(0), input_(1, UINT32_MAX), auxData_(0), incData_(0) {
	POTASSCO_ASSERT(init_trueAtom_g, "invalid static init");
}
LogicProgram::~LogicProgram() { dispose(true); }
LogicProgram::Incremental::Incremental() : startScc(0) {}
void LogicProgram::dispose(bool force) {
	// remove rules
	std::for_each(bodies_.begin(), bodies_.end(), DestroyObject());
	std::for_each(disjunctions_.begin(), disjunctions_.end(), DestroyObject());
	std::for_each(extended_.begin(), extended_.end(), DeleteObject());
	std::for_each(minimize_.begin(), minimize_.end(), DeleteObject());
	PodVector<ShowPair>::destruct(show_);
	delete auxData_; auxData_ = 0;
	MinList().swap(minimize_);
	RuleList().swap(extended_);
	BodyList().swap(bodies_);
	DisjList().swap(disjunctions_);
	bodyIndex_.clear();
	disjIndex_.clear();
	VarVec().swap(initialSupp_);
	if (theory_) {
		theory_->reset();
	}
	if (force) {
		deleteAtoms(0);
		AtomList().swap(atoms_);
		AtomState().swap(atomState_);
		LpLitVec().swap(assume_);
		delete theory_;
		delete incData_;
		VarVec().swap(propQ_);
		stats.reset();
		incData_ = 0;
		theory_  = 0;
		input_   = AtomRange(1, UINT32_MAX);
		statsId_ = 0;
	}
	rule_.clear();
}
void LogicProgram::deleteAtoms(uint32 start) {
	for (AtomList::const_iterator it = atoms_.begin() + start, end = atoms_.end(); it != end; ++it) {
		if (*it != &trueAtom_g) { delete *it; }
	}
}
bool LogicProgram::doStartProgram() {
	dispose(true);
	// atom 0 is always true
	PrgAtom* trueAt = new PrgAtom(0, false);
	atoms_.push_back(trueAt);
	trueAt->assignValue(value_true);
	trueAt->setInUpper(true);
	trueAt->setLiteral(lit_true());
	atomState_.set(0, AtomState::fact_flag);
	auxData_ = new Aux();
	return true;
}
void LogicProgram::setOptions(const AspOptions& opts) {
	opts_ = opts;
	if (opts.suppMod) { opts_.noSCC = 1; }
	if (opts.suppMod && ctx() && ctx()->sccGraph.get()) {
		ctx()->warn("'supp-models' ignored for non-tight programs.");
		opts_.suppMod = 0;
		opts_.noSCC   = 0;
	}
}
void LogicProgram::enableDistinctTrue() {
	opts_.distinctTrue();
}
ProgramParser* LogicProgram::doCreateParser() {
	return new AspParser(*this);
}
bool LogicProgram::doUpdateProgram() {
	if (!incData_) { incData_ = new Incremental(); }
	if (!frozen()) { return true; }
	// delete bodies/disjunctions...
	dispose(false);
	setFrozen(false);
	auxData_ = new Aux();
	assume_.clear();
	if (theory_) { theory_->update(); }
	incData_->unfreeze.clear();
	input_.hi = std::min(input_.hi, endAtom());
	// reset prop queue and add supported atoms from previous steps
	// {ai | ai in P}.
	PrgBody* support = input_.hi > 1 ? getTrueBody() : 0;
	propQ_.clear();
	for (Atom_t i = 1, end = startAuxAtom(); i != end; ++i) {
		if (getRootId(i) >= end) {
			// atom is equivalent to some aux atom - make i the new root
			uint32 r = getRootId(i);
			std::swap(atoms_[i], atoms_[r]);
			atoms_[r]->setEq(i);
		}
		// remove dangling references
		PrgAtom* a = atoms_[i];
		a->clearSupports();
		a->clearDeps(PrgAtom::dep_all);
		a->setIgnoreScc(false);
		if (a->relevant() || a->frozen()) {
			ValueRep v = a->value();
			a->setValue(value_free);
			a->resetId(i, !a->frozen());
			if (ctx()->master()->value(a->var()) != value_free && !a->frozen()) {
				v = ctx()->master()->isTrue(a->literal()) ? value_true : value_false;
			}
			if (v != value_free) { assignValue(a, v, PrgEdge::noEdge()); }
			if (!a->frozen() && a->value() != value_false) {
				a->setIgnoreScc(true);
				support->addHead(a, PrgEdge::GammaChoice);
			}
		}
		else if (a->removed() || (!a->eq() && a->value() == value_false)) {
			a->resetId(i, true);
			a->setValue(value_false);
			atomState_.set(i, AtomState::false_flag);
		}
	}
	// delete any introduced aux atoms
	// this is safe because aux atoms are never part of the input program
	// it is necessary in order to free their ids, i.e. the id of an aux atom
	// from step I might be needed for a program atom in step I+1
	deleteAtoms(startAuxAtom());
	atoms_.erase(atoms_.begin()+startAuxAtom(), atoms_.end());
	uint32 nAtoms = (uint32)atoms_.size();
	atomState_.resize(nAtoms);
	input_ = AtomRange(nAtoms, UINT32_MAX);
	stats.reset();
	statsId_ = 0;
	return true;
}
bool LogicProgram::doEndProgram() {
	if (!frozen() && ctx()->ok()) {
		prepareProgram(!opts_.noSCC);
		addConstraints();
		addDomRules();
		addAcycConstraint();
	}
	return ctx()->ok();
}

bool LogicProgram::clone(SharedContext& oCtx) {
	assert(frozen());
	if (&oCtx == ctx()) {
		return true;
	}
	for (Var v = oCtx.numVars() + 1; ctx()->validVar(v); ++v) {
		oCtx.addVar(Var_t::Atom, ctx()->varInfo(v).rep);
	}
	SharedContext* t = ctx();
	setCtx(&oCtx);
	bool ok = addConstraints();
	if (ok) {
		oCtx.output    = ctx()->output;
		oCtx.heuristic = t->heuristic;
	}
	setCtx(t);
	return ok;
}

void LogicProgram::addMinimize() {
	POTASSCO_ASSERT(frozen());
	for (MinList::iterator it = minimize_.begin(), end = minimize_.end(); it != end; ++it) {
		const LpWLitVec& lits = (*it)->lits;
		const weight_t   prio = (*it)->prio;
		for (LpWLitVec::const_iterator xIt = lits.begin(), xEnd = lits.end(); xIt != xEnd; ++xIt) {
			addMinLit(prio, WeightLiteral(getLiteral(Potassco::id(xIt->lit)), xIt->weight));
		}
		// Make sure minimize constraint is not empty
		if (lits.empty()) addMinLit(prio, WeightLiteral(lit_false(), 1));
	}
}
static void outRule(Potassco::AbstractProgram& out, const Rule& r) {
	if (r.normal()) { out.rule(r.ht, r.head, r.cond); }
	else            { out.rule(r.ht, r.head, r.agg.bound, r.agg.lits); }
}

void LogicProgram::accept(Potassco::AbstractProgram& out) {
	if (!ok()) {
		out.rule(Head_t::Disjunctive, Potassco::toSpan<Potassco::Atom_t>(), Potassco::toSpan<Potassco::Lit_t>());
		return;
	}
	// visit external directives
	for (VarVec::const_iterator it = auxData_->external.begin(), end = auxData_->external.end(); it != end; ++it) {
		AtomVal x = decodeExternal(*it);
		out.external(x.first, x.second);
	}
	// visit eq- and assigned atoms
	for (Atom_t i = startAtom(); i < atoms_.size(); ++i) {
		if (atoms_[i]->eq()) {
			Potassco::AtomSpan head = Potassco::toSpan(&i, 1);
			Potassco::Lit_t    body = Potassco::lit(getRootId(i));
			if (isFact(Potassco::atom(body))) {
				out.rule(Head_t::Disjunctive, head, Potassco::toSpan<Potassco::Lit_t>());
			}
			else if (inProgram(Potassco::atom(body))) {
				out.rule(Head_t::Disjunctive, head, Potassco::toSpan(&body, 1));
			}
		}
		else if (!atomState_.isFact(i) && atoms_[i]->value() != value_free) {
			Potassco::AtomSpan head = Potassco::toSpan<Potassco::Atom_t>();
			Potassco::Lit_t    body = Potassco::neg(i);
			if (atoms_[i]->value() != value_false) {
				out.rule(Head_t::Disjunctive, head, Potassco::toSpan(&body, 1));
			}
			else if (inProgram(i)) {
				body = Potassco::neg(body);
				out.rule(Head_t::Disjunctive, head, Potassco::toSpan(&body, 1));
			}
		}
	}
	// visit program rules
	const bool simp = frozen();
	using Potassco::Lit_t;
	VarVec choice;
	for (BodyList::iterator bIt = bodies_.begin(); bIt != bodies_.end(); ++bIt) {
		rule_.clear();
		choice.clear();
		Atom_t head;
		Lit_t  auxB;
		PrgBody* b = *bIt;
		if (b->relevant() && (b->inRule() || b->value() == value_false) && b->toData(*this, rule_)) {
			if (b->value() == value_false) {
				outRule(out, rule_.rule());
				continue;
			}
			uint32 numDis = 0;
			Rule r = rule_.rule();
			for (PrgBody::head_iterator hIt = b->heads_begin(); hIt != b->heads_end(); ++hIt) {
				if (hIt->isGamma() || (simp && !getHead(*hIt)->hasVar())) { continue; }
				if (hIt->isAtom() && hIt->node() && inProgram(hIt->node())) {
					if      (hIt->isNormal()) { r.head = Potassco::toSpan(&(head = hIt->node()), 1); outRule(out, r); }
					else if (hIt->isChoice()) { choice.push_back(hIt->node()); }
					if (simp && getRootAtom(hIt->node())->var() == b->var() && !r.normal()) {
						// replace complex body with head atom
						auxB = Potassco::lit(hIt->node());
						if (getRootAtom(hIt->node())->literal() != b->literal()) { auxB *= -1; }
						r.bt   = Body_t::Normal;
						r.cond = Potassco::toSpan(&auxB, 1);
					}
				}
				else if (hIt->isDisj()) { ++numDis; }
			}
			if (!choice.empty()) {
				r.head = Potassco::toSpan(choice);
				r.ht   = Head_t::Choice;
				outRule(out, r);
			}
			for (PrgBody::head_iterator hIt = b->heads_begin(); hIt != b->heads_end() && numDis; ++hIt) {
				if (hIt->isDisj()) {
					PrgDisj* d = getDisj(hIt->node());
					r.head = Potassco::toSpan(d->begin(), d->size());
					r.ht   = Head_t::Disjunctive;
					outRule(out, r);
					--numDis;
				}
			}
		}
	}
	LpWLitVec wlits;
	for (MinList::iterator it = minimize_.begin(), end = minimize_.end(); it != end; ++it) {
		Potassco::WeightLitSpan ws = Potassco::toSpan((*it)->lits);
		for (const Potassco::WeightLit_t* x = Potassco::begin(ws), *xEnd = Potassco::end(ws); x != xEnd; ++x) {
			if (x->weight == 0 || !inProgram(Potassco::atom(*x))) { // simplify literals
				wlits.assign(Potassco::begin(ws), x);
				for (; x != xEnd; ++x) {
					if (x->weight == 0)
						continue;
					Atom_t atom = getRootId(Potassco::atom(*x));
					if (x->weight < 0 || x->lit < 0 || inProgram(atom)) {
						wlits.push_back(*x);
					}
				}
				ws = Potassco::toSpan(wlits);
				break;
			}
		}
		out.minimize((*it)->prio, ws);
	}
	Potassco::LitVec lits;
	// visit output directives
	for (ShowVec::const_iterator it = show_.begin(); it != show_.end(); ++it) {
		if (extractCondition(it->first, lits)) {
			out.output(Potassco::toSpan(it->second.c_str(), std::strlen(it->second.c_str())), Potassco::toSpan(lits));
		}
	}
	// visit projection directives
	if (!auxData_->project.empty()) {
		out.project(auxData_->project.back() ? Potassco::toSpan(auxData_->project) : Potassco::toSpan<Atom_t>());
	}
	// visit assumptions
	if (!assume_.empty()) {
		out.assume(Potassco::toSpan(assume_));
	}
	// visit heuristics
	if (!auxData_->dom.empty()) {
		for (DomRules::const_iterator it = auxData_->dom.begin(), end = auxData_->dom.end(); it != end; ++it) {
			if (extractCondition(it->cond, lits)) {
				out.heuristic(it->atom, static_cast<DomModType>(it->type), it->bias, it->prio, Potassco::toSpan(lits));
			}
		}
	}
	// visit acyc edges
	if (!auxData_->acyc.empty()) {
		for (AcycRules::const_iterator it = auxData_->acyc.begin(), end = auxData_->acyc.end(); it != end; ++it) {
			if (extractCondition(it->cond, lits)) {
				out.acycEdge(it->node[0], it->node[1], Potassco::toSpan(lits));
			}
		}
	}
	if (theory_ && theory_->numAtoms()) {
		struct This : public Potassco::TheoryData::Visitor {
			This(const Asp::LogicProgram& p, Potassco::AbstractProgram& o, Potassco::LitVec& c) : self(&p), out(&o), cond(&c) {}
			virtual void visit(const Potassco::TheoryData& data, Potassco::Id_t termId, const Potassco::TheoryTerm& t) {
				if (!addSeen(termId, 1)) { return; }
				data.accept(t, *this, Potassco::TheoryData::visit_current);
				Potassco::print(*out, termId, t);
			}
			virtual void visit(const Potassco::TheoryData& data, Potassco::Id_t elemId, const Potassco::TheoryElement& e) {
				if (!addSeen(elemId, 2)) { return; }
				data.accept(e, *this, Potassco::TheoryData::visit_current);
				cond->clear();
				if (e.condition()) { self->extractCondition(e.condition(), *cond); }
				out->theoryElement(elemId, e.terms(), Potassco::toSpan(*cond));
			}
			virtual void visit(const Potassco::TheoryData& data, const Potassco::TheoryAtom& a) {
				data.accept(a, *this, Potassco::TheoryData::visit_current);
				Potassco::print(*out, a);
				const Atom_t id = a.atom();
				if (self->validAtom(id) && self->atomState_.isSet(id, AtomState::false_flag) && !self->inProgram(id)) {
					Potassco::Lit_t x = Potassco::lit(id);
					out->rule(Head_t::Disjunctive, Potassco::toSpan<Potassco::Atom_t>(), Potassco::toSpan(&x, 1));
				}
			}
			bool addSeen(Potassco::Id_t id, unsigned char n) {
				if (id >= seen.size()) { seen.resize(id + 1, 0); }
				unsigned char old = seen[id];
				return (seen[id] |= n) != old;
			}
			typedef PodVector<unsigned char>::type IdSet;
			const Asp::LogicProgram*   self;
			Potassco::AbstractProgram* out;
			Potassco::LitVec*          cond;
			IdSet                      seen;
		} self(*this, out, lits);
		theory_->accept(self, Potassco::TheoryData::visit_current);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// Program mutating functions
/////////////////////////////////////////////////////////////////////////////////////////
#define check_not_frozen() POTASSCO_REQUIRE(!frozen(), "Can't update frozen program!")
#define check_modular(x, atomId) (void)( (!!(x)) || (throw RedefinitionError((atomId), this->findName((atomId))), 0))
RedefinitionError::RedefinitionError(unsigned atomId, const char* name)
	: std::logic_error(POTASSCO_FORMAT("redefinition of atom <'%s',%u>", name && *name ? name : "_", atomId)) {
}

Atom_t LogicProgram::newAtom() {
	check_not_frozen();
	Atom_t id = static_cast<Atom_t>(atoms_.size());
	atoms_.push_back( new PrgAtom(id) );
	return id;
}
Id_t LogicProgram::newCondition(const Potassco::LitSpan& cond) {
	check_not_frozen();
	SRule meta;
	if (simplifyNormal(Head_t::Disjunctive, Potassco::toSpan<Potassco::Atom_t>(), cond, rule_, meta)) {
		Rule r = rule_.rule();
		if (r.cond.size == 0) { return 0; }
		if (r.cond.size == 1) { return Potassco::id(r.cond[0]); }
		PrgBody* b = getBodyFor(r, meta);
		b->markFrozen();
		return static_cast<Id_t>(Clasp::Asp::bodyId | b->id());
	}
	return static_cast<Id_t>(Clasp::Asp::falseId);
}
LogicProgram& LogicProgram::addOutput(const ConstString& str, const Potassco::LitSpan& cond) {
	if (cond.size == 1) {
		POTASSCO_REQUIRE(Potassco::atom(cond[0]) < bodyId, "Atom out of bounds");
		return addOutput(str, Potassco::id(cond[0]));
	}
	if (!ctx()->output.filter(str)) {
		show_.push_back(ShowPair(newCondition(cond), str));
	}
	return *this;
}
LogicProgram& LogicProgram::addOutput(const ConstString& str, Id_t id) {
	if (!ctx()->output.filter(str) && id != falseId) {
		if (Potassco::atom(id) < bodyId) {
			resize(Potassco::atom(id));
		}
		show_.push_back(ShowPair(id, str));
	}
	return *this;
}

LogicProgram& LogicProgram::addProject(const Potassco::AtomSpan& atoms) {
	check_not_frozen();
	VarVec& pro = auxData_->project;
	if (!Potassco::empty(atoms)) {
		if (!pro.empty() && pro.back() == 0) { pro.pop_back(); }
		pro.insert(pro.end(), Potassco::begin(atoms), Potassco::end(atoms));
	}
	else if (pro.empty()) {
		pro.push_back(0);
	}
	return *this;
}

TheoryData& LogicProgram::theoryData() {
	if (!theory_) { theory_ = new TheoryData(); }
	return *theory_;
}

void LogicProgram::pushFrozen(PrgAtom* atom, ValueRep value) {
	if (!atom->frozen()) { frozen_.push_back(atom->id()); }
	atom->markFrozen(value);
}

LogicProgram& LogicProgram::addExternal(Atom_t atomId, Potassco::Value_t value) {
	check_not_frozen();
	PrgAtom* a = resize(atomId);
	if (a->supports() == 0 && (isNew(a->id()) || a->frozen())) {
		ValueRep fv = static_cast<ValueRep>(value);
		if (value == Potassco::Value_t::Release) {
			// add dummy edge - will be removed once we update the set of frozen atoms
			a->addSupport(PrgEdge::noEdge());
			fv = value_free;
		}
		pushFrozen(a, fv);
		auxData_->external.push_back(encodeExternal(a->id(), value));
	}
	return *this;
}

LogicProgram& LogicProgram::freeze(Atom_t atomId, ValueRep value) {
	POTASSCO_ASSERT(value < value_weak_true);
	return addExternal(atomId, static_cast<Potassco::Value_t>(value));
}

LogicProgram& LogicProgram::unfreeze(Atom_t atomId) {
	return addExternal(atomId, Potassco::Value_t::Release);
}
void LogicProgram::setMaxInputAtom(uint32 n) {
	check_not_frozen();
	resize(n++);
	POTASSCO_REQUIRE(n >= startAtom(), "invalid input range");
	input_.hi = n;
}
Atom_t LogicProgram::startAuxAtom() const {
	return validAtom(input_.hi) ? input_.hi : (uint32)atoms_.size();
}
bool LogicProgram::supportsSmodels() const {
	if (incData_ || theory_)        { return false; }
	if (!auxData_->dom.empty())     { return false; }
	if (!auxData_->acyc.empty())    { return false; }
	if (!assume_.empty())           { return false; }
	if (!auxData_->project.empty()) { return false; }
	for (ShowVec::const_iterator it = show_.begin(), end = show_.end(); it != end; ++it) {
		Potassco::Lit_t lit = Potassco::lit(it->first);
		if (lit <= 0 || static_cast<uint32>(lit) >= bodyId) { return false; }
	}
	return true;
}

bool LogicProgram::isExternal(Atom_t aId) const {
	if (!aId || !validAtom(aId)) { return false; }
	PrgAtom* a = getRootAtom(aId);
	return a->frozen() && (a->supports() == 0 || frozen());
}
bool LogicProgram::isFact(Atom_t aId) const {
	return validAtom(aId) && (atomState_.isFact(aId) || atomState_.isFact(getRootId(aId)));
}
bool LogicProgram::isDefined(Atom_t aId) const {
	if (!validAtom(aId) || getAtom(aId)->removed()) {
		return false;
	}
	PrgAtom* a = getAtom(aId);
	return isFact(aId) || (a->relevant() && a->supports() && !isExternal(aId));
}
bool LogicProgram::inProgram(Atom_t id) const {
	if (PrgAtom* a = (validAtom(id) ? getAtom(id) : 0)) {
		return a->relevant() && (a->supports() || a->frozen() || !isNew(id));
	}
	return false;
}
LogicProgram& LogicProgram::addAssumption(const Potassco::LitSpan& lits) {
	assume_.insert(assume_.end(), Potassco::begin(lits), Potassco::end(lits));
	return *this;
}

LogicProgram& LogicProgram::addAcycEdge(uint32 n1, uint32 n2, Id_t condId) {
	if (condId != falseId) {
		AcycArc arc = { condId, {n1, n2} };
		auxData_->acyc.push_back(arc);
	}
	upStat(RK(Acyc), 1);
	return *this;
}

LogicProgram& LogicProgram::addDomHeuristic(Atom_t atom, DomModType t, int bias, unsigned prio) {
	return addDomHeuristic(atom, t, bias, prio, Potassco::toSpan<Potassco::Lit_t>());
}
LogicProgram& LogicProgram::addDomHeuristic(Atom_t atom, DomModType type, int bias, unsigned prio, Id_t cond) {
	static_assert(sizeof(DomRule) == sizeof(uint32[3]), "Invalid DomRule size");
	if (cond != falseId) {
		auxData_->dom.push_back(DomRule());
		DomRule& x = auxData_->dom.back();
		x.atom = atom;
		x.type = type;
		x.cond = cond;
		x.bias = (int16)Range<int>(INT16_MIN, INT16_MAX).clamp(bias);
		x.prio = (uint16)Range<unsigned>(unsigned(0), unsigned(~uint16(0)) ).clamp(prio);
	}
	upStat(RK(Heuristic), 1);
	return *this;
}
LogicProgram& LogicProgram::addRule(const Rule& rule) {
	check_not_frozen();
	SRule meta;
	if (simplifyRule(rule, rule_, meta)) {
		Rule sRule = rule_.rule();
		upStat(sRule.ht);
		if (handleNatively(sRule)) { // and can be handled natively
			addRule(sRule, meta);
		}
		else {
			upStat(sRule.bt);
			if (Potassco::size(sRule.head) <= 1 && transformNoAux(sRule)) {
				// rule transformation does not require aux atoms - do it now
				int oId  = statsId_;
				statsId_ = 1;
				RuleTransform tm(*this);
				upStat(sRule.bt, -1);
				upStat(rule.ht, -1);
				tm.transform(sRule, RuleTransform::strategy_no_aux);
				statsId_ = oId;
			}
			else {
				// make sure we have all head atoms
				for (Potassco::AtomSpan::iterator it = Potassco::begin(sRule.head), end = Potassco::end(sRule.head); it != end; ++it) {
					resize(*it);
				}
				extended_.push_back(new RuleBuilder(rule_));
			}
		}
	}
	if (statsId_ == 0) {
		// Assume all (new) heads are initially in "upper" closure.
		for (Potassco::AtomSpan::iterator it = Potassco::begin(rule.head), end = Potassco::end(rule.head); it != end; ++it) {
			if (!isNew(*it)) continue;
			if (validAtom(*it))
				getAtom(*it)->setInUpper(true);
			else
				auxData_->skippedHeads.insert(*it);
		}
	}
	rule_.clear();
	return *this;
}

LogicProgram& LogicProgram::addRule(Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body) {
	return addRule(Rule::normal(ht, head, body));
}
LogicProgram& LogicProgram::addRule(Head_t ht, const Potassco::AtomSpan& head, Potassco::Weight_t bound, const Potassco::WeightLitSpan& lits) {
	return addRule(Rule::sum(ht, head, bound, lits));
}
LogicProgram& LogicProgram::addRule(Potassco::RuleBuilder& rb) {
	LogicProgramAdapter prg(*this);
	rb.end(&prg);
	return *this;
}
LogicProgram& LogicProgram::addMinimize(weight_t prio, const Potassco::WeightLitSpan& lits) {
	SingleOwnerPtr<Min> n(new Min());
	n->prio = prio;
	MinList::iterator it = std::lower_bound(minimize_.begin(), minimize_.end(), n.get(), CmpMin());
	if (it == minimize_.end() || (*it)->prio != prio) {
		n->lits.assign(Potassco::begin(lits), Potassco::end(lits));
		minimize_.insert(it, n.get());
		n.release();
		upStat(RuleStats::Minimize);
	}
	else {
		(*it)->lits.insert((*it)->lits.end(), Potassco::begin(lits), Potassco::end(lits));
	}
	// Touch all atoms in minimize -> these are input atoms even if they won't occur in a head.
	for (Potassco::WeightLitSpan::iterator wIt = Potassco::begin(lits), end = Potassco::end(lits); wIt != end; ++wIt) {
		resize(Potassco::atom(*wIt));
	}
	return *this;
}
#undef check_not_frozen
/////////////////////////////////////////////////////////////////////////////////////////
// Query functions
/////////////////////////////////////////////////////////////////////////////////////////
bool LogicProgram::isFact(PrgAtom* a) const {
	uint32 eqId = getRootId(a->id());
	if (atomState_.isFact(eqId)) {
		return true;
	}
	if (a->value() == value_true) {
		for (PrgAtom::sup_iterator it = a->supps_begin(), end = a->supps_end(); it != end; ++it) {
			if (it->isBody() && it->isNormal() && getBody(it->node())->bound() == 0) {
				return true;
			}
		}
	}
	return false;
}
Literal LogicProgram::getLiteral(Id_t id, MapLit_t m) const {
	Literal out = lit_false();
	Potassco::Id_t nId = nodeId(id);
	if (isAtom(id) && validAtom(nId)) {
		out = getRootAtom(nId)->literal();
		if (m == MapLit_t::Refined) {
			IndexMap::const_iterator dom;
			if ((dom = domEqIndex_.find(nId)) != domEqIndex_.end()) {
				out = posLit(dom->second);
			}
			else if (isSentinel(out) && incData_ && !incData_->steps.empty()) {
				Var v = isNew(id)
					? incData_->steps.back().second
					: std::lower_bound(incData_->steps.begin(), incData_->steps.end(), Incremental::StepTrue(nId, 0))->second;
				out = Literal(v, out.sign());
			}
		}
	}
	else if (isBody(id)) {
		POTASSCO_ASSERT(validBody(nId), "Invalid condition");
		out = getBody(getEqBody(nId))->literal();
	}
	return out ^ signId(id);
}
void LogicProgram::doGetAssumptions(LitVec& out) const {
	for (VarVec::const_iterator it = frozen_.begin(), end = frozen_.end(); it != end; ++it) {
		Literal lit = getRootAtom(*it)->assumption();
		if (lit != lit_true()) { out.push_back( lit ); }
	}
	for (Potassco::LitVec::const_iterator it = assume_.begin(), end = assume_.end(); it != end; ++it) {
		out.push_back(getLiteral(Potassco::id(*it)));
	}
}
bool LogicProgram::extractCore(const LitVec& solverCore, Potassco::LitVec& prgLits) const 	{
	uint32 marked = 0;
	prgLits.clear();
	for (LitVec::const_iterator it = solverCore.begin(); it != solverCore.end(); ++it) {
		if (!ctx()->validVar(it->var())) { break; }
		ctx()->mark(*it);
		++marked;
	}
	if (marked == solverCore.size()) {
		for (VarVec::const_iterator it = frozen_.begin(), end = frozen_.end(); it != end && marked; ++it) {
			PrgAtom* atom = getRootAtom(*it);
			Literal lit = atom->assumption();
			if (lit == lit_true() || !ctx()->marked(lit)) continue;
			prgLits.push_back(atom->literal() == lit ? Potassco::lit(*it) : Potassco::neg(*it));
			ctx()->unmark(lit);
			--marked;
		}
		for (Potassco::LitVec::const_iterator it = assume_.begin(), end = assume_.end(); it != end && marked; ++it) {
			Literal lit = getLiteral(Potassco::id(*it));
			if (!ctx()->marked(lit)) continue;
			prgLits.push_back(*it);
			ctx()->unmark(lit);
			--marked;
		}
	}
	for (LitVec::const_iterator it = solverCore.begin(); it != solverCore.end(); ++it) {
		if (ctx()->validVar(it->var()))
			ctx()->unmark(it->var());
	}
	return prgLits.size() == solverCore.size();
}
/////////////////////////////////////////////////////////////////////////////////////////
// Program definition - private
/////////////////////////////////////////////////////////////////////////////////////////
void LogicProgram::addRule(const Rule& r, const SRule& meta) {
	if (Potassco::size(r.head) <= 1 && r.ht == Head_t::Disjunctive) {
		if      (Potassco::empty(r.head))        { addIntegrity(r, meta); return; }
		else if (r.normal() && r.cond.size == 0) { addFact(r.head); return; }
	}
	PrgBody* b = getBodyFor(r, meta);
	// only a non-false body can define atoms
	if (b->value() != value_false) {
		bool const disjunctive = Potassco::size(r.head) > 1 && r.ht == Head_t::Disjunctive;
		const EdgeType t = r.ht == Head_t::Disjunctive ? PrgEdge::Normal : PrgEdge::Choice;
		uint32 headHash = 0;
		bool ignoreScc  = opts_.noSCC || b->size() == 0;
		for (Potassco::AtomSpan::iterator it = Potassco::begin(r.head), end = Potassco::end(r.head); it != end; ++it) {
			PrgAtom* a = resize(*it);
			check_modular(isNew(*it) || a->frozen() || a->value() == value_false, *it);
			if (!disjunctive) {
				// Note: b->heads may now contain duplicates. They are removed in PrgBody::simplifyHeads.
				b->addHead(a, t);
				if (ignoreScc) { a->setIgnoreScc(ignoreScc); }
			}
			else {
				headHash += hashLit(posLit(*it));
				atomState_.addToHead(*it);
			}
		}
		if (disjunctive) {
			PrgDisj* d = getDisjFor(r.head, headHash);
			b->addHead(d, t);
		}
	}
}
void LogicProgram::addFact(const Potassco::AtomSpan& head) {
	PrgBody* tb = 0;
	for (Potassco::AtomSpan::iterator it = Potassco::begin(head), end = Potassco::end(head); it != end; ++it) {
		PrgAtom* a = resize(*it);
		check_modular(isNew(*it) || a->frozen() || a->value() == value_false, *it);
		if (*it != a->id() || atomState_.isFact(*it)) { continue; }
		a->setIgnoreScc(true);
		atomState_.set(*it, AtomState::fact_flag);
		if (!a->hasDep(PrgAtom::dep_all) && !a->frozen()) {
			if (!a->assignValue(value_true) || !a->propagateValue(*this, false)) {
				setConflict();
			}
			for (PrgAtom::sup_iterator bIt = a->supps_begin(), bEnd = a->supps_end(); bIt != bEnd; ++bIt) {
				if      (bIt->isBody()) { getBody(bIt->node())->markHeadsDirty(); }
				else if (bIt->isDisj()) { getDisj(bIt->node())->markDirty(); }
			}
			atoms_[*it] = &trueAtom_g;
			delete a;
		}
		else {
			if (!tb) tb = getTrueBody();
			tb->addHead(a, PrgEdge::Normal);
			assignValue(a, value_true, PrgEdge::newEdge(*tb, PrgEdge::Normal));
		}
	}
}
void LogicProgram::addIntegrity(const Rule& r, const SRule& meta) {
	if (r.sum() || r.cond.size != 1 || meta.bid != varMax) {
		PrgBody* B = getBodyFor(r, meta);
		if (!B->assignValue(value_false) || !B->propagateValue(*this, true)) {
			setConflict();
		}
	}
	else {
		PrgAtom* a = resize(Potassco::atom(r.cond[0]));
		ValueRep v = r.cond[0] > 0 ? value_false : value_weak_true;
		assignValue(a, v, PrgEdge::noEdge());
	}
}
bool LogicProgram::assignValue(PrgAtom* a, ValueRep v, PrgEdge reason) {
	if (a->eq()) { a = getRootAtom(a->id()); }
	ValueRep old = a->value();
	if (old == value_weak_true && v != value_weak_true) old = value_free;
	if (!a->assignValue(v)) { setConflict(); return false; }
	if (old == value_free)  { propQ_.push_back(a->id());  }
	if (v == value_false) {
		atomState_.set(a->id(), AtomState::false_flag);
	}
	else if (v == value_true && reason.isBody() && reason.isNormal() && getBody(reason.node())->bound() == 0) {
		atomState_.set(a->id(), AtomState::fact_flag);
	}
	return true;
}
bool LogicProgram::assignValue(PrgHead* h, ValueRep v, PrgEdge reason) {
	return !h->isAtom() || assignValue(static_cast<PrgAtom*>(h), v, reason);
}

bool LogicProgram::handleNatively(const Rule& r) const {
	ExtendedRuleMode m = opts_.erMode;
	if (m == mode_native || (r.normal() && r.ht == Head_t::Disjunctive)) {
		return true;
	}
	else if (m == mode_transform_integ || m == mode_transform_scc || m == mode_transform_nhcf) {
		return true;
	}
	else if (m == mode_transform) {
		return false;
	}
	else if (m == mode_transform_dynamic) {
		return r.normal() || transformNoAux(r) == false;
	}
	else if (m == mode_transform_choice) {
		return r.ht != Head_t::Choice;
	}
	else if (m == mode_transform_card)   {
		return r.bt != Body_t::Count;
	}
	else if (m == mode_transform_weight) {
		return r.normal();
	}
	assert(false && "unhandled extended rule mode");
	return true;
}

bool LogicProgram::transformNoAux(const Rule& r) const {
	return r.ht == Head_t::Disjunctive && r.sum() && (r.agg.bound == 1 || (Potassco::size(r.agg.lits) <= 6 && choose(toU32(Potassco::size(r.agg.lits)), r.agg.bound) <= 15));
}

void LogicProgram::transformExtended() {
	uint32 a = numAtoms();
	RuleTransform tm(*this);
	for (RuleList::size_type i = 0; i != extended_.size(); ++i) {
		Rule r = extended_[i]->rule();
		upStat(r.ht, -1);
		upStat(r.bt, -1);
		if (r.normal() || (r.ht == Head_t::Disjunctive && Potassco::size(r.head) < 2)) {
			tm.transform(r);
		}
		else {
			using Potassco::Lit_t;
			Atom_t aux = newAtom();
			Lit_t auxB = Potassco::lit(aux);
			Rule rAux1 = r; // aux :- body
			rAux1.ht   = Head_t::Disjunctive;
			rAux1.head = Potassco::toSpan(&aux, 1);
			Rule rAux2 = Rule::normal(r.ht, r.head, Potassco::toSpan(&auxB, 1));  // head :- auxB
			if (handleNatively(rAux1)) { addRule(rAux1); }
			else {
				RuleTransform::Strategy st = transformNoAux(rAux1) ? RuleTransform::strategy_no_aux : RuleTransform::strategy_default;
				tm.transform(rAux1, st);
			}
			if (handleNatively(rAux2)) { addRule(rAux2); }
			else                       { tm.transform(rAux2); }
		}
		delete extended_[i];
	}
	extended_.clear();
	incTrAux(numAtoms() - a);
}

void LogicProgram::transformIntegrity(uint32 nAtoms, uint32 maxAux) {
	if (stats.bodies[1][Body_t::Count] == 0) { return; }
	// find all constraint rules that are integrity constraints
	BodyList integrity;
	for (uint32 i = 0, end = static_cast<uint32>(bodies_.size()); i != end; ++i) {
		PrgBody* b = bodies_[i];
		if (b->relevant() && b->type() == Body_t::Count && b->value() == value_false) {
			integrity.push_back(b);
		}
	}
	if (!integrity.empty() && (integrity.size() == 1 || (nAtoms/double(bodies_.size()) > 0.5 && integrity.size() / double(bodies_.size()) < 0.01))) {
		uint32 aux = static_cast<uint32>(atoms_.size());
		RuleTransform tr(*this);
		RuleBuilder temp;
		// transform integrity constraints
		for (BodyList::size_type i = 0; i != integrity.size(); ++i) {
			PrgBody* b = integrity[i];
			uint32 est = b->bound()*( b->sumW()-b->bound() );
			if (est > maxAux) {
				// reached limit on aux atoms - stop transformation
				break;
			}
			if (b->toData(*this, temp) && temp.bodyType() != Body_t::Normal) {
				maxAux -= est;
				// transform rule
				setFrozen(false);
				upStat(Head_t::Disjunctive, -1);
				upStat(Body_t::Count, -1);
				tr.transform(Rule::sum(Head_t::Disjunctive, Potassco::toSpan<Potassco::Atom_t>(), temp.sum()));
				setFrozen(true);
				// propagate integrity condition to new rules
				propagate(true);
				b->markRemoved();
			}
			temp.clear();
		}
		// create vars for new atoms/bodies
		for (uint32 i = aux; i != atoms_.size(); ++i) {
			PrgAtom* a = atoms_[i];
			for (PrgAtom::sup_iterator it = a->supps_begin(); it != a->supps_end(); ++it) {
				PrgBody* nb = bodies_[it->node()];
				assert(nb->value() != value_false);
				nb->assignVar(*this);
			}
			a->assignVar(*this, a->supports() ? *a->supps_begin() : PrgEdge::noEdge());
		}
		incTrAux(static_cast<uint32>(atoms_.size()) - aux);
	}
}

void LogicProgram::prepareExternals() {
	if (auxData_->external.empty()) { return; }
	VarVec& external = auxData_->external;
	VarVec::iterator j = external.begin();
	for (VarVec::const_iterator it = j, end = external.end(); it != end; ++it) {
		Atom_t id = getRootId(decodeExternal(*it).first);
		const PrgAtom* atom = getAtom(id);
		if (!atomState_.inHead(id) && (atom->supports() == 0 || *atom->supps_begin() == PrgEdge::noEdge())) {
			Potassco::Value_t value = atom->supports() == 0 ? static_cast<Potassco::Value_t>(atom->freezeValue()) : Potassco::Value_t::Release;
			atomState_.addToHead(id);
			*j++ = encodeExternal(id, value);
		}
	}
	external.erase(j, external.end());
	for (VarVec::const_iterator it = external.begin(), end = external.end(); it != end; ++it) {
		atomState_.clearRule(decodeExternal(*it).first);
	}
}
void LogicProgram::updateFrozenAtoms() {
	if (frozen_.empty()) { return; }
	PrgBody* support   = 0;
	VarVec::iterator j = frozen_.begin();
	for (VarVec::const_iterator it = j, end = frozen_.end(); it != end; ++it) {
		Id_t id = getRootId(*it);
 		PrgAtom* a = getAtom(id);
		assert(a->frozen());
		a->resetId(id, false);
		if (a->supports() == 0) {
			assert(a->relevant());
			POTASSCO_REQUIRE(id < startAuxAtom(), "frozen atom shall be an input atom");
			if (!support) { support = getTrueBody(); }
			a->setIgnoreScc(true);
			support->addHead(a, PrgEdge::GammaChoice);
			*j++ = id; // still frozen
		}
		else {
			a->clearFrozen();
			if (*a->supps_begin() == PrgEdge::noEdge()) {
				// remove dummy edge added in unfreeze()
				a->removeSupport(PrgEdge::noEdge());
			}
			if (!isNew(id) && incData_) {
				// add to unfreeze so that we can later perform completion
				incData_->unfreeze.push_back(id);
			}
		}
	}
	frozen_.erase(j, frozen_.end());
}

void LogicProgram::prepareProgram(bool checkSccs) {
	assert(!frozen());
	prepareExternals();
	// Given that freezeTheory() might introduce otherwise
	// unused atoms, it must be called before we fix the
	// number of input atoms. It must also be called before resetting
	// the initial "upper" closure so that we can correctly classify
	// theory atoms.
	freezeTheory();
	// Prepare for preprocessing by resetting our "upper" closure.
	for (uint32 i = startAtom(); i != endAtom(); ++i) {
		getAtom(i)->setInUpper(false);
	}
	uint32 nAtoms = (input_.hi = std::min(input_.hi, endAtom()));
	stats.auxAtoms += endAtom() - nAtoms;
	for (uint32 i = 0; i != RuleStats::numKeys(); ++i) {
		stats.rules[1][i] += stats.rules[0][i];
	}
	for (uint32 i = 0; i != BodyStats::numKeys(); ++i) {
		stats.bodies[1][i] += stats.bodies[0][i];
	}
	statsId_ = 1;
	transformExtended();
	updateFrozenAtoms();
	PrgAtom* suppAtom = 0;
	if (opts_.suppMod) {
		VarVec h;
		suppAtom  = getAtom(newAtom());
		h.assign(1, suppAtom->id());
		addRule(Head_t::Choice, Potassco::toSpan(h), Potassco::toSpan<Potassco::Lit_t>());
		Potassco::Lit_t body = Potassco::lit(suppAtom->id());
		h.clear();
		for (Atom_t v = startAtom(), end = suppAtom->id(); v != end; ++v) {
			if (atoms_[v]->supports() != 0) { h.push_back(v); }
		}
		addRule(Head_t::Choice, Potassco::toSpan(h), Potassco::toSpan(&body, 1));
	}
	setFrozen(true);
	Preprocessor p;
	if (hasConflict() || !propagate(true) || !p.preprocess(*this, opts_.iters != 0 ? Preprocessor::full_eq : Preprocessor::no_eq, opts_.iters, opts_.dfOrder != 0)) {
		setConflict();
		return;
	}
	if (suppAtom && (!assignValue(suppAtom, value_false, PrgEdge::noEdge()) || !propagate(true))) {
		setConflict();
		return;
	}
	if (opts_.erMode == mode_transform_integ || opts_.erMode == mode_transform_dynamic) {
		nAtoms -= startAtom();
		transformIntegrity(nAtoms, std::min(uint32(15000), nAtoms*2));
	}
	addMinimize();
	uint32 sccs = 0;
	if (checkSccs) {
		uint32 startScc = incData_ ? incData_->startScc : 0;
		SccChecker c(*this, auxData_->scc, startScc);
		sccs       = c.sccs();
		stats.sccs = (sccs-startScc);
		if (incData_) { incData_->startScc = c.sccs(); }
		if (!disjunctions_.empty() || (opts_.erMode == mode_transform_scc && sccs)) {
			// reset node ids changed by scc checking
			for (uint32 i = 0; i != bodies_.size(); ++i) {
				if (getBody(i)->relevant()) { getBody(i)->resetId(i, true); }
			}
			for (uint32 i = 0; i != atoms_.size(); ++i) {
				if (getAtom(i)->relevant()) { getAtom(i)->resetId(i, true); }
			}
		}
	}
	else { stats.sccs = PrgNode::noScc; }
	finalizeDisjunctions(p, sccs);
	prepareComponents();
	prepareOutputTable();
	freezeAssumptions();
	if (incData_ && options().distTrue) {
		for (Var a = startAtom(), end = startAuxAtom(); a != end; ++a) {
			if (isSentinel(getRootAtom(a)->literal())) {
				Incremental::StepTrue t(end - 1, 0);
				if (!incData_->steps.empty()) { t.second = ctx()->addVar(Var_t::Atom, 0); }
				incData_->steps.push_back(t);
				break;
			}
		}
	}
	if (theory_) {
		TFilter f = { this };
		theory_->filter(f);
	}
	stats.atoms = static_cast<uint32>(atoms_.size()) - startAtom();
	bodyIndex_.clear();
	disjIndex_.clear();
}
void LogicProgram::freezeTheory() {
	if (theory_) {
		const IdSet& skippedHeads = auxData_->skippedHeads;
		for (TheoryData::atom_iterator it = theory_->currBegin(), end = theory_->end(); it != end; ++it) {
			const Potassco::TheoryAtom& a = **it;
			if (isFact(a.atom()) || !isNew(a.atom())) { continue; }
			PrgAtom* atom = resize(a.atom());
			bool inUpper  = atom->inUpper() || skippedHeads.count(a.atom()) != 0;
			if (!atom->frozen() && atom->supports() == 0 && atom->relevant() && !inUpper) {
				pushFrozen(atom, value_free);
			}
		}
	}
}
bool LogicProgram::TFilter::operator()(const Potassco::TheoryAtom& a) const {
	Atom_t id = a.atom();
	if (self->getLiteral(id) != lit_false() && self->getRootAtom(id)->value() != value_false) {
		self->ctx()->setFrozen(self->getLiteral(id).var(), true);
		return false;
	}
	PrgAtom* at = self->getRootAtom(id);
	return !at->frozen();
}
struct LogicProgram::DlpTr : public RuleTransform::ProgramAdapter {
	DlpTr(LogicProgram* x, EdgeType et) : self(x), type(et), scc(PrgNode::noScc) {}
	virtual Atom_t newAtom() {
		Atom_t x   = self->newAtom();
		PrgAtom* a = self->getAtom(x);
		a->setScc(scc);
		a->setSeen(true);
		atoms.push_back(x);
		if (scc != PrgNode::noScc) { self->auxData_->scc.push_back(a); }
		return x;
	}
	virtual void addRule(const Rule& r) {
		SRule meta;
		if (!self->simplifyRule(r, rule, meta)) { return; }
		bool gamma = type == PrgEdge::Gamma;
		Rule rs = rule.rule();
		PrgAtom* a = self->getAtom(rs.head[0]);
		PrgBody* B = self->assignBodyFor(rs, meta, type, gamma);
		if (B->value() != value_false && !B->hasHead(a, PrgEdge::Normal)) {
			B->addHead(a, type);
			self->stats.gammas += uint32(gamma);
		}
	}
	void assignAuxAtoms() {
		self->incTrAux(sizeVec(atoms));
		while (!atoms.empty()) {
			PrgAtom* ax = self->getAtom(atoms.back());
			atoms.pop_back();
			if (ax->supports()) {
				ax->setInUpper(true);
				ax->assignVar(*self, *ax->supps_begin());
			}
			else { self->assignValue(ax, value_false, PrgEdge::noEdge()); }
		}
	}
	LogicProgram* self;
	EdgeType      type;
	uint32        scc;
	VarVec        atoms;
	RuleBuilder   rule;
};

// replace disjunctions with gamma (shifted) and delta (component-shifted) rules
void LogicProgram::finalizeDisjunctions(Preprocessor& p, uint32 numSccs) {
	if (disjunctions_.empty()) { return; }
	VarVec head; BodyList supports;
	disjIndex_.clear();
	SccMap sccMap;
	sccMap.resize(numSccs, 0);
	enum SccFlag { seen_scc = 1u, is_scc_non_hcf = 128u };
	// replace disjunctions with shifted rules and non-hcf-disjunctions
	DisjList disj; disj.swap(disjunctions_);
	setFrozen(false);
	uint32 shifted = 0;
	stats.nonHcfs  = uint32(nonHcfs_.size());
	Literal bot    = lit_false();
	Potassco::LitVec rb;
	VarVec rh;
	DlpTr tr(this, PrgEdge::Gamma);

	// detach disjunctions
	for (uint32 id = 0, maxId = sizeVec(disj); id != maxId; ++id) {
		PrgDisj* d = disj[id];
		d->resetId(id, true);    // id changed during scc checking
		d->detach(*this, false); // remove from atoms and bodies but keep state
	}

	// replace disjunctions with shifted rules or new component-shifted disjunction
	for (uint32 id = 0, maxId = sizeVec(disj); id != maxId; ++id) {
		PrgDisj* d = disj[id];
		Literal dx = d->inUpper() ? d->literal() : bot;
		head.clear(); supports.clear();
		for (PrgDisj::atom_iterator it = d->begin(), end = d->end(); it != end; ++it) {
			uint32  aId = *it;
			PrgAtom* at = getAtom(aId);
			if (dx == bot) { continue; }
			if (at->eq())  {
				at = getAtom(aId = getRootId(aId));
			}
			if (isFact(at)){
				dx = bot;
				continue;
			}
			if (at->inUpper()) {
				head.push_back(aId);
				if (at->scc() != PrgNode::noScc){ sccMap[at->scc()] = seen_scc; }
			}
		}
		EdgeVec temp;
		d->clearSupports(temp);
		for (EdgeVec::iterator it = temp.begin(), end = temp.end(); it != end; ++it) {
			PrgBody* b = getBody(it->node());
			if (b->relevant() && b->value() != value_false) { supports.push_back(b); }
		}
		d->destroy();
		// create shortcut for supports to avoid duplications during shifting
		Literal supportLit = dx != bot ? getEqAtomLit(dx, supports, p, sccMap) : dx;
		// create shifted rules and split disjunctions into non-hcf components
		RuleTransform shifter(tr);
		for (VarVec::iterator hIt = head.begin(), hEnd = head.end(); hIt != hEnd; ++hIt) {
			uint32 scc = getAtom(*hIt)->scc();
			if (scc == PrgNode::noScc || (sccMap[scc] & seen_scc) != 0) {
				if (scc != PrgNode::noScc) { sccMap[scc] &= ~seen_scc; }
				else                       { scc = UINT32_MAX; }
				rh.assign(1, *hIt);
				rb.clear();
				if (supportLit.var() != 0) { rb.push_back(toInt(supportLit)); }
				else if (supportLit.sign()){ continue; }
				for (VarVec::iterator oIt = head.begin(); oIt != hEnd; ++oIt) {
					if (oIt != hIt) {
						if (getAtom(*oIt)->scc() == scc) { rh.push_back(*oIt); }
						else                             { rb.push_back(Potassco::neg(*oIt)); }
					}
				}
				SRule meta;
				if (!simplifyRule(Rule::normal(Head_t::Disjunctive, Potassco::toSpan(rh), Potassco::toSpan(rb)), rule_, meta)) {
					continue;
				}
				Rule sr = rule_.rule();
				PrgBody* B = assignBodyFor(sr, meta, PrgEdge::Normal, true);
				if (B->value() != value_false && Potassco::size(sr.head) == 1) {
					++shifted;
					B->addHead(getAtom(sr.head[0]), PrgEdge::Normal);
				}
				else if (B->value() != value_false && Potassco::size(sr.head) > 1) {
					PrgDisj* x = getDisjFor(sr.head, 0);
					B->addHead(x, PrgEdge::Normal);
					x->assignVar(*this, *x->supps_begin());
					x->setInUpper(true);
					x->setSeen(true);
					if ((sccMap[scc] & is_scc_non_hcf) == 0) {
						sccMap[scc] |= is_scc_non_hcf;
						nonHcfs_.add(scc);
					}
					if (!options().noGamma) {
						if (Potassco::size(sr.cond) >= 4) {
							// make body eq to a new aux atom
							tr.scc = B->scc(*this) == scc ? scc : PrgNode::noScc;
							Atom_t eqAtom = tr.newAtom();
							B->addHead(getAtom(eqAtom), PrgEdge::Normal);
							rb.assign(1, Potassco::lit(eqAtom));
							sr.cond = Potassco::toSpan(rb);
							tr.assignAuxAtoms();
							tr.scc = PrgNode::noScc;
						}
						shifter.transform(sr, RuleTransform::strategy_no_aux);
					}
					else {
						// only add support edge
						for (PrgDisj::atom_iterator a = x->begin(), end = x->end(); a != end; ++a) {
							B->addHead(getAtom(*a), PrgEdge::GammaChoice);
						}
					}
				}
			}
		}
	}
	assert(tr.atoms.empty());
	if (!disjunctions_.empty() && nonHcfs_.config == 0) {
		nonHcfs_.config = ctx()->configuration()->config("tester");
	}
	upStat(RK(Normal), shifted);
	stats.nonHcfs = uint32(nonHcfs_.size()) - stats.nonHcfs;
	rh.clear();
	setFrozen(true);
}
// optionally transform extended rules in sccs
void LogicProgram::prepareComponents() {
	int trRec = opts_.erMode == mode_transform_scc;
	// HACK: force transformation of extended rules in non-hcf components
	// REMOVE this once minimality check supports aggregates
	if (!disjunctions_.empty() && trRec != 1) {
		trRec = 2;
	}
	if (trRec != 0) {
		DlpTr tr(this, PrgEdge::Normal);
		RuleTransform trans(tr);
		RuleBuilder temp;
		setFrozen(false);
		EdgeVec heads;
		// find recursive aggregates
		for (uint32 bIdx = 0, bEnd = numBodies(); bIdx != bEnd; ++bIdx) {
			PrgBody* B = bodies_[bIdx];
			if (B->type() == Body_t::Normal || !B->hasVar() || B->value() == value_false) { continue; } // not aggregate or not relevant
			tr.scc = B->scc(*this);
			if (tr.scc == PrgNode::noScc || (trRec == 2 && !nonHcfs_.find(tr.scc))) { continue; } // not recursive
			// transform all rules a :- B, where scc(a) == scc(B):
			heads.clear();
			for (PrgBody::head_iterator hIt = B->heads_begin(), hEnd = B->heads_end(); hIt != hEnd; ++hIt) {
				assert(hIt->isAtom());
				if (getAtom(hIt->node())->scc() == tr.scc) { heads.push_back(*hIt); }
			}
			if (heads.empty()) { continue; }
			using Potassco::Lit_t;
			Head_t ht = !isChoice(heads[0].type()) ? Head_t::Disjunctive : Head_t::Choice;
			Atom_t  h = heads[0].node();
			Lit_t aux = 0;
			if (heads.size() > 1) { // more than one head, make body eq to some new aux atom
				ht  = Head_t::Disjunctive;
				h   = tr.newAtom();
				aux = Potassco::lit(h);
			}
			temp.clear();
			if (!B->toData(*this, temp) || temp.bodyType() == Body_t::Normal) {
				B->simplify(*this, true, 0);
				continue;
			}
			trans.transform(Rule::sum(ht, Potassco::toSpan(&h, 1), temp.sum()));
			for (EdgeVec::const_iterator hIt = heads.begin(); hIt != heads.end(); ++hIt) {
				B->removeHead(getAtom(hIt->node()), hIt->type());
				if (h != hIt->node()) {
					ht = !isChoice(hIt->type()) ? Head_t::Disjunctive : Head_t::Choice;
					h  = hIt->node();
					tr.addRule(Rule::normal(ht, Potassco::toSpan(&h, 1), Potassco::toSpan(&aux, 1)));
				}
			}
		}
		tr.assignAuxAtoms();
		setFrozen(true);
	}
}
void LogicProgram::prepareOutputTable() {
	OutputTable& out = ctx()->output;
	// add new output predicates in program order to output table
	std::stable_sort(show_.begin(), show_.end(), compose22(std::less<Id_t>(), select1st<ShowPair>(), select1st<ShowPair>()));
	for (ShowVec::iterator it = show_.begin(), end = show_.end(); it != end; ++it) {
		Literal lit = getLiteral(it->first);
		bool isAtom = it->first < startAuxAtom();
		if      (!isSentinel(lit))  { out.add(it->second, lit, it->first); if (isAtom) ctx()->setOutput(lit.var(), true); }
		else if (lit == lit_true()) { out.add(it->second); }
	}
	if (!auxData_->project.empty()) {
		for (VarVec::const_iterator it = auxData_->project.begin(), end = auxData_->project.end(); it != end; ++it) {
			out.addProject(getLiteral(*it));
		}
	}
}

// Make assumptions/externals exempt from sat-preprocessing
void LogicProgram::freezeAssumptions() {
	for (VarVec::const_iterator it = frozen_.begin(), end = frozen_.end(); it != end; ++it) {
		ctx()->setFrozen(getRootAtom(*it)->var(), true);
	}
	for (Potassco::LitVec::const_iterator it = assume_.begin(), end = assume_.end(); it != end; ++it) {
		ctx()->setFrozen(getLiteral(Potassco::id(*it)).var(), true);
	}
}

// add (completion) nogoods
bool LogicProgram::addConstraints() {
	ClauseCreator gc(ctx()->master());
	if (options().iters == 0) {
		gc.addDefaultFlags(ClauseCreator::clause_force_simplify);
	}
	ctx()->startAddConstraints();
	// handle initial conflict, if any
	if (!ctx()->ok() || !ctx()->addUnary(getTrueAtom()->trueLit())) {
		return false;
	}
	if (incData_ && !incData_->steps.empty() && !ctx()->addUnary(posLit(incData_->steps.back().second))) {
		return false;
	}
	if (options().noGamma && !disjunctions_.empty()) {
		// add "rule" nogoods for disjunctions
		for (DisjList::const_iterator it = disjunctions_.begin(); it != disjunctions_.end(); ++it) {
			gc.start().add(~(*it)->literal());
			for (PrgDisj::atom_iterator a = (*it)->begin(); a != (*it)->end(); ++a) {
				gc.add(getAtom(*a)->literal());
			}
			if (!gc.end()) { return false; }
		}
	}
	// add bodies from this step
	for (BodyList::const_iterator it = bodies_.begin(); it != bodies_.end(); ++it) {
		if (!toConstraint((*it), *this, gc)) { return false; }
	}
	// add atoms thawed in this step
	for (VarIter it = unfreeze_begin(), end = unfreeze_end(); it != end; ++it) {
		if (!toConstraint(getAtom(*it), *this, gc)) { return false; }
	}
	// add atoms from this step
	const bool freezeAll = incData_ != 0;
	const uint32 hiAtom  = startAuxAtom();
	uint32 id = startAtom();
	for (AtomList::const_iterator it = atoms_.begin()+startAtom(), end = atoms_.end(); it != end; ++it, ++id) {
		if (!toConstraint(*it, *this, gc)) { return false; }
		if (id < hiAtom && (*it)->hasVar()){
			if (freezeAll) { ctx()->setFrozen((*it)->var(), true); }
			ctx()->setInput((*it)->var(), true);
		}
	}
	if (!auxData_->scc.empty()) {
		if (ctx()->sccGraph.get() == 0) {
			ctx()->sccGraph = new PrgDepGraph(static_cast<PrgDepGraph::NonHcfMapType>(opts_.oldMap == 0));
		}
		uint32 oldNodes = ctx()->sccGraph->nodes();
		ctx()->sccGraph->addSccs(*this, auxData_->scc, nonHcfs_);
		stats.ufsNodes  = ctx()->sccGraph->nodes()-oldNodes;
	}
	return true;
}
void LogicProgram::addDomRules() {
	if (auxData_->dom.empty()) { return; }
	VarVec domVec;
	EqVec eqVec;
	DomRules&  doms = auxData_->dom;
	Solver const& s = *ctx()->master();
	// mark any previous domain atoms so that we can decide
	// whether existing variables can be used for the atoms in doms
	if (incData_) {
		domVec.swap(incData_->doms);
		for (VarVec::const_iterator it = domVec.begin(); it != domVec.end(); ++it) {
			if (s.value(*it) == value_free) { ctx()->mark(posLit(*it)); }
		}
	}
	DomRules::iterator j;
	IndexMap::const_iterator eq;
	DomRule r;
	for (DomRules::iterator it = (j = doms.begin()), end = doms.end(); it != end; ++it) {
		Literal cond = getLiteral(it->cond);
		Literal slit = getLiteral(it->atom);
		Var     svar = slit.var();
		if (s.isFalse(cond) || s.value(svar) != value_free) { continue; }
		if (s.isTrue(cond)) { it->cond = 0; cond = lit_true(); }
		// check if atom is the root for its var
		if (!atomState_.isSet(it->atom, AtomState::dom_flag)) {
			if (!ctx()->marked(posLit(svar))) {
				// var(it->atom) is not yet used - make it->atom its root
				ctx()->mark(posLit(svar));
				atomState_.set(it->atom, AtomState::dom_flag);
				domVec.push_back(svar);
			}
			else if ((eq = domEqIndex_.find(it->atom)) != domEqIndex_.end()) {
				// var(it->atom) is used but we already created a new var for it->atom
				slit = posLit(svar = eq->second);
			}
			else {
				// var(it->atom) is used - introduce new aux var and make it eq to lit(atom)
				Eq n = { ctx()->addVar(Var_t::Atom, VarInfo::Nant), slit };
				eqVec.push_back(n);
				svar = n.var;
				slit = posLit(svar);
				domEqIndex_.insert(IndexMap::value_type(static_cast<uint32>(it->atom), svar));
			}
		}
		*j++ = (r = *it);
		if (slit.sign()) {
			if      (r.type == DomModType::Sign)  { r.bias = r.bias != 0 ? -r.bias : 0; }
			else if (r.type == DomModType::True)  { r.type = DomModType::False; }
			else if (r.type == DomModType::False) { r.type = DomModType::True; }
		}
		ctx()->heuristic.add(svar, static_cast<DomModType>(r.type), r.bias, r.prio, cond);
	}
	if (j != doms.end()) {
		upStat(RK(Heuristic), -static_cast<int>(doms.end() - j));
		doms.erase(j, doms.end());
	}
	// cleanup var flags
	for (VarVec::const_iterator it = domVec.begin(); it != domVec.end(); ++it) {
		ctx()->unmark(*it);
	}
	if (incData_) {
		incData_->doms.swap(domVec);
	}
	if (!eqVec.empty()) {
		ctx()->startAddConstraints();
		for (EqVec::const_iterator it = eqVec.begin(), end = eqVec.end(); it != end; ++it) {
			// it->var == it->lit
			ctx()->addBinary(~it->lit, posLit(it->var));
			ctx()->addBinary( it->lit, negLit(it->var));
		}
	}
}

void LogicProgram::addAcycConstraint() {
	AcycRules& acyc = auxData_->acyc;
	if (acyc.empty()) { return; }
	SharedContext& ctx = *this->ctx();
	ExtDepGraph* graph = ctx.extGraph.get();
	const Solver&    s = *ctx.master();
	if (graph) { graph->update(); }
	else       { ctx.extGraph = (graph = new ExtDepGraph()); }
	for (AcycRules::const_iterator it = acyc.begin(), end = acyc.end(); it != end; ++it) {
		Literal lit = getLiteral(it->cond);
		if (!s.isFalse(lit)) {
			graph->addEdge(lit, it->node[0], it->node[1]);
		}
		else {
			upStat(RK(Acyc), -1);
		}
	}
	if (graph->finalize(ctx) == 0) { ctx.extGraph = 0; }
}
#undef check_modular
/////////////////////////////////////////////////////////////////////////////////////////
// misc/helper functions
/////////////////////////////////////////////////////////////////////////////////////////
PrgAtom* LogicProgram::resize(Atom_t atomId) {
	while (atoms_.size() <= AtomList::size_type(atomId)) {
		newAtom();
	}
	return getRootAtom(atomId);
}

bool LogicProgram::propagate(bool backprop) {
	assert(frozen());
	bool oldB = opts_.backprop != 0;
	opts_.backprop = backprop;
	for (VarVec::size_type i = 0; i != propQ_.size(); ++i) {
		PrgAtom* a = getAtom(propQ_[i]);
		if (!a->relevant()) { continue; }
		if (!a->propagateValue(*this, backprop)) {
			setConflict();
			return false;
		}
		if (a->hasVar() && a->id() < startAtom() && !ctx()->addUnary(a->trueLit())) {
			setConflict();
			return false;
		}
	}
	opts_.backprop = oldB;
	propQ_.clear();
	return true;
}
ValueRep LogicProgram::litVal(const PrgAtom* a, bool pos) const {
	if (a->value() != value_free || !a->relevant()) {
		bool vSign = a->value() == value_false || !a->relevant();
		if  (vSign == pos) { return value_false; }
		return a->value() != value_weak_true ? value_true : value_free;
	}
	return value_free;
}

// Simplifies the given normal rule H :- l1, ..., ln
//  - removes true and duplicate literals from body: {T,a,b,a} -> {a, b}.
//  - checks for contradictions and false literals in body: {a, not a} -> F
//  - checks for satisfied head and removes false atoms from head
// POST: if true out contains the simplified normal rule.
bool LogicProgram::simplifyNormal(Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body, RuleBuilder& out, SRule& meta) {
	out.clear();
	out.startBody();
	meta = SRule();
	bool ok = true;
	for (Potassco::LitSpan::iterator it = Potassco::begin(body), end = Potassco::end(body); it != end; ++it) {
		POTASSCO_CHECK(Potassco::atom(*it) < bodyId, EOVERFLOW, "Atom out of bounds");
		PrgAtom* a = resize(Potassco::atom(*it));
		Literal  p = Literal(a->id(), *it < 0);// replace any eq atoms
		ValueRep v = litVal(a, !p.sign());
		if (v == value_false || atomState_.inBody(~p)) {
			ok = false;
			break;
		}
		else if (v != value_true  && !atomState_.inBody(p)) {
			atomState_.addToBody(p);
			out.addGoal(toInt(p));
			meta.pos  += !p.sign();
			meta.hash += hashLit(p);
		}
	}
	uint32_t bs = toU32(size(out.body()));
	meta.bid = ok ? findBody(meta.hash, Body_t::Normal, bs) : varMax;
	ok = ok && pushHead(ht, head, 0, out);
	for (const Potassco::Lit_t* it = out.lits_begin(); bs--;) {
		atomState_.clearRule(Potassco::atom(*it++));
	}
	return ok;
}

struct IsLit {
	IsLit(Potassco::Lit_t x) : lhs(x) {}
	template <class P>
	bool operator()(const P& rhs) const { return lhs == Potassco::lit(rhs); }
	Potassco::Lit_t lhs;
};

// Simplifies the given sum rule: H :- lb { l1 = w1 ... ln = wn }.
//  - removes assigned literals and updates lb accordingly
//  - removes literals li with weight wi = 0
//  - reduces weights wi > bound() to bound
//  - merges duplicate literals in sum, i.e. lb {a=w1, b=w2, a=w3} -> lb {a=w1+w3, b=w2}
//  - checks for contradiction, i.e. sum contains both p and not p and both are needed
//  - replaces sum with count if all weights are equal
//  - replaces sum with normal body if all literals must be true for the sum to be satisfied
// POST: if true out contains the simplified rule.
bool LogicProgram::simplifySum(Head_t ht, const Potassco::AtomSpan& head, const Potassco::Sum_t& body, RuleBuilder& out, SRule& meta) {
	meta = SRule();
	weight_t bound = body.bound, maxW = 1, minW = CLASP_WEIGHT_T_MAX, sumW = 0, dirty = 0;
	out.clear();
	out.startSum(bound);
	for (Potassco::WeightLitSpan::iterator it = Potassco::begin(body.lits), end = Potassco::end(body.lits); it != end && bound > 0; ++it) {
		POTASSCO_CHECK(it->weight >= 0, EDOM, "Non-negative weight expected!");
		POTASSCO_CHECK(Potassco::atom(*it) < bodyId, EOVERFLOW, "Atom out of bounds");
		if (it->weight == 0) continue; // skip irrelevant lits
		PrgAtom* a = resize(Potassco::atom(*it));
		Literal  p = Literal(a->id(), Potassco::lit(*it) < 0);// replace any eq atoms
		ValueRep v = litVal(a, !p.sign());
		weight_t w = Potassco::weight(*it);
		if (v == value_true) { bound -= w; }
		else if (v != value_false) {
			POTASSCO_CHECK((CLASP_WEIGHT_T_MAX-sumW)>= w, EOVERFLOW, "Integer overflow!");
			sumW += w;
			if (!atomState_.inBody(p)) {
				atomState_.addToBody(p);
				out.addGoal(toInt(p), w);
				meta.pos += !p.sign();
				meta.hash += hashLit(p);
			}
			else { // Merge duplicate lits
				Potassco::WeightLit_t* pos = std::find_if(out.wlits_begin(), out.wlits_end(), IsLit(toInt(p)));
				POTASSCO_ASSERT(pos != out.wlits_end());
				w = (pos->weight += w);
				++dirty;
			}
			if (w > maxW) { maxW = w; }
			if (w < minW) { minW = w; }
			dirty += static_cast<weight_t>(atomState_.inBody(~p));
		}
	}
	weight_t sumR = sumW;
	if (bound > 0 && (dirty || maxW > bound)) {
		sumR = 0, minW = CLASP_WEIGHT_T_MAX;
		for (Potassco::WeightLit_t* it = out.wlits_begin(), *end = out.wlits_end(); it != end; ++it) {
			Literal  p = toLit(it->lit);
			weight_t w = it->weight;
			if (w > bound) { sumW -= (w - bound); it->weight = (maxW = w = bound); }
			if (w < minW) { minW = w; }
			sumR += w;
			if (p.sign() && atomState_.inBody(~p)) {
				// body contains p and ~p: we can achieve at most max(weight(p), weight(~p))
				sumR -= std::min(w, std::find_if(out.wlits_begin(), end, IsLit(Potassco::neg(it->lit)))->weight);
			}
		}
	}
	out.setBound(bound);
	if (bound <= 0 || sumR < bound) {
		for (const Potassco::WeightLit_t* it = out.wlits_begin(), *end = out.wlits_end(); it != end; ++it) { atomState_.clearRule(Potassco::atom(*it)); }
		return bound <= 0 && simplifyNormal(ht, head, Potassco::toSpan<Potassco::Lit_t>(), out, meta);
	}
	else if ((sumW - minW) < bound) {
		out.weaken(Body_t::Normal);
		meta.bid = findBody(meta.hash, Body_t::Normal, toU32(size(out.body())));
		bool ok = pushHead(ht, head, 0, out);
		for (const Potassco::Lit_t* it = out.lits_begin(), *end = out.lits_end(); it != end; ++it) {
			atomState_.clearRule(Potassco::atom(*it));
		}
		return ok;
	}
	else if (minW == maxW) {
		out.weaken(Body_t::Count, maxW != 1);
		bound = out.bound();
	}
	meta.bid = findBody(meta.hash, out.bodyType(), (uint32_t)std::distance(out.wlits_begin(), out.wlits_end()), out.bound(), out.wlits_begin());
	bool ok  = pushHead(ht, head, sumW - out.bound(), out);
	for (const Potassco::WeightLit_t* it = out.wlits_begin(), *end = out.wlits_end(); it != end; ++it) {
		atomState_.clearRule(Potassco::atom(*it));
	}
	return ok;
}

// Pushes the given rule head to the body given in out.
// Pre: Body literals are marked and lits is != 0 if body is a sum.
bool LogicProgram::pushHead(Head_t ht, const Potassco::AtomSpan& head, weight_t slack, RuleBuilder& out) {
	const uint8 ignoreMask = AtomState::false_flag|AtomState::head_flag;
	uint32 hs = 0;
	bool sat = false, sum = out.bodyType() == Body_t::Sum;
	out.start(ht);
	for (Potassco::AtomSpan::iterator it = Potassco::begin(head), end = Potassco::end(head); it != end; ++it) {
		if (!atomState_.isSet(*it, AtomState::simp_mask)) {
			out.addHead(*it);
			atomState_.addToHead(*it);
			++hs;
		}
		else if (!atomState_.isSet(*it, ignoreMask)) { // h occurs in B+ and/or B- or is true
			weight_t wp = weight_t(atomState_.inBody(posLit(*it))), wn = weight_t(atomState_.inBody(negLit(*it)));
			if (wp && sum) { wp = std::find_if(out.wlits_begin(), out.wlits_end(), IsLit(Potassco::lit(*it)))->weight; }
			if (wn && sum) { wn = std::find_if(out.wlits_begin(), out.wlits_end(), IsLit(Potassco::neg(*it)))->weight; }
			if (atomState_.isFact(*it) || wp > slack) { sat = true; }
			else if (wn <= slack) {
				out.addHead(*it);
				atomState_.addToHead(*it);
				++hs;
			}
		}
	}
	for (const Atom_t* it = out.head_begin(), *end = it + hs; it != end; ++it) {
		atomState_.clearRule(*it);
	}
	return !sat || (ht == Head_t::Choice && hs);
}

bool LogicProgram::simplifyRule(const Rule& r, Potassco::RuleBuilder& out, SRule& meta) {
	return r.normal()
		? simplifyNormal(r.ht, r.head, r.cond, out, meta)
		: simplifySum(r.ht, r.head, r.agg, out, meta);
}
// create new atom aux representing supports, i.e.
// aux == S1 v ... v Sn
Literal LogicProgram::getEqAtomLit(Literal lit, const BodyList& supports, Preprocessor& p, const SccMap& sccMap) {
	if (supports.empty() || lit == lit_false()) {
		return lit_false();
	}
	else if (supports.size() == 1 && supports[0]->size() < 2 && supports.back()->literal() == lit) {
		return supports[0]->size() == 0 ? lit_true() : supports[0]->goal(0);
	}
	else if (p.getRootAtom(lit) != varMax && opts_.noSCC) {
		// Use existing root atom only if scc checking is disabled.
		// Otherwise, we would have to recheck SCCs from that atom again because
		// adding a new edge could create a new or change an existing SCC.
		return posLit(p.getRootAtom(lit));
	}
	incTrAux(1);
	Atom_t auxV  = newAtom();
	PrgAtom* aux = getAtom(auxV);
	uint32 scc   = PrgNode::noScc;
	aux->setLiteral(lit);
	aux->setSeen(true);
	if (p.getRootAtom(lit) == varMax)
		p.setRootAtom(aux->literal(), auxV);
	for (BodyList::const_iterator sIt = supports.begin(); sIt != supports.end(); ++sIt) {
		PrgBody* b = *sIt;
		if (b->relevant() && b->value() != value_false) {
			for (uint32 g = 0; scc == PrgNode::noScc && g != b->size() && !b->goal(g).sign(); ++g) {
				uint32 aScc = getAtom(b->goal(g).var())->scc();
				if (aScc != PrgNode::noScc && (sccMap[aScc] & 1u)) { scc = aScc; }
			}
			b->addHead(aux, PrgEdge::Normal);
			if (b->value() != value_free && !assignValue(aux, b->value(), PrgEdge::newEdge(*b, PrgEdge::Normal))) {
				break;
			}
			aux->setInUpper(true);
		}
	}
	if (!aux->inUpper()) {
		aux->setValue(value_false);
		return lit_false();
	}
	else if (scc != PrgNode::noScc) {
		aux->setScc(scc);
		auxData_->scc.push_back(aux);
	}
	return posLit(auxV);
}

PrgBody* LogicProgram::getBodyFor(const Rule& r, const SRule& meta, bool addDeps) {
	if (meta.bid < bodies_.size()) {
		return getBody(meta.bid);
	}
	// no corresponding body exists, create a new object
	PrgBody* b = PrgBody::create(*this, numBodies(), r, meta.pos, addDeps);
	bodyIndex_.insert(IndexMap::value_type(meta.hash, b->id()));
	bodies_.push_back(b);
	if (b->isSupported()) {
		initialSupp_.push_back(b->id());
	}
	upStat(r.bt);
	return b;
}
PrgBody* LogicProgram::getTrueBody() {
	uint32 id = findBody(0, Body_t::Normal, 0);
	if (id < bodies_.size()) {
		return getBody(id);
	}
	return getBodyFor(Rule::normal(Head_t::Choice, Potassco::toSpan<Atom_t>(), Potassco::toSpan<Potassco::Lit_t>()), SRule());
}
PrgBody* LogicProgram::assignBodyFor(const Rule& r, const SRule& meta, EdgeType depEdge, bool simpStrong) {
	PrgBody* b = getBodyFor(r, meta, depEdge != PrgEdge::Gamma);
	if (!b->hasVar() && !b->seen()) {
		uint32 eqId;
		b->markDirty();
		b->simplify(*this, simpStrong, &eqId);
		if (eqId != b->id()) {
			assert(b->id() == bodies_.size()-1);
			removeBody(b, meta.hash);
			bodies_.pop_back();
			if (depEdge != PrgEdge::Gamma) {
				for (uint32 i = 0; i != b->size(); ++i) {
					getAtom(b->goal(i).var())->removeDep(b->id(), !b->goal(i).sign());
				}
			}
			b->destroy();
			b = bodies_[eqId];
		}
	}
	b->setSeen(true);
	b->assignVar(*this);
	return b;
}

bool LogicProgram::equalLits(const PrgBody& b, const WeightLitSpan& lits) const {
	WeightLitSpan::iterator lBeg = Potassco::begin(lits), lEnd = Potassco::end(lits);
	for (uint32 i = 0, end = b.size(); i != end; ++i) {
		Potassco::WeightLit_t wl = { toInt(b.goal(i)), b.weight(i) };
		if (!std::binary_search(lBeg, lEnd, wl)) { return false; }
	}
	return true;
}

// Pre: all literals in body are marked.
uint32 LogicProgram::findBody(uint32 hash, Body_t type, uint32 size, weight_t bound, Potassco::WeightLit_t* sum) {
	IndexRange bodies = bodyIndex_.equal_range(hash);
	bool sorted = false;
	if (type == Body_t::Normal) { bound = static_cast<weight_t>(size); }
	for (IndexIter it = bodies.first; it != bodies.second; ++it) {
		const PrgBody& b = *getBody(it->second);
		if (!checkBody(b, type, size, bound) || !atomState_.inBody(b.goals_begin(), b.goals_end())) {
			continue;
		}
		else if (!b.hasWeights()) {
			return b.id();
		}
		else if (sum) {
			if (!sorted) {
				std::sort(sum, sum + size);
				sorted = true;
			}
			if (equalLits(b, Potassco::toSpan(sum, size))) { return b.id(); }
		}
	}
	return varMax;
}

uint32 LogicProgram::findEqBody(const PrgBody* b, uint32 hash) {
	IndexRange bodies = bodyIndex_.equal_range(hash);
	if (bodies.first == bodies.second)  { return varMax;  }
	uint32 eqId = varMax, n = 0, r = 0;
	for (IndexIter it = bodies.first; it != bodies.second && eqId == varMax; ++it) {
		const PrgBody& rhs = *getBody(it->second);
		if (!checkBody(rhs, b->type(), b->size(), b->bound())) { continue; }
		else if (b->size() == 0)  { eqId = rhs.id(); }
		else if (b->size() == 1)  { eqId = b->goal(0) == rhs.goal(0) && b->weight(0) == rhs.weight(0) ? rhs.id() : varMax; }
		else {
			if (++n == 1) { std::for_each(b->goals_begin(), b->goals_end(), std::bind1st(std::mem_fun(&AtomState::addToBody), &atomState_)); }
			if      (!atomState_.inBody(rhs.goals_begin(), rhs.goals_end())) { continue; }
			else if (!b->hasWeights()) { eqId = rhs.id(); }
			else {
				if (n == 1 || r == 0) {
					rule_.clear();
					if (!b->toData(*this, rule_) || rule_.bodyType() != Body_t::Sum) {
						rule_.clear();
						continue;
					}
					r = 1;
					std::sort(rule_.wlits_begin(), rule_.wlits_end());
				}
				if (equalLits(rhs, rule_.sum().lits)) { eqId = rhs.id(); }
			}
		}
	}
	if (n) {
		rule_.clear();
		std::for_each(b->goals_begin(), b->goals_end(), std::bind1st(std::mem_fun(&AtomState::clearBody), &atomState_));
	}
	return eqId;
}

PrgDisj* LogicProgram::getDisjFor(const Potassco::AtomSpan& head, uint32 headHash) {
	PrgDisj* d = 0;
	if (headHash) {
		LogicProgram::IndexRange eqRange = disjIndex_.equal_range(headHash);
		for (; eqRange.first != eqRange.second; ++eqRange.first) {
			PrgDisj& o = *disjunctions_[eqRange.first->second];
			if (o.relevant() && o.size() == Potassco::size(head) && atomState_.allMarked(o.begin(), o.end(), AtomState::head_flag)) {
				assert(o.id() == eqRange.first->second);
				d = &o;
				break;
			}
		}
		for (Potassco::AtomSpan::iterator it = Potassco::begin(head), end = Potassco::end(head); it != end; ++it) {
			atomState_.clearRule(*it);
		}
	}
	if (!d) {
		// no corresponding disjunction exists, create a new object
		// and link it to all atoms
		++stats.disjunctions[statsId_];
		d = PrgDisj::create((uint32)disjunctions_.size(), head);
		disjunctions_.push_back(d);
		PrgEdge edge = PrgEdge::newEdge(*d, PrgEdge::Choice);
		for (Potassco::AtomSpan::iterator it = Potassco::begin(head), end = Potassco::end(head); it != end; ++it) {
			getAtom(*it)->addSupport(edge);
		}
		if (headHash) {
			disjIndex_.insert(IndexMap::value_type(headHash, d->id()));
		}
	}
	return d;
}

// body has changed - update index
uint32 LogicProgram::update(PrgBody* body, uint32 oldHash, uint32 newHash) {
	uint32 id   = removeBody(body, oldHash);
	if (body->relevant()) {
		uint32 eqId = findEqBody(body, newHash);
		if (eqId == varMax) {
			// No equivalent body found.
			// Add new entry to index
			bodyIndex_.insert(IndexMap::value_type(newHash, id));
		}
		return eqId;
	}
	return varMax;
}

// body b has changed - remove old entry from body node index
uint32 LogicProgram::removeBody(PrgBody* b, uint32 hash) {
	IndexRange ra = bodyIndex_.equal_range(hash);
	uint32 id     = b->id();
	for (; ra.first != ra.second; ++ra.first) {
		if (bodies_[ra.first->second] == b) {
			id = ra.first->second;
			bodyIndex_.erase(ra.first);
			break;
		}
	}
	return id;
}

PrgAtom* LogicProgram::mergeEqAtoms(PrgAtom* a, Id_t rootId) {
	PrgAtom* root = getAtom(rootId = getRootId(rootId));
	ValueRep mv   = getMergeValue(a, root);
	assert(!a->eq() && !root->eq() && !a->frozen());
	if (a->ignoreScc()) { root->setIgnoreScc(true); }
	if (mv != a->value()    && !assignValue(a, mv, PrgEdge::noEdge()))   { return 0; }
	if (mv != root->value() && !assignValue(root, mv, PrgEdge::noEdge())){ return 0; }
	a->setEq(rootId);
	incEqs(Var_t::Atom);
	return root;
}

// returns whether posSize(root) <= posSize(body)
bool LogicProgram::positiveLoopSafe(PrgBody* body, PrgBody* root) const {
	uint32 i = 0, end = std::min(body->size(), root->size());
	while (i != end && body->goal(i).sign() == root->goal(i).sign()) { ++i; }
	return i == root->size() || root->goal(i).sign();
}

PrgBody* LogicProgram::mergeEqBodies(PrgBody* b, Id_t rootId, bool hashEq, bool atomsAssigned) {
	PrgBody* root = getBody(rootId = getEqNode(bodies_, rootId));
	bool     bp   = options().backprop != 0;
	if (b == root) { return root; }
	assert(!b->eq() && !root->eq() && (hashEq || b->literal() == root->literal()));
	if (!b->simplifyHeads(*this, atomsAssigned) || (b->value() != root->value() && (!mergeValue(b, root) || !root->propagateValue(*this, bp) || !b->propagateValue(*this, bp)))) {
		setConflict();
		return 0;
	}
	if (hashEq || positiveLoopSafe(b, root)) {
		b->setLiteral(root->literal());
		if (!root->mergeHeads(*this, *b, atomsAssigned, !hashEq)) {
			setConflict();
			return 0;
		}
		incEqs(Var_t::Body);
		b->setEq(rootId);
		return root;
	}
	return b;
}

const char* LogicProgram::findName(Atom_t x) const {
	for (OutputTable::pred_iterator it = ctx()->output.pred_begin(), end = ctx()->output.pred_end(); it != end; ++it) {
		if (it->user == x) { return it->name; }
	}
	for (ShowVec::const_iterator it = show_.begin(), end = show_.end(); it != end; ++it) {
		if (it->first == x){ return it->second; }
	}
	return "";
}
VarVec& LogicProgram::getSupportedBodies(bool sorted) {
	if (sorted) {
		std::stable_sort(initialSupp_.begin(), initialSupp_.end(), LessBodySize(bodies_));
	}
	return initialSupp_;
}

Atom_t LogicProgram::falseAtom() {
	Atom_t aFalse = 0;
	for (Var i = 1; i < atoms_.size() && !aFalse; ++i) {
		if (atoms_[i]->value() == value_false || atomState_.isSet(i, AtomState::false_flag)) {
			aFalse = i;
		}
	}
	if (!aFalse) {
		bool s = frozen();
		setFrozen(false);
		aFalse = newAtom();
		assignValue(getAtom(aFalse), value_false, PrgEdge::noEdge());
		setFrozen(s);
	}
	return aFalse;
}

bool LogicProgram::extractCondition(Id_t id, Potassco::LitVec& out) const {
	out.clear();
	if (id == Clasp::Asp::falseId || (frozen() && getLiteral(id) == lit_false())) { return false; }
	if (!id || isAtom(id)) {
		out.assign(id != 0, Potassco::lit(id));
		return true;
	}
	Id_t bId = nodeId(id);
	POTASSCO_ASSERT(validBody(bId), "Invalid literal");
	const PrgBody* B = getBody(getEqBody(bId));
	out.reserve(B->size());
	for (PrgBody::goal_iterator it = B->goals_begin(), end = B->goals_end(); it != end; ++it) {
		out.push_back( toInt(*it) );
	}
	return true;
}
#undef RT
/////////////////////////////////////////////////////////////////////////////////////////
// class LogicProgramAdapter
/////////////////////////////////////////////////////////////////////////////////////////
LogicProgramAdapter::LogicProgramAdapter(LogicProgram& prg) : lp_(&prg), inc_(false) {}
void LogicProgramAdapter::initProgram(bool inc) {
	inc_ = inc;
}
void LogicProgramAdapter::beginStep() {
	if (inc_ || lp_->frozen()) { lp_->updateProgram(); }
}
void LogicProgramAdapter::endStep() {

}
void LogicProgramAdapter::rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body) {
	lp_->addRule(ht, head, body);
}
void LogicProgramAdapter::rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, Potassco::Weight_t bound, const Potassco::WeightLitSpan& body) {
	lp_->addRule(ht, head, bound, body);
}
void LogicProgramAdapter::minimize(Potassco::Weight_t prio, const Potassco::WeightLitSpan& lits) {
	lp_->addMinimize(prio, lits);
}
void LogicProgramAdapter::project(const Potassco::AtomSpan& atoms) {
	lp_->addProject(atoms);
}
void LogicProgramAdapter::output(const Potassco::StringSpan& str, const Potassco::LitSpan& cond) {
	lp_->addOutput(ConstString(str), cond);
}
void LogicProgramAdapter::external(Potassco::Atom_t a, Potassco::Value_t v) {
	lp_->addExternal(a, v);
}
void LogicProgramAdapter::assume(const Potassco::LitSpan& lits) {
	lp_->addAssumption(lits);
}
void LogicProgramAdapter::heuristic(Potassco::Atom_t a, Potassco::Heuristic_t t, int bias, unsigned prio, const Potassco::LitSpan& cond) {
	lp_->addDomHeuristic(a, t, bias, prio, cond);
}
void LogicProgramAdapter::acycEdge(int s, int t, const Potassco::LitSpan& cond) {
	lp_->addAcycEdge(static_cast<uint32>(s), static_cast<uint32>(t), cond);
}
void LogicProgramAdapter::theoryTerm(Potassco::Id_t termId, int number) {
	lp_->theoryData().addTerm(termId, number);
}
void LogicProgramAdapter::theoryTerm(Potassco::Id_t termId, const Potassco::StringSpan& name) {
	lp_->theoryData().addTerm(termId, name);
}
void LogicProgramAdapter::theoryTerm(Potassco::Id_t termId, int cId, const Potassco::IdSpan& args) {
	if (cId >= 0) { lp_->theoryData().addTerm(termId, static_cast<Potassco::Id_t>(cId), args); }
	else { lp_->theoryData().addTerm(termId, static_cast<Potassco::Tuple_t>(cId), args); }
}
void LogicProgramAdapter::theoryElement(Potassco::Id_t elementId, const Potassco::IdSpan& terms, const Potassco::LitSpan& cond) {
	lp_->theoryData().addElement(elementId, terms, lp_->newCondition(cond));
}
void LogicProgramAdapter::theoryAtom(Potassco::Id_t atomOrZero, Potassco::Id_t termId, const Potassco::IdSpan& elements) {
	lp_->theoryData().addAtom(atomOrZero, termId, elements);
}
void LogicProgramAdapter::theoryAtom(Potassco::Id_t atomOrZero, Potassco::Id_t termId, const Potassco::IdSpan& elements, Potassco::Id_t op, Potassco::Id_t rhs) {
	lp_->theoryData().addAtom(atomOrZero, termId, elements, op, rhs);
}
} } // end namespace Asp

