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
#include <potassco/smodels.h>
#include <potassco/rule_utils.h>
#include <ostream>
#include <string>
#include <cstring>
#include <vector>
#include POTASSCO_EXT_INCLUDE(unordered_map)
typedef POTASSCO_EXT_NS::unordered_map<std::string, Potassco::Id_t> StrMap;
namespace Potassco {

enum SmodelsRule {
	End         = 0,
	Basic       = 1, Cardinality = 2, Choice   = 3,
	Generate    = 4, Weight      = 5, Optimize = 6,
	Disjunctive = 8,
	ClaspIncrement = 90, ClaspAssignExt = 91, ClaspReleaseExt = 92
};
int isSmodelsHead(Head_t t, const AtomSpan& head) {
	if (empty(head))         { return End; }
	if (t == Head_t::Choice) { return Choice; }
	return size(head) == 1 ? Basic : Disjunctive;
}

int isSmodelsRule(Head_t t, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body) {
	if (isSmodelsHead(t, head) != Basic || bound < 0) { return End; }
	for (WeightLitSpan::iterator it = begin(body), end = Potassco::end(body); it != end; ++it) {
		if (weight(*it) != 1) { return Weight; }
	}
	return Cardinality;
}
AtomTable::~AtomTable() {}
/////////////////////////////////////////////////////////////////////////////////////////
// SmodelsInput
/////////////////////////////////////////////////////////////////////////////////////////
struct SmodelsInput::SymTab : public AtomTable{
	SymTab(AbstractProgram& o) : out(&o) {}
	virtual void add(Atom_t id, const StringSpan& name, bool output) {
		atoms.insert(StrMap::value_type(std::string(Potassco::begin(name), Potassco::end(name)), id));
		if (output) {
			Lit_t lit = static_cast<Lit_t>(id);
			out->output(name, toSpan(&lit, 1));
		}
	}
	virtual Atom_t find(const StringSpan& name) {
		temp.assign(Potassco::begin(name), Potassco::end(name));
		StrMap::const_iterator it = atoms.find(temp);
		return it != atoms.end() ? it->second : 0;
	}
	struct Heuristic {
		std::string atom;
		Heuristic_t type;
		int         bias;
		unsigned    prio;
		Lit_t       cond;
	};
	StrMap           atoms;
	std::string      temp;
	AbstractProgram* out;
};
struct SmodelsInput::NodeTab {
	Id_t add(const StringSpan& n) {
		return nodes.insert(StrMap::value_type(std::string(begin(n), end(n)), (Id_t)nodes.size())).first->second;
	}
	StrMap nodes;
};
SmodelsInput::SmodelsInput(AbstractProgram& out, const Options& opts, AtomTable* syms) : out_(out), atoms_(syms), nodes_(0), opts_(opts), delSyms_(false) {}
SmodelsInput::~SmodelsInput() { if (delSyms_) delete atoms_; delete nodes_; }
void SmodelsInput::doReset() {}
bool SmodelsInput::doAttach(bool& inc) {
	char n = stream()->peek();
	if (BufferedStream::isDigit(n) && ((inc = (n == '9')) == false || opts_.claspExt)) {
		out_.initProgram(inc);
		return true;
	}
	return false;
}

bool SmodelsInput::doParse() {
	out_.beginStep();
	if (readRules() && readSymbols() && readCompute("B+", true) && readCompute("B-", false) && readExtra()) {
		out_.endStep();
		return true;
	}
	return false;
}

void SmodelsInput::matchBody(RuleBuilder& rule) {
	uint32_t len = matchPos();
	uint32_t neg = matchPos();
	for (rule.startBody(); len--;) {
		Lit_t p = lit(matchAtom());
		if (neg) { p *= -1; --neg; }
		rule.addGoal(p);
	}
}

void SmodelsInput::matchSum(RuleBuilder& rule, bool weights) {
	uint32_t bnd = matchPos();
	uint32_t len = matchPos();
	uint32_t neg = matchPos();
	if (!weights) { std::swap(len, bnd); std::swap(bnd, neg); }
	rule.startSum(bnd);
	for (uint32_t i = 0; i != len; ++i) {
		Lit_t p = lit(matchAtom());
		if (neg) { p *= -1; --neg; }
		rule.addGoal(p, 1);
	}
	if (weights) {
		for (WeightLit_t* x = rule.wlits_begin(), *end = x + len; x != end; ++x) {
			x->weight = (Weight_t)matchPos("non-negative weight expected");
		}
	}
}
bool SmodelsInput::readRules() {
	RuleBuilder rule;
	Weight_t minPrio = 0;
	for (unsigned rt; (rt = matchPos("rule type expected")) != 0;) {
		rule.clear();
		switch (rt) {
			default: return require(false, "unrecognized rule type");
			case Choice: case Disjunctive: // n a1..an
				rule.start(rt == Choice ? Head_t::Choice : Head_t::Disjunctive);
				for (unsigned i = matchAtom("positive head size expected"); i--;) { rule.addHead(matchAtom()); }
				matchBody(rule);
				rule.end(&out_);
				break;
			case Basic:
				rule.start(Head_t::Disjunctive).addHead(matchAtom());
				matchBody(rule);
				rule.end(&out_);
				break;
			case Cardinality: // fall through
			case Weight:      // fall through
				rule.start(Head_t::Disjunctive).addHead(matchAtom());
				matchSum(rule, rt == Weight);
				rule.end(&out_);
				break;
			case Optimize:
				rule.startMinimize(minPrio++);
				matchSum(rule, true);
				rule.end(&out_);
				break;
			case ClaspIncrement:
				require(opts_.claspExt && matchPos() == 0, "unrecognized rule type");
				break;
			case ClaspAssignExt:
			case ClaspReleaseExt:
				require(opts_.claspExt, "unrecognized rule type");
				if (rt == ClaspAssignExt) {
					Atom_t rHead = matchAtom();
					out_.external(rHead, static_cast<Value_t>((matchPos(2, "0..2 expected") ^ 3) - 1));
				}
				else {
					out_.external(matchAtom(), Value_t::Release);
				}
				break;
		}
	}
	return true;
}

bool SmodelsInput::readSymbols() {
	std::string name;
	if (opts_.cEdge && !nodes_) { nodes_ = new NodeTab; }
	if (opts_.cHeuristic && !atoms_) { atoms_ = new SymTab(out_); delSyms_ = true; }
	StringSpan n0, n1;
	SymTab::Heuristic heu;
	std::vector<SymTab::Heuristic> doms;
	for (Lit_t atom; (atom = (Lit_t)matchPos()) != 0;) {
		name.clear();
		stream()->get();
		for (char c; (c = stream()->get()) != '\n';) {
			require(c != 0, "atom name expected!");
			name += c;
		}
		const char* n = name.c_str();
		bool filter = false;
		if (opts_.cEdge && matchEdgePred(n, n0, n1) > 0) {
			Id_t s = nodes_->add(n0);
			Id_t t = nodes_->add(n1);
			out_.acycEdge(static_cast<int>(s), static_cast<int>(t), toSpan(&atom, 1));
			filter = opts_.filter;
		}
		else if (opts_.cHeuristic && matchDomHeuPred(n, n0, heu.type, heu.bias, heu.prio) > 0) {
			heu.cond = atom;
			heu.atom.assign(Potassco::begin(n0), Potassco::end(n0));
			doms.push_back(heu);
			filter = opts_.filter;
		}
		if      (atoms_)  { atoms_->add(atom, toSpan(name), !filter); }
		else if (!filter) { out_.output(toSpan(name), toSpan(&atom, 1)); }
	}
	for (std::vector<SymTab::Heuristic>::const_iterator it = doms.begin(), end = doms.end(); it != end; ++it) {
		if (Atom_t x = atoms_->find(toSpan(it->atom))) {
			out_.heuristic(x, it->type, it->bias, it->prio, toSpan(&it->cond, 1));
		}
	}
	if (!incremental()) {
		delete nodes_;
		if (delSyms_) delete atoms_;
		nodes_ = 0;
		atoms_ = 0;
	}
	return true;
}

bool SmodelsInput::readCompute(const char* comp, bool val) {
	require(match(comp) && stream()->get() == '\n', "compute statement expected");
	for (Lit_t x; (x = (Lit_t)matchPos()) != 0;) {
		if (val) { x = neg(x); }
		out_.rule(Head_t::Disjunctive, toSpan<Atom_t>(), toSpan(&x, 1));
	}
	return true;
}

bool SmodelsInput::readExtra() {
	if (match("E")) {
		for (Atom_t atom; (atom = matchPos()) != 0;) {
			out_.external(atom, Value_t::Free);
		}
	}
	matchPos("number of models expected");
	return true;
}

int readSmodels(std::istream& in, AbstractProgram& out, ErrorHandler err, const SmodelsInput::Options& opts) {
	SmodelsInput reader(out, opts);
	return readProgram(in, reader, err);
}
/////////////////////////////////////////////////////////////////////////////////////////
// SmodelsOutput
/////////////////////////////////////////////////////////////////////////////////////////
namespace {
	struct Atom     { template <class T> Atom_t operator()(T x) const { return atom(x); } };
	struct SmWeight { uint32_t operator()(const WeightLit_t& x) const { return static_cast<unsigned>(x.weight >= 0 ? x.weight : -x.weight); } };
	inline Lit_t smLit(const WeightLit_t& x) { return x.weight >= 0 ? x.lit : -x.lit; }
	inline Lit_t smLit(Lit_t x)              { return x; }
	template <class T>
	static unsigned negSize(const Potassco::Span<T>& lits) {
		unsigned r = 0;
		for (const T* it = Potassco::begin(lits), *end = Potassco::end(lits); it != end; ++it) { r += smLit(*it) < 0; }
		return r;
	}
	template <class T, class Op>
	static void print(std::ostream& os, const Span<T>& span, unsigned neg, unsigned pos, Op op) {
		for (const T* it = begin(span); neg; ++it) { if (smLit(*it)  < 0) { os << " " << op(*it); --neg; } }
		for (const T* it = begin(span); pos; ++it) { if (smLit(*it) >= 0) { os << " " << op(*it); --pos; } }
	}
} // namespace
SmodelsOutput::SmodelsOutput(std::ostream& os, bool ext, Atom_t fAtom) : os_(os), false_(fAtom), sec_(0), ext_(ext), inc_(false), fHead_(false) {}
SmodelsOutput& SmodelsOutput::startRule(int rt) { os_ << rt; return *this; }
SmodelsOutput& SmodelsOutput::add(unsigned i)   { os_ << " " << i; return *this; }
SmodelsOutput& SmodelsOutput::add(Head_t ht, const AtomSpan& head) {
	if (ht == Head_t::Choice || size(head) > 1) { add((unsigned)size(head)); }
	for (const Atom_t* x = begin(head); x != end(head); ++x) { add(*x); }
	return *this;
}

SmodelsOutput& SmodelsOutput::add(const LitSpan& lits) {
	unsigned neg = negSize(lits), size = static_cast<unsigned>(Potassco::size(lits));
	add(size).add(neg);
	print(os_, lits, neg, size - neg, Atom());
	return *this;
}
SmodelsOutput& SmodelsOutput::add(Weight_t bnd, const WeightLitSpan& lits, bool card) {
	unsigned neg = negSize(lits), size = static_cast<unsigned>(Potassco::size(lits));
	if (!card) { add(static_cast<unsigned>(bnd)); }
	add(size).add(neg);
	if (card)  { add(static_cast<unsigned>(bnd)); }
	print(os_, lits, neg, size - neg, Atom());
	if (!card) { print(os_, lits, neg, size - neg, SmWeight()); }
	return *this;
}
SmodelsOutput& SmodelsOutput::endRule() {
	os_ << "\n";
	return *this;
}
void SmodelsOutput::initProgram(bool b) {
	inc_ = b;
	POTASSCO_REQUIRE(!inc_ || ext_, "incremental programs not supported in smodels format");
}
void SmodelsOutput::beginStep() {
	if (ext_ && inc_) { startRule(ClaspIncrement).add(0).endRule(); }
	sec_ = 0;
	fHead_ = false;
}
void SmodelsOutput::rule(Head_t ht, const AtomSpan& head, const LitSpan& body) {
	POTASSCO_REQUIRE(sec_ == 0, "adding rules after symbols not supported");
	if (empty(head)) {
		if (ht == Head_t::Choice) { return; }
		else {
			POTASSCO_REQUIRE(false_ != 0, "empty head requires false atom");
			fHead_ = true;
			return SmodelsOutput::rule(ht, toSpan(&false_, 1), body);
		}
	}
	SmodelsRule rt = (SmodelsRule)isSmodelsHead(ht, head);
	POTASSCO_REQUIRE(rt != End, "unsupported rule type");
	startRule(rt).add(ht, head).add(body).endRule();
}
void SmodelsOutput::rule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body) {
	POTASSCO_REQUIRE(sec_ == 0, "adding rules after symbols not supported");
	if (empty(head)) {
		POTASSCO_REQUIRE(false_ != 0, "empty head requires false atom");
		fHead_ = true;
		return SmodelsOutput::rule(ht, toSpan(&false_, 1), bound, body);
	}
	SmodelsRule rt = (SmodelsRule)isSmodelsRule(ht, head, bound, body);
	POTASSCO_REQUIRE(rt != End, "unsupported rule type");
	startRule(rt).add(ht, head).add(bound, body, rt == Cardinality).endRule();
}
void SmodelsOutput::minimize(Weight_t, const WeightLitSpan& lits) {
	startRule(Optimize).add(0, lits, false).endRule();
}
void SmodelsOutput::output(const StringSpan& str, const LitSpan& cond) {
	POTASSCO_REQUIRE(sec_ <= 1, "adding symbols after compute not supported");
	POTASSCO_REQUIRE(size(cond) == 1 && lit(*begin(cond)) > 0, "general output directive not supported in smodels format");
	if (sec_ == 0) { startRule(End).endRule(); sec_ = 1; }
	os_ << unsigned(cond[0]) << " ";
	os_.write(begin(str), size(str));
	os_ << "\n";
}
void SmodelsOutput::external(Atom_t a, Value_t t) {
	POTASSCO_REQUIRE(ext_, "external directive not supported in smodels format");
	if (t != Value_t::Release) {
		startRule(ClaspAssignExt).add(a).add((unsigned(t)^3)-1).endRule();
	}
	else {
		startRule(ClaspReleaseExt).add(a).endRule();
	}
}
void SmodelsOutput::assume(const LitSpan& lits) {
	POTASSCO_REQUIRE(sec_ < 2, "at most one compute statement supported in smodels format");
	while (sec_ != 2) { startRule(End).endRule(); ++sec_; }
	os_ << "B+\n";
	for (const Lit_t* x = begin(lits); x != end(lits); ++x) {
		if (lit(*x) > 0) { os_ << atom(*x) << "\n"; }
	}
	os_ << "0\nB-\n";
	for (const Lit_t* x = begin(lits); x != end(lits); ++x) {
		if (lit(*x) < 0) { os_ << atom(*x) << "\n"; }
	}
	if (fHead_ && false_) {
		os_ << false_ << "\n";
	}
	os_ << "0\n";
}
void SmodelsOutput::endStep() {
	if (sec_ < 2) { SmodelsOutput::assume(Potassco::toSpan<Lit_t>()); }
	os_ << "1\n";
}
} // namespace Potassco
