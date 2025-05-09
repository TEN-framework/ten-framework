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
#include <potassco/aspif.h>
#include <potassco/theory_data.h>
#include <potassco/rule_utils.h>
#include <ostream>
#include <vector>
#include <string>
#include <cstring>
#include <functional>
#if defined(_MSC_VER)
#pragma warning (disable : 4996)
#endif
namespace Potassco {
/////////////////////////////////////////////////////////////////////////////////////////
// AspifInput
/////////////////////////////////////////////////////////////////////////////////////////
struct AspifInput::Extra {
	std::vector<Id_t> ids;
	std::string       sym;
};
AspifInput::AspifInput(AbstractProgram& out) : out_(out), rule_(0), data_(0) {}
AspifInput::~AspifInput() {  }

bool AspifInput::doAttach(bool& inc) {
	if (!match("asp ")) { return false; }
	require(matchPos() == 1, "unsupported major version");
	require(matchPos() == 0, "unsupported minor version");
	matchPos("revision number expected");
	while (match(" ", false)) { ; }
	inc  = match("incremental", false);
	out_.initProgram(inc);
	return require(stream()->get() == '\n', "invalid extra characters in problem line");
}
bool AspifInput::doParse() {
#define CR(r) Directive_t::r
	RuleBuilder rule;
	Extra data;
	rule_ = &rule;
	data_ = &data;
	out_.beginStep();
	for (unsigned rt; (rt = matchPos(Directive_t::eMax, "rule type or 0 expected")) != 0; rule.clear()) {
		switch (rt) {
			default: return require(false, "unrecognized rule type");
			{case CR(Rule):
				rule.start(static_cast<Head_t>(matchPos(Head_t::eMax, "invalid head type")));
				matchAtoms();
				Body_t  bt = static_cast<Body_t>(matchPos(Body_t::eMax, "invalid body type"));
				if (bt == Body_t::Normal) {
					matchLits();
				}
				else {
					rule.startSum(matchInt());
					matchWLits(0);
				}
				rule.end(&out_);
				break;}
			case CR(Minimize):
				rule.startMinimize(matchInt());
				matchWLits(INT_MIN);
				rule.end(&out_);
				break;
			case CR(Project):
				matchAtoms();
				out_.project(rule.head());
				break;
			{case CR(Output):
				matchString();
				matchLits();
				out_.output(toSpan(data.sym), rule.body());
				break;}
			case CR(External):
				if (Atom_t atom = matchAtom()) {
					Value_t val = static_cast<Value_t>(matchPos(Value_t::eMax, "value expected"));
					out_.external(atom, val);
				}
				break;
			case CR(Assume):
				matchLits();
				out_.assume(rule.body());
				break;
			{case CR(Heuristic):
				Heuristic_t type = static_cast<Heuristic_t>(matchPos(Heuristic_t::eMax, "invalid heuristic modifier"));
				Atom_t      atom = matchAtom();
				int         bias = matchInt();
				unsigned    prio = matchPos(INT_MAX, "invalid heuristic priority");
				matchLits();
				out_.heuristic(atom, type, bias, prio, rule.body());
				break;}
			{case CR(Edge):
				unsigned start = matchPos(INT_MAX, "invalid edge, start node expected");
				unsigned end   = matchPos(INT_MAX, "invalid edge, end node expected");
				matchLits();
				out_.acycEdge((int)start, (int)end, rule.body());
				break;}
			case CR(Theory): matchTheory(matchPos()); break;
			case CR(Comment): skipLine(); break;
		}
	}
#undef CR
	out_.endStep();
	rule_ = 0;
	data_ = 0;
	return true;
}

void AspifInput::matchAtoms() {
	for (uint32_t len = matchPos("number of atoms expected"); len--;) { rule_->addHead(matchAtom()); }
}
void AspifInput::matchLits() {
	rule_->startBody();
	for (uint32_t len = matchPos("number of literals expected"); len--;) { rule_->addGoal(matchLit()); }
}
void AspifInput::matchWLits(int32_t minW) {
	for (uint32_t len = matchPos("number of literals expected"); len--;) { rule_->addGoal(matchWLit(minW)); }
}
void AspifInput::matchString() {
	uint32_t len = matchPos("non-negative string length expected");
	stream()->get();
	data_->sym.resize(len);
	require(stream()->copy(len ? &data_->sym[0] : static_cast<char*>(0), (int)len) == (int)len, "invalid string");
}
void AspifInput::matchIds() {
	uint32_t len = matchPos("number of terms expected");
	data_->ids.resize(len);
	for (uint32_t i = 0; i != len; ++i) {
		data_->ids[i] = matchPos();
	}
}
void AspifInput::matchTheory(unsigned rt) {
	Id_t tId = matchPos();
	switch (rt) {
		default: require(false, "unrecognized theory directive type"); return;
		case Theory_t::Number:
			out_.theoryTerm(tId, matchInt());
			break;
		case Theory_t::Symbol:
			matchString();
			out_.theoryTerm(tId, toSpan(data_->sym));
			break;
		case Theory_t::Compound: {
			int type = matchInt(Tuple_t::eMin, INT_MAX, "unrecognized compound term type");
			matchIds();
			out_.theoryTerm(tId, type, toSpan(data_->ids));
			break;
		}
		case Theory_t::Element: {
			matchIds();
			matchLits();
			out_.theoryElement(tId, toSpan(data_->ids), rule_->body());
			break;
		}
		case Theory_t::Atom: // fall through
		case Theory_t::AtomWithGuard: {
			Id_t termId = matchPos();
			matchIds();
			if (rt == Theory_t::Atom) {
				out_.theoryAtom(tId, termId, toSpan(data_->ids));
			}
			else {
				Id_t opId = matchPos();
				out_.theoryAtom(tId, termId, toSpan(data_->ids), opId, matchPos());
			}
			break;
		}
	}
}

int readAspif(std::istream& in, AbstractProgram& out, ErrorHandler err) {
	AspifInput reader(out);
	return readProgram(in, reader, err);
}
/////////////////////////////////////////////////////////////////////////////////////////
// AspifOutput
/////////////////////////////////////////////////////////////////////////////////////////
AspifOutput::AspifOutput(std::ostream& os) : os_(os) {
}

AspifOutput& AspifOutput::startDir(Directive_t r) {
	os_ << static_cast<unsigned>(r);
	return *this;
}
AspifOutput& AspifOutput::add(int x) {
	os_ << " " << x;
	return *this;
}
AspifOutput& AspifOutput::add(const WeightLitSpan& lits) {
	os_ << " " << size(lits);
	for (const WeightLit_t* x = begin(lits); x != end(lits); ++x) {
		os_ << " " << lit(*x) << " " << weight(*x);
	}
	return *this;
}
AspifOutput& AspifOutput::add(const LitSpan& lits) {
	os_ << " " << size(lits);
	for (const Lit_t* x = begin(lits); x != end(lits); ++x) {
		os_ << " " << lit(*x);
	}
	return *this;
}
AspifOutput& AspifOutput::add(const AtomSpan& atoms) {
	os_ << " " << size(atoms);
	for (const Atom_t* x = begin(atoms); x != end(atoms); ++x) { os_ << " " << *x; }
	return *this;
}
AspifOutput& AspifOutput::add(const StringSpan& str) {
	os_ << " " << size(str) << " ";
	os_.write(begin(str), size(str));
	return *this;
}
AspifOutput& AspifOutput::endDir() {
	os_ << "\n";
	return *this;
}
void AspifOutput::initProgram(bool inc) {
	os_ << "asp 1 0 0";
	if (inc) os_ << " incremental";
	os_ << "\n";
}
void AspifOutput::rule(Head_t ht, const AtomSpan& head, const LitSpan& body) {
	startDir(Directive_t::Rule).add(static_cast<int>(ht)).add(head)
		.add(static_cast<int>(Body_t::Normal)).add(body)
		.endDir();
}
void AspifOutput::rule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body) {
	startDir(Directive_t::Rule).add(static_cast<int>(ht)).add(head)
		.add(static_cast<int>(Body_t::Sum)).add(static_cast<int>(bound)).add(body)
		.endDir();
}
void AspifOutput::minimize(Weight_t prio, const WeightLitSpan& lits) {
	startDir(Directive_t::Minimize).add(prio).add(lits).endDir();
}
void AspifOutput::output(const StringSpan& str, const LitSpan& cond) {
	startDir(Directive_t::Output).add(str).add(cond).endDir();
}
void AspifOutput::external(Atom_t a, Value_t v) {
	startDir(Directive_t::External).add(static_cast<int>(a)).add(static_cast<int>(v)).endDir();
}
void AspifOutput::assume(const LitSpan& lits) {
	startDir(Directive_t::Assume).add(lits).endDir();
}
void AspifOutput::project(const AtomSpan& atoms) {
	startDir(Directive_t::Project).add(atoms).endDir();
}
void AspifOutput::acycEdge(int s, int t, const LitSpan& cond) {
	startDir(Directive_t::Edge).add(s).add(t).add(cond).endDir();
}
void AspifOutput::heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& cond) {
	startDir(Directive_t::Heuristic).add(static_cast<int>(t)).add(static_cast<int>(a))
		.add(bias).add(static_cast<int>(prio)).add(cond)
		.endDir();
}
void AspifOutput::theoryTerm(Id_t termId, int number) {
	startDir(Directive_t::Theory).add(Theory_t::Number).add(termId).add(number).endDir();
}
void AspifOutput::theoryTerm(Id_t termId, const StringSpan& name) {
	startDir(Directive_t::Theory).add(Theory_t::Symbol).add(termId).add(name).endDir();
}
void AspifOutput::theoryTerm(Id_t termId, int cId, const IdSpan& args) {
	startDir(Directive_t::Theory).add(Theory_t::Compound).add(termId).add(cId).add(args).endDir();
}
void AspifOutput::theoryElement(Id_t elementId, const IdSpan& terms, const LitSpan& cond) {
	startDir(Directive_t::Theory).add(Theory_t::Element).add(elementId).add(terms).add(cond).endDir();
}
void AspifOutput::theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements) {
	startDir(Directive_t::Theory).add(Theory_t::Atom).add(atomOrZero).add(termId).add(elements).endDir();
}
void AspifOutput::theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements, Id_t op, Id_t rhs) {
	startDir(Directive_t::Theory).add(Theory_t::AtomWithGuard).add(atomOrZero).add(termId).add(elements).add(op).add(rhs).endDir();
}

void AspifOutput::beginStep() {
}
void AspifOutput::endStep() {
	os_ << "0\n";
}
} // namespace Potassco
