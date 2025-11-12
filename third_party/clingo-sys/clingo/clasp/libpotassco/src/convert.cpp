//
// Copyright (c) 2015-2017 Benjamin Kaufmann
//
// This file is part of Potassco.
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
#include <potassco/convert.h>
#include <potassco/string_convert.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <map>
#include <string>
#include POTASSCO_EXT_INCLUDE(unordered_map)
typedef POTASSCO_EXT_NS::unordered_map<Potassco::Atom_t, const char*> SymTab;
#if defined(_MSC_VER)
#pragma warning (disable : 4996)
#endif
namespace Potassco {
/////////////////////////////////////////////////////////////////////////////////////////
// SmodelsConvert::SmData
/////////////////////////////////////////////////////////////////////////////////////////
struct SmodelsConvert::SmData {
	struct Atom {
		Atom() : smId(0), head(0), show(0), extn(0) {}
		operator Atom_t() const { return smId; }
		unsigned smId :  28;// corresponding smodels atom
		unsigned head :  1; // atom occurs in a head of a rule
		unsigned show :  1; // atom has a name
		unsigned extn :  2; // value if atom is external
	};
	struct Heuristic {
		Atom_t      atom;
		Heuristic_t type;
		int         bias;
		unsigned    prio;
		unsigned    cond;
	};
	struct Symbol {
		unsigned     atom : 31;
		unsigned     hash :  1;
		const char*  name;
		bool operator<(const Symbol& rhs) const { return atom < rhs.atom; }
	};
	typedef std::vector<Atom>           AtomMap;
	typedef std::vector<Atom_t>         AtomVec;
	typedef std::vector<Lit_t>          LitVec;
	typedef std::vector<WeightLit_t>    WLitVec;
	typedef std::vector<Heuristic>      HeuVec;
	typedef std::map<Weight_t, WLitVec> MinMap;
	typedef std::vector<Symbol>         OutVec;
	SmData() : next_(2) {}
	~SmData() {
		flushStep();
		for (SymTab::iterator it = symTab_.begin(), end = symTab_.end(); it != end; ++it) {
			delete [] it->second;
		}
	}
	Atom_t newAtom()   { return next_++; }
	Atom_t falseAtom() { return 1; }
	bool   mapped(Atom_t a) const {
		return a < atoms_.size() && atoms_[a].smId != 0;
	}
	Atom&  mapAtom(Atom_t a) {
		if (mapped(a)) { return atoms_[a]; }
		if (a >= atoms_.size()) { atoms_.resize(a + 1); }
		atoms_[a].smId = next_++;
		return atoms_[a];
	}
	Lit_t  mapLit(Lit_t in) {
		Lit_t x = static_cast<Lit_t>(mapAtom(atom(in)));
		return in < 0 ? -x : x;
	}
	WeightLit_t mapLit(WeightLit_t in) {
		in.lit = mapLit(in.lit);
		return in;
	}
	Atom_t mapHeadAtom(Atom_t a) {
		Atom& x = mapAtom(a);
		x.head = 1;
		return x;
	}
	AtomSpan mapHead(const AtomSpan& h);
	template <class T>
	Span<T>  mapLits(const Span<T>& in, std::vector<T>& out) {
		out.clear();
		for (typename Span<T>::iterator x = begin(in); x != end(in); ++x) { out.push_back(mapLit(*x)); }
		return toSpan(out);
	}
	const char* addOutput(Atom_t atom, const StringSpan&, bool addHash);
	void addMinimize(Weight_t prio, const WeightLitSpan& lits) {
		WLitVec& body = minimize_[prio];
		body.reserve(body.size() + size(lits));
		for (const WeightLit_t* it = begin(lits); it != end(lits); ++it) {
			WeightLit_t x = *it;
			if (weight(x) < 0) {
				x.lit = -x.lit;
				x.weight = -x.weight;
			}
			body.push_back(x);
		}
	}
	void addExternal(Atom_t a, Value_t v) {
		Atom& ma = mapAtom(a);
		if (!ma.head) {
			ma.extn = static_cast<unsigned>(v);
			extern_.push_back(a);
		}
	}
	void addHeuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, Atom_t cond) {
		Heuristic h = {a, t, bias, prio, cond};
		heuristic_.push_back(h);
	}
	void flushStep() {
		minimize_.clear();
		AtomVec().swap(extern_);
		HeuVec().swap(heuristic_);
		for (; !output_.empty(); output_.pop_back()) {
			if (!output_.back().hash) { delete [] output_.back().name; }
		}
	}
	AtomMap atoms_;    // maps input atoms to output atoms
	MinMap  minimize_; // maps priorities to minimize statements
	AtomVec head_;     // active rule head
	LitVec  lits_;     // active body literals
	WLitVec wlits_;    // active weight body literals
	AtomVec extern_;   // external atoms
	HeuVec  heuristic_;// list of heuristic modifications not yet processed
	SymTab  symTab_;
	OutVec  output_;   // list of output atoms not yet processed
	Atom_t  next_;     // next unused output atom
};
AtomSpan SmodelsConvert::SmData::mapHead(const AtomSpan& h) {
	head_.clear();
	for (const Atom_t* x = begin(h); x != end(h); ++x) {
		head_.push_back(mapHeadAtom(*x));
	}
	if (head_.empty()) { head_.push_back(falseAtom()); }
	return toSpan(head_);
}
const char* SmodelsConvert::SmData::addOutput(Atom_t atom, const StringSpan& str, bool addHash) {
	char* n = new char[str.size + 1];
	*std::copy(begin(str), end(str), n) = 0;
	Symbol s; s.atom = atom; s.name = n; s.hash = 0;
	if (addHash && symTab_.insert(SymTab::value_type(atom, s.name)).second) {
		s.hash = 1;
	}
	output_.push_back(s);
	return s.name;
}
/////////////////////////////////////////////////////////////////////////////////////////
// SmodelsConvert
/////////////////////////////////////////////////////////////////////////////////////////
SmodelsConvert::SmodelsConvert(AbstractProgram& out, bool ext) : out_(out), data_(new SmData), ext_(ext) {}
SmodelsConvert::~SmodelsConvert() {
	delete data_;
}
Lit_t SmodelsConvert::get(Lit_t in) const {
	return data_->mapLit(in);
}
unsigned SmodelsConvert::maxAtom() const {
	return data_->next_ - 1;
}
const char* SmodelsConvert::getName(Atom_t a) const {
	SymTab::iterator it = data_->symTab_.find(a);
	return it != data_->symTab_.end() ? it->second : 0;
}
Atom_t SmodelsConvert::makeAtom(const LitSpan& cond, bool named) {
	Atom_t id = 0;
	if (size(cond) != 1 || cond[0] < 0 || (data_->mapAtom(atom(cond[0])).show && named)) {
		// aux :- cond.
		Atom_t aux = (id = data_->newAtom());
		out_.rule(Head_t::Disjunctive, toSpan(&aux, 1), data_->mapLits(cond, data_->lits_));
	}
	else {
		SmData::Atom& ma = data_->mapAtom(atom(*begin(cond)));
		ma.show = static_cast<unsigned>(named);
		id = ma.smId;
	}
	return id;
}
void SmodelsConvert::initProgram(bool inc) {
	out_.initProgram(inc);
}
void SmodelsConvert::beginStep() {
	out_.beginStep();
}
void SmodelsConvert::rule(Head_t ht, const AtomSpan& head, const LitSpan& body) {
	if (!empty(head) || ht == Head_t::Disjunctive) {
		AtomSpan mHead = data_->mapHead(head);
		out_.rule(ht, mHead, data_->mapLits(body, data_->lits_));
	}
}
void SmodelsConvert::rule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body) {
	if (!empty(head) || ht == Head_t::Disjunctive) {
		AtomSpan      mHead = data_->mapHead(head);
		WeightLitSpan mBody = data_->mapLits(body, data_->wlits_);
		if (isSmodelsRule(ht, mHead, bound, mBody)) {
			out_.rule(ht, mHead, bound, mBody);
			return;
		}
		Atom_t aux = data_->newAtom();
		data_->lits_.assign(1, lit(aux));
		out_.rule(Head_t::Disjunctive, toSpan(&aux, 1), bound, mBody);
		out_.rule(ht, mHead, toSpan(data_->lits_));
	}
}

void SmodelsConvert::minimize(Weight_t prio, const WeightLitSpan& lits) {
	data_->addMinimize(prio, lits);
}
void SmodelsConvert::output(const StringSpan& str, const LitSpan& cond) {
	// create a unique atom for cond and set its name to str
	data_->addOutput(makeAtom(cond, true), str, true);
}

void SmodelsConvert::external(Atom_t a, Value_t v) {
	data_->addExternal(a, v);
}
void SmodelsConvert::heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& cond) {
	if (!ext_) { out_.heuristic(a, t, bias, prio, cond); }
	// create unique atom representing _heuristic(...)
	Atom_t heuPred = makeAtom(cond, true);
	data_->addHeuristic(a, t, bias, prio, heuPred);
}
void SmodelsConvert::acycEdge(int s, int t, const LitSpan& condition) {
	if (!ext_) { out_.acycEdge(s, t, condition); }
	StringBuilder buf;
	buf.appendFormat("_edge(%d,%d)", s, t);
	data_->addOutput(makeAtom(condition, true), toSpan(buf), false);
}

void SmodelsConvert::flush() {
	flushMinimize();
	flushExternal();
	flushHeuristic();
	flushSymbols();
	Lit_t f = -static_cast<Lit_t>(data_->falseAtom());
	out_.assume(toSpan(&f, 1));
	data_->flushStep();
}
void SmodelsConvert::endStep() {
	flush();
	out_.endStep();
}
void SmodelsConvert::flushMinimize() {
	for (SmData::MinMap::iterator it = data_->minimize_.begin(), end = data_->minimize_.end(); it != end; ++it) {
		out_.minimize(it->first, data_->mapLits(toSpan(it->second), data_->wlits_));
	}
}
void SmodelsConvert::flushExternal() {
	LitSpan T = toSpan<Lit_t>();
	data_->head_.clear();
	for (SmData::AtomVec::const_iterator it = data_->extern_.begin(), end = data_->extern_.end(); it != end; ++it) {
		SmData::Atom& a = data_->mapAtom(*it);
		Value_t vt = static_cast<Value_t>(a.extn);
		if (!ext_) {
			if (a.head) { continue; }
			Atom_t at = a;
			if      (vt == Value_t::Free) { data_->head_.push_back(at); }
			else if (vt == Value_t::True) { out_.rule(Head_t::Disjunctive, toSpan(&at, 1), T); }
		}
		else {
			out_.external(a, vt);
		}
	}
	if (!data_->head_.empty()) {
		out_.rule(Head_t::Choice, toSpan(data_->head_), T);
	}
}
void SmodelsConvert::flushHeuristic() {
	StringBuilder buf;
	for (SmData::HeuVec::const_iterator it = data_->heuristic_.begin(), end = data_->heuristic_.end(); it != end; ++it) {
		const SmData::Heuristic& heu = *it;
		if (!data_->mapped(heu.atom)) { continue; }
		SmData::Atom& ma = data_->mapAtom(heu.atom);
		const char* name = ma.show ? getName(ma.smId) : 0;
		if (!name) {
			ma.show = 1;
			buf.clear();
			buf.appendFormat("_atom(%u)", ma.smId);
			name = data_->addOutput(ma, toSpan(buf), true);
		}
		buf.clear();
		buf.appendFormat("_heuristic(%s,%s,%d,%u)", name, toString(heu.type), heu.bias, heu.prio);
		Lit_t c = static_cast<Lit_t>(heu.cond);
		out_.output(toSpan(buf), toSpan(&c, 1));
	}
}
void SmodelsConvert::flushSymbols() {
	std::sort(data_->output_.begin(), data_->output_.end());
	for (SmData::OutVec::const_iterator it = data_->output_.begin(), end = data_->output_.end(); it != end; ++it) {
		Lit_t x = static_cast<Lit_t>(it->atom);
		out_.output(toSpan(it->name, std::strlen(it->name)), toSpan(&x, 1));
	}
}

} // namespace Potassco
