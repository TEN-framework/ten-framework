//
// Copyright (c) 2010-2017 Benjamin Kaufmann
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
#include <clasp/dependency_graph.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/solve_algorithms.h>
#include <clasp/util/timer.h>
namespace Clasp {
SolveTestEvent::SolveTestEvent(const Solver& s, uint32 a_hcc, bool part)
	: SolveEvent<SolveTestEvent>(s, Event::verbosity_max)
	, result(-1), hcc(a_hcc), partial(part) {
	confDelta   = s.stats.conflicts;
	choiceDelta = s.stats.choices;
	time        = 0.0;
}
uint64 SolveTestEvent::choices() const {
	return solver->stats.choices - choiceDelta;
}
uint64 SolveTestEvent::conflicts() const {
	return solver->stats.conflicts - confDelta;
}
namespace Asp {
/////////////////////////////////////////////////////////////////////////////////////////
// class PrgDepGraph
/////////////////////////////////////////////////////////////////////////////////////////
PrgDepGraph::PrgDepGraph(NonHcfMapType m) {
	// add sentinal atom needed for disjunctions
	createAtom(lit_false(), PrgNode::noScc);
	VarVec adj;	adj.push_back(idMax);
	initAtom(sentinel_atom, 0, adj, 0);
	seenComponents_ = 0;
	mapType_        = (uint32)m;
	stats_ = 0;
}

PrgDepGraph::~PrgDepGraph() {
	for (AtomVec::size_type i = 0; i != atoms_.size(); ++i) {
		delete [] atoms_[i].adj_;
	}
	for (AtomVec::size_type i = 0; i != bodies_.size(); ++i) {
		delete [] bodies_[i].adj_;
	}
	delete stats_;
	while (!components_.empty()) {
		delete components_.back();
		components_.pop_back();
	}
}
bool PrgDepGraph::relevantPrgAtom(const Solver& s, PrgAtom* a) const {
	return !a->ignoreScc() && a->inUpper() && a->scc() != PrgNode::noScc && !s.isFalse(a->literal());
}
bool PrgDepGraph::relevantPrgBody(const Solver& s, PrgBody* b) const {
	return !s.isFalse(b->literal());
}

// Creates a positive-body-atom-dependency graph (PBADG)
// The PBADG contains a node for each atom A of a non-trivial SCC and
// a node for each body B, s.th. there is a non-trivially connected atom A with
// B in body(A).
// Pre : b->seen() = 1 for all new and relevant bodies b
// Post: b->seen() = 0 for all bodies that were added to the PBADG
void PrgDepGraph::addSccs(LogicProgram& prg, const AtomList& sccAtoms, const NonHcfSet& nonHcfs) {
	// Pass 1: Create graph atom nodes and estimate number of bodies
	atoms_.reserve(atoms_.size() + sccAtoms.size());
	AtomList::size_type numBodies = 0;
	SharedContext& ctx = *prg.ctx();
	for (AtomList::size_type i = 0, end = sccAtoms.size(); i != end; ++i) {
		PrgAtom* a = sccAtoms[i];
		if (relevantPrgAtom(*ctx.master(), a)) {
			// add graph atom node and store link between program node and graph node for later lookup
			a->resetId(createAtom(a->literal(), a->scc()), true);
			// atom is defined by more than just a bunch of clauses
			ctx.setFrozen(a->var(), true);
			numBodies += a->supports();
		}
	}
	// Pass 2: Init atom nodes and create body nodes
	VarVec adj, ext;
	bodies_.reserve(bodies_.size() + numBodies/2);
	PrgBody* prgBody; PrgDisj* prgDis;
	for (AtomList::size_type i = 0, end = sccAtoms.size(); i != end; ++i) {
		PrgAtom*   a  = sccAtoms[i];
		if (relevantPrgAtom(*ctx.master(), a)) {
			uint32 prop = 0;
			for (PrgAtom::sup_iterator it = a->supps_begin(), endIt = a->supps_end(); it != endIt; ++it) {
				assert(it->isBody() || it->isDisj());
				NodeId bId= PrgNode::noNode;
				if (it->isBody() && !it->isGamma()) {
					prgBody = prg.getBody(it->node());
					bId     = relevantPrgBody(*ctx.master(), prgBody) ? addBody(prg, prgBody) : PrgNode::noNode;
				}
				else if (it->isDisj()) {
					prgDis  = prg.getDisj(it->node());
					bId     = addDisj(prg, prgDis);
					prop   |= AtomNode::property_in_disj;
				}
				if (bId != PrgNode::noNode) {
					if (!bodies_[bId].seen()) {
						bodies_[bId].seen(true);
						adj.push_back(bId);
					}
					if (it->isChoice()) {
						// mark atom as in choice
						prop |= AtomNode::property_in_choice;
					}
				}
			}
			uint32 nPred= (uint32)adj.size();
			for (PrgAtom::dep_iterator it = a->deps_begin(), endIt = a->deps_end(); it != endIt; ++it) {
				if (!it->sign()) {
					prgBody = prg.getBody(it->var());
					if (relevantPrgBody(*ctx.master(), prgBody) && prgBody->scc(prg) == a->scc()) {
						NodeId bodyId = addBody(prg, prgBody);
						if (!bodies_[bodyId].extended()) {
							adj.push_back(bodyId);
						}
						else {
							ext.push_back(bodyId);
							ext.push_back(bodies_[bodyId].get_pred_idx(a->id()));
							assert(bodies_[bodyId].get_pred(ext.back()) == a->id());
							prop |= AtomNode::property_in_ext;
						}
					}
				}
			}
			if (!ext.empty()) {
				adj.push_back(idMax);
				adj.insert(adj.end(), ext.begin(), ext.end());
			}
			adj.push_back(idMax);
			initAtom(a->id(), prop, adj, nPred);
			adj.clear(); ext.clear();
		}
	}
	if (nonHcfs.size() != 0 && stats_ == 0 && nonHcfs.config && nonHcfs.config->context().stats) {
		stats_ = enableNonHcfStats(nonHcfs.config->context().stats, prg.isIncremental());
	}
	// "update" existing non-hcf components
	for (NonHcfIter it = nonHcfBegin(), end = nonHcfEnd(); it != end; ++it) {
		(*it)->update(ctx);
	}
	// add new non-hcf components
	uint32 hcc = seenComponents_;
	for (NonHcfSet::const_iterator it = nonHcfs.begin() + seenComponents_, end = nonHcfs.end(); it != end; ++it, ++hcc) {
		addNonHcf(hcc, ctx, nonHcfs.config, *it);
	}
	seenComponents_ = nonHcfs.size();
}

uint32 PrgDepGraph::createAtom(Literal lit, uint32 aScc) {
	NodeId id    = (uint32)atoms_.size();
	atoms_.push_back(AtomNode());
	AtomNode& ua = atoms_.back();
	ua.lit       = lit;
	ua.scc       = aScc;
	return id;
}

void PrgDepGraph::initAtom(uint32 id, uint32 prop, const VarVec& adj, uint32 numPreds) {
	AtomNode& ua = atoms_[id];
	ua.setProperties(prop);
	ua.adj_      = new NodeId[adj.size()];
	ua.sep_      = ua.adj_ + numPreds;
	NodeId* sExt = ua.adj_;
	NodeId* sSame= sExt + numPreds;
	uint32  aScc = ua.scc;
	for (VarVec::const_iterator it = adj.begin(), end = adj.begin()+numPreds; it != end; ++it) {
		BodyNode& bn = bodies_[*it];
		if (bn.scc != aScc) { *sExt++ = *it; }
		else                { *--sSame= *it; }
		bn.seen(false);
	}
	std::reverse(sSame, ua.adj_ + numPreds);
	std::copy(adj.begin()+numPreds, adj.end(), ua.sep_);
}

uint32 PrgDepGraph::createBody(PrgBody* b, uint32 bScc) {
	NodeId id = (uint32)bodies_.size();
	bodies_.push_back(BodyNode(b, bScc));
	return id;
}

// Creates and initializes a body node for the given body b.
uint32 PrgDepGraph::addBody(const LogicProgram& prg, PrgBody* b) {
	if (b->seen()) {     // first time we see this body -
		VarVec preds, atHeads;
		uint32 bScc  = b->scc(prg);
		NodeId bId   = createBody(b, bScc);
		addPreds(prg, b, bScc, preds);
		addHeads(prg, b, atHeads);
		initBody(bId, preds, atHeads);
		b->resetId(bId, false);
		prg.ctx()->setFrozen(b->var(), true);
	}
	return b->id();
}

// Adds all relevant predecessors of b to preds.
// The returned list looks like this:
// [[B], a1, [w1], ..., aj, [wj], idMax, l1, [w1], ..., lk, [wk], idMax], where
// B is the bound of b (only for card/weight rules),
// ai is a positive predecessor from bScc,
// wi is the weight of ai (only for weight rules), and
// li is a literal of a subgoal from some other scc (only for cardinality/weight rules)
void PrgDepGraph::addPreds(const LogicProgram& prg, PrgBody* b, uint32 bScc, VarVec& preds) const {
	if (bScc == PrgNode::noScc) { preds.clear(); return; }
	const bool weights = b->type() == Body_t::Sum;
	for (uint32 i = 0; i != b->size() && !b->goal(i).sign(); ++i) {
		PrgAtom* pred = prg.getAtom(b->goal(i).var());
		if (relevantPrgAtom(*prg.ctx()->master(), pred) && pred->scc() == bScc) {
			preds.push_back( pred->id() );
			if (weights) { preds.push_back(b->weight(i)); }
		}
	}
	if (b->type() != Body_t::Normal) {
		preds.insert(preds.begin(), b->bound());
		preds.push_back(idMax);
		for (uint32 n = 0; n != b->size(); ++n) {
			PrgAtom* pred = prg.getAtom(b->goal(n).var());
			bool     ext  = b->goal(n).sign() || pred->scc() != bScc;
			Literal lit   = b->goal(n).sign() ? ~pred->literal() : pred->literal();
			if (ext && !prg.ctx()->master()->isFalse(lit)) {
				preds.push_back(lit.rep());
				if (weights) { preds.push_back(b->weight(n)); }
			}
		}
	}
	preds.push_back(idMax);
}

// Splits the heads of b into atoms and disjunctions.
// Disjunctions are flattened to sentinel-enclose datom-lists.
uint32 PrgDepGraph::addHeads(const LogicProgram& prg, PrgBody* b, VarVec& heads) const {
	for (PrgBody::head_iterator it = b->heads_begin(), end = b->heads_end(); it != end; ++it) {
		if (it->isAtom() && !it->isGamma()) {
			PrgAtom* a = prg.getAtom(it->node());
			if (relevantPrgAtom(*prg.ctx()->master(), a)) {
				heads.push_back(a->id());
			}
		}
		else if (it->isDisj()) {
			assert(prg.getDisj(it->node())->inUpper() && prg.getDisj(it->node())->supports() == 1);
			PrgDisj* d = prg.getDisj(it->node());
			// flatten disjunction and enclose in sentinels
			heads.push_back(sentinel_atom);
			getAtoms(prg, d, heads);
			heads.push_back(sentinel_atom);
		}
	}
	return sizeVec(heads);
}

// Adds the atoms from the given disjunction to atoms and returns the disjunction's scc.
uint32 PrgDepGraph::getAtoms(const LogicProgram& prg, PrgDisj* d, VarVec& atoms) const {
	uint32 scc = PrgNode::noScc;
	for (PrgDisj::atom_iterator it = d->begin(), end = d->end(); it != end; ++it) {
		PrgAtom* a = prg.getAtom(*it);
		if (relevantPrgAtom(*prg.ctx()->master(), a)) {
			assert(scc == PrgNode::noScc || scc == a->scc());
			atoms.push_back(a->id());
			scc = a->scc();
		}
	}
	return scc;
}

// Initializes preds and succs lists of the body node with the given id.
void PrgDepGraph::initBody(uint32 id, const VarVec& preds, const VarVec& atHeads) {
	BodyNode* bn = &bodies_[id];
	uint32 nSuccs= sizeVec(atHeads);
	bn->adj_     = new NodeId[nSuccs + preds.size()];
	bn->sep_     = bn->adj_ + nSuccs;
	NodeId* sSame= bn->adj_;
	NodeId* sExt = sSame + nSuccs;
	uint32  bScc = bn->scc;
	uint32  hScc = PrgNode::noScc;
	uint32  disj = 0;
	for (VarVec::const_iterator it = atHeads.begin(), end = atHeads.end(); it != end;) {
		if (*it) {
			hScc = getAtom(*it).scc;
			if (hScc == bScc) { *sSame++ = *it++; }
			else              { *--sExt  = *it++; }
		}
		else {
			hScc = getAtom(it[1]).scc; ++disj;
			if (hScc == bScc) { *sSame++ = *it++; while ( (*sSame++ = *it++) ) { ; } }
			else              { *--sExt  = *it++; while ( (*--sExt  = *it++) ) { ; } }
		}
	}
	std::copy(preds.begin(), preds.end(), bn->sep_);
	bn->sep_ += bn->extended();
	if (disj) { bodies_[id].data |= BodyNode::flag_has_delta; }
}

uint32 PrgDepGraph::addDisj(const LogicProgram& prg, PrgDisj* d) {
	assert(d->inUpper() && d->supports() == 1);
	if (d->seen()) { // first time we see this disjunction
		PrgBody* prgBody = prg.getBody(d->supps_begin()->node());
		uint32   bId     = PrgNode::noNode;
		if (relevantPrgBody(*prg.ctx()->master(), prgBody)) {
			bId = addBody(prg, prgBody);
		}
		d->resetId(bId, false);
	}
	return d->id();
}

void PrgDepGraph::addNonHcf(uint32 id, SharedContext& ctx, Configuration* config, uint32 scc) {
	VarVec sccAtoms, sccBodies;
	// get all atoms from scc
	for (uint32 i = 0; i != numAtoms(); ++i) {
		if (getAtom(i).scc == scc) {
			sccAtoms.push_back(i);
			atoms_[i].set(AtomNode::property_in_non_hcf);
		}
	}
	// get all bodies defining an atom in scc
	const Solver& generator = *ctx.master(); (void)generator;
	for (uint32 i = 0; i != sccAtoms.size(); ++i) {
		const AtomNode& a = getAtom(sccAtoms[i]);
		for (const NodeId* bodyIt = a.bodies_begin(), *bodyEnd = a.bodies_end(); bodyIt != bodyEnd; ++bodyIt) {
			BodyNode& B = bodies_[*bodyIt];
			if (!B.seen()) {
				assert(generator.value(B.lit.var()) != value_free || !generator.seen(B.lit));
				sccBodies.push_back(*bodyIt);
				B.seen(true);
			}
		}
	}
	for (uint32 i = 0; i != sccBodies.size(); ++i) { bodies_[sccBodies[i]].seen(false); }
	components_.push_back( new NonHcfComponent(id, *this, ctx, config, scc, sccAtoms, sccBodies) );
	if (stats_) { stats_->addHcc(*components_.back()); }
}
void PrgDepGraph::simplify(const Solver& s) {
	const bool shared        = s.sharedContext()->isShared();
	ComponentVec::iterator j = components_.begin();
	for (ComponentVec::iterator it = components_.begin(), end = components_.end(); it != end; ++it) {
		bool ok = (*it)->simplify(s);
		if (!shared) {
			if (ok) { *j++ = *it; }
			else    {
				if (stats_) { stats_->removeHcc(**it); }
				delete *it;
			}
		}
	}
	if (!shared) { components_.erase(j, components_.end()); }
}
PrgDepGraph::NonHcfStats* PrgDepGraph::enableNonHcfStats(uint32 level, bool inc) {
	if (!stats_) { stats_ = new NonHcfStats(*this, level, inc); }
	return stats_;
}
/////////////////////////////////////////////////////////////////////////////////////////
// class PrgDepGraph::NonHcfComponent::ComponentMap
/////////////////////////////////////////////////////////////////////////////////////////
class PrgDepGraph::NonHcfComponent::ComponentMap {
public:
	ComponentMap() { static_assert(sizeof(Mapping) == sizeof(uint64), "Invalid padding!"); }
	struct Mapping {
		explicit Mapping(NodeId id) : node(id), var(0), ext(0) { }
		uint32 node;     // node id in dep-graph of generator program P
		uint32 var : 30; // var in tester solver
		uint32 ext :  2; // additional data
		// Atom
		bool  disj() const { return ext != 0u; }
		bool hasTp() const { return ext == 2u; }
		Literal up() const { return posLit(var); }
		Literal hp() const { assert(disj()); return posLit(var + 1); }
		Literal tp() const { assert(disj()); return posLit((var + 2)*uint32(hasTp())); }
		// Body
		Literal fb() const { return Literal(var, (ext & 1u) != 0u); }
		bool    eq() const { return ext != 0u; }
		bool operator<(const Mapping& other) const { return node < other.node; }
	};
	typedef  PrgDepGraph                 SccGraph;
	typedef  PodVector<Mapping>::type    NodeMap;
	typedef  NodeMap::iterator           MapIt;
	typedef  NodeMap::const_iterator     MapIt_c;
	typedef  std::pair<MapIt_c, MapIt_c> MapRange;
	void     addVars(Solver& generator, const SccGraph& dep, const VarVec& atoms, const VarVec& bodies, SharedContext& out);
	void     addAtomConstraints(SharedContext& out);
	void     addBodyConstraints(const Solver& generator, const SccGraph& dep, uint32 scc, SharedContext& out);
	void     mapGeneratorAssignment(const Solver& generator, const SccGraph& dep, LitVec& out) const;
	void     mapTesterModel(const Solver& tester, VarVec& out) const;
	bool     simplify(const Solver& generator, const SccGraph& dep, Solver& tester);
	MapRange atoms() const { return MapRange(mapping.begin(), mapping.begin() + numAtoms); }
	MapRange bodies()const { return MapRange(mapping.begin() + numAtoms, mapping.end()); }
	MapIt_c  findAtom(NodeId nodeId) const { return std::lower_bound(mapping.begin(), mapping.begin()+numAtoms, Mapping(nodeId)); }
	NodeMap  mapping; // maps nodes of P to literals in C;
	uint32   numAtoms;// number of atoms
};
// Adds necessary variables for all atoms and bodies to the component program.
// Input-Vars: (set via assumptions)
//  tp: for each atom p in a proper disjunctive head, tp is true iff p is true in P
//  fb: for each body b, fb is true iff b is false in P
// Aux-Var: (derived)
//  hp: for each atom p in a proper disjunctive head, hp is true iff tp and ~up
// Output: (unfounded sets)
//  up: for each atom p, up is true iff a is unfounded w.r.t the assignment of P.
void PrgDepGraph::NonHcfComponent::ComponentMap::addVars(Solver& generator, const SccGraph& dep, const VarVec& atoms, const VarVec& bodies, SharedContext& comp) {
	assert(generator.decisionLevel() == 0);
	mapping.reserve(atoms.size() + bodies.size());
	const PrgDepGraph::NonHcfMapType mt = dep.nonHcfMapType();
	for (VarVec::const_iterator it = atoms.begin(), end = atoms.end(); it != end; ++it) {
		const AtomNode& at = dep.getAtom(*it);
		Literal gen        = at.lit;
		if (generator.isFalse(gen)) { continue; }
		Mapping map(*it);
		// up [ hp [tp] ]
		map.var = comp.addVar(Var_t::Atom); // up
		map.ext = (mt == PrgDepGraph::map_old || at.inDisjunctive());
		comp.setFrozen(map.var, true);
		if (map.ext) {
			comp.addVar(Var_t::Atom); // hp
			if (!generator.isTrue(gen)) { // tp
				comp.setFrozen(comp.addVar(Var_t::Atom), true);
				++map.ext;
			}
		}
		mapping.push_back(map);
	}
	numAtoms = (uint32)mapping.size();
	std::stable_sort(mapping.begin(), mapping.end());
	// add necessary vars for bodies
	for (VarVec::const_iterator it = bodies.begin(), end = bodies.end(); it != end; ++it) {
		Literal gen = dep.getBody(*it).lit;
		if (generator.isFalse(gen))  { continue; }
		Mapping map(*it);
		if (!generator.seen(gen) && !generator.isTrue(gen)) {
			map.var = comp.addVar(Var_t::Atom);
			comp.setFrozen(map.var, true);
			generator.markSeen(gen);
		}
		else if (generator.isTrue(gen)) {
			map.ext = 1u;
		}
		else {
			map.ext = 2u;
			for (MapRange r = this->bodies(); r.first != r.second;) {
				--r.second;
				if (dep.getBody(r.second->node).lit == gen) {
					map.var = r.second->var;
					break;
				}
			}
		}
		assert(map.var <= comp.numVars() && (map.var || map.ext == 1u));
		mapping.push_back(map);
	}
	for (MapRange r = this->bodies(); r.first != r.second; ++r.first) {
		if (!r.first->eq()) {
			Var v = dep.getBody(r.first->node).lit.var();
			generator.clearSeen(v);
		}
	}
}

// Adds constraints stemming from the given atoms to the component program.
// 1. [up(a0) v ... v up(an-1)], where
//   - ai is an atom in P from the given atom set, and
//   - up(ai) is the corresponding output-atom in the component program C.
// 2. For each atom ai in atom set occurring in a proper disjunction, [hp(ai) <=> tp(ai), ~up(ai)], where
//   tp(ai), hp(ai), up(ai) are the input, aux, and output atoms in C.
void PrgDepGraph::NonHcfComponent::ComponentMap::addAtomConstraints(SharedContext& comp) {
	ClauseCreator cc1(comp.master()), cc2(comp.master());
	cc1.addDefaultFlags(ClauseCreator::clause_force_simplify);
	cc1.start();
	for (MapRange r = atoms(); r.first != r.second; ++r.first) {
		const Mapping& m = *r.first;
		cc1.add(m.up());
		if (m.disj()) {
			cc2.start().add(~m.tp()).add(m.up()).add(m.hp()).end(); // [~tp v up v hp]
			cc2.start().add(~m.hp()).add(m.tp()).end();  // [~hp v tp]
			cc2.start().add(~m.hp()).add(~m.up()).end(); // [~hp v ~up]
		}
	}
	cc1.end();
}

// Adds constraints stemming from the given bodies to the component program.
// For each atom ai and rule a0 | ai | ...| an :- B, s.th. B in bodies
//  [~up(ai) v fb(B) V hp(aj), j != i V up(p), p in B+ ^ C], where
// hp(ai), up(ai) are the aux and output atoms of ai in C.
void PrgDepGraph::NonHcfComponent::ComponentMap::addBodyConstraints(const Solver& generator, const SccGraph& dep, uint32 scc, SharedContext& comp) {
	ClauseCreator cc(comp.master());
	cc.addDefaultFlags(ClauseCreator::clause_force_simplify);
	ClauseCreator dc(comp.master());
	MapIt j = mapping.begin() + numAtoms;
	for (MapRange r = bodies(); r.first != r.second; ++r.first) {
		const BodyNode& B = dep.getBody(r.first->node);
		if (generator.isFalse(B.lit)) { continue; }
		POTASSCO_REQUIRE(!B.extended(), "Extended bodies not supported - use '--trans-ext=weight'");
		for (const NodeId* hIt = B.heads_begin(), *hEnd = B.heads_end(); hIt != hEnd; ++hIt) {
			uint32 hScc = *hIt ? dep.getAtom(*hIt).scc : dep.getAtom(hIt[1]).scc;
			if (hScc != scc) {
				// the head is not relevant to this non-hcf - skip it
				if (!*hIt) { do { ++hIt; } while (*hIt); }
				continue;
			}
			// [fb(B) v ~up(a) V hp(o) for all o != a in B.disHead V up(b) for each b in B+ ^ C]
			cc.start().add(r.first->fb());
			if (B.scc == scc) { // add subgoals from same scc
				for (const NodeId* aIt = B.preds(); *aIt != idMax; ++aIt) {
					MapIt_c atMapped = findAtom(*aIt);
					cc.add(atMapped->up());
				}
			}
			if (*hIt) { // normal head
				MapIt_c atMapped = findAtom(*hIt);
				assert(atMapped != atoms().second);
				cc.add(~atMapped->up());
				cc.end();
			}
			else { // disjunctive head
				const NodeId* dHead = ++hIt;
				for (; *hIt; ++hIt) {
					dc.start();
					dc = cc;
					MapIt_c atMapped = findAtom(*hIt);
					dc.add(~atMapped->up());
					for (const NodeId* other = dHead; *other; ++other) {
						if (*other != *hIt) {
							assert(dep.getAtom(*other).scc == scc);
							atMapped = findAtom(*other);
							dc.add(atMapped->hp());
						}
					}
					dc.end();
				}
			}
		}
		if (!r.first->eq()) { *j++ = *r.first; }
	}
	mapping.erase(j, mapping.end());
}

// Maps the generator assignment given in s to a list of tester assumptions.
void PrgDepGraph::NonHcfComponent::ComponentMap::mapGeneratorAssignment(const Solver& s, const SccGraph& dep, LitVec& assume) const {
	Literal  gen;
	assume.clear(); assume.reserve(mapping.size());
	for (MapRange r = atoms(); r.first != r.second; ++r.first) {
		const Mapping& at = *r.first;
		gen = dep.getAtom(at.node).lit;
		if (at.hasTp()) {
			assume.push_back(at.tp() ^ (!s.isTrue(gen)));
		}
		if (s.isFalse(gen)) { assume.push_back(~at.up()); }
	}
	for (MapRange r = bodies(); r.first != r.second; ++r.first) {
		gen = dep.getBody(r.first->node).lit;
		assume.push_back(r.first->fb() ^ (!s.isFalse(gen)));
	}
}
// Maps the tester model given in s back to a list of unfounded atoms in the generator.
void PrgDepGraph::NonHcfComponent::ComponentMap::mapTesterModel(const Solver& s, VarVec& out) const {
	assert(s.numFreeVars() == 0);
	out.clear();
	for (MapRange r = atoms(); r.first != r.second; ++r.first) {
		if (s.isTrue(r.first->up())) {
			out.push_back(r.first->node);
		}
	}
}
bool PrgDepGraph::NonHcfComponent::ComponentMap::simplify(const Solver& generator, const SccGraph& dep, Solver& tester) {
	if (!tester.popRootLevel(UINT32_MAX)) { return false; }
	if (tester.sharedContext()->isShared() && (tester.sharedContext()->allowImplicit(Constraint_t::Conflict) || tester.sharedContext()->distributor.get())) {
		// Simplification not safe: top-level assignments of threads are
		// not necessarily synchronised at this point and clauses simplified
		// with top-level assignment of this thread might not (yet) be valid
		// wrt possible assumptions in other threads.
		return true;
	}
	const bool rem = !tester.sharedContext()->isShared();
	MapIt j        = rem ? mapping.begin() : mapping.end();
	for (MapIt_c it = mapping.begin(), aEnd = it + numAtoms, end = mapping.end(); it != end; ++it) {
		const Mapping& m = *it;
		const bool  atom = it < aEnd;
		Literal        g = atom ? dep.getAtom(m.node).lit : dep.getBody(m.node).lit;
		if (generator.topValue(g.var()) == value_free) {
			if (rem) { *j++ = m; }
			continue;
		}
		bool isFalse = generator.isFalse(g);
		bool ok      = atom || tester.force(isFalse ? m.fb() : ~m.fb());
		if (atom) {
			if (!isFalse){ ok = !m.hasTp() || tester.force(m.tp());  if (rem) { *j++ = m; } }
			else         { ok = tester.force(~m.up()) && (!m.hasTp() || tester.force(~m.tp())); numAtoms -= (ok && rem); }
		}
		if (!ok) {
			if (rem) { j = std::copy(it, end, j); }
			break;
		}
	}
	mapping.erase(j, mapping.end());
	return tester.simplify();
}
/////////////////////////////////////////////////////////////////////////////////////////
// class PrgDepGraph::NonHcfComponent
/////////////////////////////////////////////////////////////////////////////////////////
PrgDepGraph::NonHcfComponent::NonHcfComponent(uint32 id, const PrgDepGraph& dep, SharedContext& genCtx, Configuration* c, uint32 scc, const VarVec& atoms, const VarVec& bodies)
	: dep_(&dep)
	, prg_(new SharedContext())
	, comp_(new ComponentMap())
	, id_(id)
	, scc_(scc) {
	Solver& generator = *genCtx.master();
	prg_->setConcurrency(genCtx.concurrency(), SharedContext::resize_reserve);
	prg_->setConfiguration(c, Ownership_t::Retain);
	comp_->addVars(generator, dep, atoms, bodies, *prg_);
	prg_->startAddConstraints();
	comp_->addAtomConstraints(*prg_);
	comp_->addBodyConstraints(generator, dep, scc, *prg_);
	prg_->endInit(true);
}

PrgDepGraph::NonHcfComponent::~NonHcfComponent() {
	delete prg_;
	delete comp_;
}

void PrgDepGraph::NonHcfComponent::update(const SharedContext& generator) {
	for (uint32 i = 0; generator.hasSolver(i); ++i) {
		if (!prg_->hasSolver(i)) { prg_->attach(prg_->pushSolver());   }
		else                     { prg_->initStats(*prg_->solver(i)); }
	}
}

void PrgDepGraph::NonHcfComponent::assumptionsFromAssignment(const Solver& s, LitVec& assume) const {
	comp_->mapGeneratorAssignment(s, *dep_, assume);
}

bool PrgDepGraph::NonHcfComponent::test(const Solver& generator, const LitVec& assume, VarVec& unfoundedOut) const {
	assert(generator.id() < prg_->concurrency() && "Invalid id!");
	// Forwards to message handler of generator so that messages are
	// handled during long running tests.
	struct Tester : MessageHandler {
		Tester(Solver& s, MessageHandler* gen) : solver(&s), generator(gen) { if (gen) { s.addPost(this); } }
		~Tester() { if (generator) { solver->removePost(this); } }
		bool handleMessages()                            { return generator->handleMessages(); }
		bool propagateFixpoint(Solver&, PostPropagator*) { return Tester::handleMessages() || !terminate(); }
		bool terminate()                                 { solver->setStopConflict(); return true; }
		int test(const LitVec& assume) {
			return int(BasicSolve(*solver).satisfiable(assume, solver->stats.choices == 0) == false);
		}
		Solver*         solver;
		MessageHandler* generator;
	} tester(*prg_->solver(generator.id()), static_cast<MessageHandler*>(generator.getPost(PostPropagator::priority_reserved_msg)));
	SolveTestEvent ev(*tester.solver, id_, generator.numFreeVars() != 0);
	tester.solver->stats.addTest(ev.partial);
	generator.sharedContext()->report(ev);
	ev.time = ThreadTime::getTime();
	if ((ev.result = tester.test(assume)) == 0) {
		tester.solver->stats.addModel(tester.solver->decisionLevel());
		comp_->mapTesterModel(*tester.solver, unfoundedOut);
	}
	ev.time = ThreadTime::getTime() - ev.time;
	tester.solver->stats.addCpuTime(ev.time);
	generator.sharedContext()->report(ev);
	return ev.result != 0;
}
bool PrgDepGraph::NonHcfComponent::simplify(const Solver& s) const {
	return comp_->simplify(s, *dep_, *prg_->solver(s.id()));
}
/////////////////////////////////////////////////////////////////////////////////////////
// class PrgDepGraph::NonHcfStats
/////////////////////////////////////////////////////////////////////////////////////////
struct PrgDepGraph::NonHcfStats::Data {
	typedef StatsVec<ProblemStats> ProblemVec;
	typedef StatsVec<SolverStats>  SolverVec;
	struct ComponentStats {
		ProblemVec problem;
		SolverVec  solvers;
		SolverVec  accu;
	};
	Data(uint32 level, bool inc) : components(level > 1 ? new ComponentStats : 0) {
		if (inc) { solvers.multi = new SolverStats(); }
	}
	~Data() { delete components; delete solvers.multi; }
	void addHcc(const NonHcfComponent& c) {
		assert(components);
		ProblemVec&   hcc = components->problem;
		SolverVec& solver = components->solvers;
		SolverVec*   accu = solvers.multi ? &components->accu : 0;
		uint32 id = c.id();
		if (id >= hcc.size()) {
			hcc.growTo(id + 1);
			solver.growTo(id + 1);
			if (accu) { accu->growTo(id + 1); }
		}
		if (!hcc[id]) {
			hcc[id]    = new ProblemStats(c.ctx().stats());
			solver[id] = new SolverStats();
			if (accu) { (*accu)[id] = new SolverStats(); solver[id]->multi = (*accu)[id]; }
		}
	}
	void updateHcc(const NonHcfComponent& c) {
		c.ctx().accuStats(solvers);
		if (components && c.id() < components->solvers.size()) {
			POTASSCO_REQUIRE(components->solvers[c.id()], "component not added to stats!");
			c.ctx().accuStats(*components->solvers[c.id()]);
			components->solvers[c.id()]->flush();
		}
	}
	ProblemStats    hccs;
	SolverStats     solvers;
	ComponentStats* components;
};
PrgDepGraph::NonHcfStats::NonHcfStats(PrgDepGraph& g, uint32 l, bool inc) : graph_(&g), data_(new Data(l, inc)) {
	for (NonHcfIter it = g.nonHcfBegin(), end = g.nonHcfEnd(); it != end; ++it) {
		addHcc(**it);
	}
}
PrgDepGraph::NonHcfStats::~NonHcfStats() { delete data_; }
void PrgDepGraph::NonHcfStats::accept(StatsVisitor& out, bool final) const {
	if (!data_->solvers.multi) { final = false; }
	out.visitProblemStats(data_->hccs);
	out.visitSolverStats(final ? *data_->solvers.multi : data_->solvers);
	if (data_->components && out.visitHccs(StatsVisitor::Enter)) {
		const Data::SolverVec& solver = final ? data_->components->accu : data_->components->solvers;
		const Data::ProblemVec&   hcc = data_->components->problem;
		for (uint32 i = 0, end = sizeVec(hcc); i != end; ++i) {
			out.visitHcc(i, *hcc[i], *solver[i]);
		}
		out.visitHccs(StatsVisitor::Leave);
	}
}
void PrgDepGraph::NonHcfStats::startStep(uint32 statsLevel) {
	data_->solvers.reset();
	if (data_->components) { data_->components->solvers.reset(); }
	if (statsLevel > 1 && !data_->components) {
		data_->components = new Data::ComponentStats();
		for (NonHcfIter it = graph_->nonHcfBegin(), end = graph_->nonHcfEnd(); it != end; ++it) {
			data_->addHcc(**it);
		}
	}
}
void PrgDepGraph::NonHcfStats::endStep() {
	for (NonHcfIter it = graph_->nonHcfBegin(), end = graph_->nonHcfEnd(); it != end; ++it) {
		data_->updateHcc(**it);
	}
	data_->solvers.flush();
}
void PrgDepGraph::NonHcfStats::addHcc(const NonHcfComponent& c) {
	data_->hccs.accu(c.ctx().stats());
	if (data_->components) { data_->addHcc(c); }
}
void PrgDepGraph::NonHcfStats::removeHcc(const NonHcfComponent& c) {
	data_->updateHcc(c);
}
void PrgDepGraph::NonHcfStats::addTo(StatsMap& problem, StatsMap& solving, StatsMap* accu) const {
	data_->solvers.addTo("hccs", solving, accu);
	problem.add("hccs", StatisticObject::map(&data_->hccs));
	if (data_->components) {
		problem.add("hcc", data_->components->problem.toStats());
		solving.add("hcc", data_->components->solvers.toStats());
		if (accu) { accu->add("hcc", data_->components->accu.toStats()); }
	}
}
} // namespace Asp
/////////////////////////////////////////////////////////////////////////////////////////
// class ExtDepGraph
/////////////////////////////////////////////////////////////////////////////////////////
ExtDepGraph::ExtDepGraph(uint32) : maxNode_(0), comEdge_(0), genCnt_(0) {}
ExtDepGraph::~ExtDepGraph(){}
void ExtDepGraph::addEdge(Literal lit, uint32 startNode, uint32 endNode) {
	POTASSCO_REQUIRE(!frozen(), "ExtDepGraph::update() not called!");
	fwdArcs_.push_back(Arc::create(lit, startNode, endNode));
	maxNode_ = std::max(std::max(startNode, endNode)+uint32(1), maxNode_);
	if (comEdge_ && std::min(startNode, endNode) < nodes_.size()) {
		invArcs_.clear();
		comEdge_ = 0;
		++genCnt_;
	}
}
bool ExtDepGraph::frozen() const {
	return !fwdArcs_.empty() && fwdArcs_.back().tail() == UINT32_MAX;
}
void ExtDepGraph::update() {
	if (frozen()) {
		fwdArcs_.pop_back();
	}
}
uint32 ExtDepGraph::finalize(SharedContext& ctx) {
	if (frozen()) {
		return comEdge_;
	}
	// sort by end node
	std::sort(fwdArcs_.begin() + comEdge_, fwdArcs_.end(), CmpArc<1>());
	invArcs_.reserve(fwdArcs_.size());
	Node sent = { UINT32_MAX, UINT32_MAX };
	nodes_.resize(maxNode_, sent);
	for (ArcVec::const_iterator it = fwdArcs_.begin() + comEdge_, end = fwdArcs_.end(); it != end;) {
		uint32 node = it->head();
		POTASSCO_REQUIRE(!comEdge_ || nodes_[node].invOff == UINT32_MAX, "ExtDepGraph: invalid incremental update!");
		Inv inv;
		nodes_[node].invOff = (uint32)invArcs_.size();
		do {
			inv.lit  = it->lit;
			inv.rep  = static_cast<uint32>(it->tail() << 1) | 1u;
			invArcs_.push_back(inv);
			ctx.setFrozen(it->lit.var(), true);
		} while (++it != end && it->head() == node);
		invArcs_.back().rep ^= 1u;
	}
	// sort by start node
	std::sort(fwdArcs_.begin() + comEdge_, fwdArcs_.end(), CmpArc<0>());
	for (ArcVec::const_iterator it = fwdArcs_.begin() + comEdge_, end = fwdArcs_.end(); it != end;) {
		uint32 node = it->tail();
		POTASSCO_REQUIRE(!comEdge_ || nodes_[node].fwdOff == UINT32_MAX, "ExtDepGraph: invalid incremental update!");
		nodes_[node].fwdOff = static_cast<uint32>(it - fwdArcs_.begin());
		it          = std::lower_bound(it, end, node + 1, CmpArc<0>());
	}
	comEdge_ = (uint32)fwdArcs_.size();
	fwdArcs_.push_back(Arc::create(lit_false(), UINT32_MAX, UINT32_MAX));
	return comEdge_;
}
uint64 ExtDepGraph::attach(Solver& s, Constraint& p, uint64 genId) {
	uint32 count = static_cast<uint32>(genId >> 32);
	uint32 edges = static_cast<uint32>(genId);
	uint32 update= count == genCnt_ ? 0 : edges;
	GenericWatch* w;
	for (uint32 i = (count == genCnt_ ? edges : 0), eId, end = comEdge_; i < end; ++i) {
		const Arc& a = fwdArcs_[i];
		if (a.head() != a.tail()) {
			if (s.topValue(a.lit.var()) == value_free) {
				if (!update || (w = s.getWatch(a.lit, &p)) == 0) {
					s.addWatch(a.lit, &p, i);
				}
				else {
					w->data = i;
					--update;
				}
			}
			else if (s.isTrue(a.lit)) {
				p.propagate(s, a.lit, (eId = i));
			}
		}
		else if (!s.force(~a.lit)) {
			break;
		}
	}
	return (static_cast<uint64>(genCnt_) << 32) | comEdge_;
}
void ExtDepGraph::detach(Solver* s, Constraint& p) {
	if (s) {
		for (ArcVec::size_type i = fwdArcs_.size(); i--; ) {
			s->removeWatch(fwdArcs_[i].lit, &p);
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// class AcyclicityCheck
/////////////////////////////////////////////////////////////////////////////////////////
struct AcyclicityCheck::ReasonStore {
	typedef PodVector<LitVec*>::type NogoodMap;
	NogoodMap db;
	void getReason(Literal p, LitVec& out) {
		if (const LitVec* r = db[p.var()]) {
			out.insert(out.end(), r->begin(), r->end());
		}
	}
	void setReason(Literal p, LitVec::const_iterator first, LitVec::const_iterator end) {
		Var v = p.var();
		if (v >= db.size()) { db.resize(v+1, 0); }
		if (db[v] == 0)     { db[v] = new LitVec(first, end); }
		else                { db[v]->assign(first, end); }
	}
	~ReasonStore() {
		std::for_each(db.begin(), db.end(), DeleteObject());
	}
};
AcyclicityCheck::AcyclicityCheck(DependencyGraph* graph) : graph_(graph), solver_(0), nogoods_(0), strat_(bit_mask<uint32>(config_bit)), tagCnt_(0), genId_(0)  {
}
AcyclicityCheck::~AcyclicityCheck() {
	delete nogoods_;
}

void AcyclicityCheck::setStrategy(Strategy p) {
	strat_ = p;
}
void AcyclicityCheck::setStrategy(const SolverParams& p) {
	if (p.acycFwd) { setStrategy(prop_fwd);  }
	else           { setStrategy(p.loopRep == LoopReason_t::Implicit ? prop_full_imp : prop_full); }
	store_set_bit(strat_, config_bit);
}

bool AcyclicityCheck::init(Solver& s) {
	if (!graph_) { graph_ = s.sharedContext()->extGraph.get(); }
	if (!graph_) { return true; }
	if (test_bit(strat_, config_bit)) {
		setStrategy(s.sharedContext()->configuration()->solver(s.id()));
	}
	tags_.assign(graph_->nodes(), tagCnt_ = 0);
	parent_.resize(graph_->nodes());
	todo_.clear();
	solver_ = &s;
	genId_  = graph_->attach(s, *this, genId_);
	return true;
}

uint32 AcyclicityCheck::startSearch() {
	if (++tagCnt_ != 0) { return tagCnt_; }
	const uint32 last = tagCnt_ - 1;
	for (Var v = 0; v != tags_.size(); ++v) {
		tags_[v] = tags_[v] == last;
	}
	return tagCnt_ = 2;
}
void AcyclicityCheck::setReason(Literal p, LitVec::const_iterator first, LitVec::const_iterator end) {
	if (!nogoods_) { nogoods_ = new ReasonStore(); }
	nogoods_->setReason(p, first, end);
}
void AcyclicityCheck::addClauseLit(Solver& s, Literal p) {
	assert(s.isFalse(p));
	uint32 dl = s.level(p.var());
	if (dl && !s.seen(p)) {
		s.markSeen(p);
		s.markLevel(dl);
		reason_.push_back(p);
	}
}

void AcyclicityCheck::reset() {
	todo_.clear();
	reason_.clear();
}

bool AcyclicityCheck::valid(Solver& s) {
	if (todo_.empty()) { return true; }
	return AcyclicityCheck::propagateFixpoint(s, 0);
}
bool AcyclicityCheck::isModel(Solver& s) {
	return AcyclicityCheck::valid(s);
}

void AcyclicityCheck::destroy(Solver* s, bool detach) {
	if (s && detach) {
		s->removePost(this);
	}
	if (graph_) {
		graph_->detach(detach ? s : 0, *this);
	}
	PostPropagator::destroy(s, detach);
}
void AcyclicityCheck::reason(Solver&, Literal p, LitVec& out) {
	if (!reason_.empty() && reason_[0] == p) {
		out.insert(out.end(), reason_.begin()+1, reason_.end());
	}
	else if (nogoods_) {
		nogoods_->getReason(p, out);
	}
}

bool AcyclicityCheck::propagateFixpoint(Solver& s, PostPropagator*) {
	for (Arc x; !todo_.empty();) {
		x = todo_.pop_ret();
		if (!dfsForward(s, x) || (strategy() != prop_fwd && !dfsBackward(s, x))) {
			return false;
		}
	}
	todo_.clear();
	return true;
}
bool AcyclicityCheck::dfsForward(Solver& s, const Arc& root) {
	const uint32 tag = startSearch();
	nStack_.clear();
	pushVisit(root.head(), tag);
	for (Var node, nodeNext; !nStack_.empty();) {
		node = nStack_.back();
		nStack_.pop_back();
		for (const Arc* a = graph_->fwdBegin(node); a; a = graph_->fwdNext(a)) {
			if (s.isTrue(a->lit)) {
				nodeNext = a->head();
				if (nodeNext == root.tail()) {
					setParent(nodeNext, Parent::create(a->lit, node));
					reason_.assign(1, ~root.lit);
					for (Var n0 = nodeNext; n0 != root.head();) {
						Parent parent = parent_[n0];
						assert(s.isTrue(parent.lit));
						reason_.push_back(parent.lit);
						n0 = parent.node;
					}
					return s.force(~root.lit, this);
				}
				else if (!visited(nodeNext, tag)) {
					setParent(nodeNext, Parent::create(a->lit, node));
					pushVisit(nodeNext, tag);
				}
			}
		}
	}
	return true;
}
bool AcyclicityCheck::dfsBackward(Solver& s, const Arc& root) {
	const uint32 tag = startSearch();
	const uint32 fwd = tag - 1;
	nStack_.clear();
	pushVisit(root.tail(), tag);
	for (Var node, nodeNext; !nStack_.empty(); ) {
		node = nStack_.back();
		nStack_.pop_back();
		for (const Inv* a = graph_->invBegin(node); a; a = graph_->invNext(a)) {
			ValueRep val = s.value(a->lit.var());
			if (val == falseValue(a->lit) || visited(nodeNext = a->tail(), tag)) { continue; }
			if (visited(nodeNext, fwd)) { // a->lit would complete a cycle - force to false
				assert(val == value_free || s.level(a->lit.var()) == s.decisionLevel());
				reason_.assign(1, ~a->lit);
				addClauseLit(s, ~root.lit);
				for (Var n = nodeNext; n != root.head(); ) {
					Parent parent = parent_[n];
					assert(s.isTrue(parent.lit) && visited(parent.node, fwd));
					addClauseLit(s, ~parent.lit);
					n = parent.node;
				}
				for (Var n = node; n != root.tail(); ) {
					Parent parent = parent_[n];
					assert(s.isTrue(parent.lit)&& visited(parent.node, tag));
					addClauseLit(s, ~parent.lit);
					n = parent.node;
				}
				if (val == value_free && strategy() == prop_full) {
					ConstraintInfo info(Constraint_t::Loop);
					s.finalizeConflictClause(reason_, info, 0);
					ClauseCreator::create(s, reason_, ClauseCreator::clause_no_prepare, info);
				}
				else {
					for (uint32 i = 1; i != reason_.size(); ++i) {
						s.clearSeen(reason_[i].var());
						reason_[i] = ~reason_[i];
					}
					if (!s.force(~a->lit, this)) { return false; }
					setReason(~a->lit, reason_.begin()+1, reason_.end());
				}
				assert(s.isFalse(a->lit));
				if (!s.propagateUntil(this)) { return false; }
			}
			else if (val != value_free) { // follow true edge backward
				setParent(nodeNext, Parent::create(a->lit, node));
				pushVisit(nodeNext, tag);
			}
		}
	}
	return true;
}
}

