//
// Copyright (c) 2016-2017 Benjamin Kaufmann
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
#include <potassco/aspif_text.h>
#include <potassco/string_convert.h>
#include <potassco/rule_utils.h>
#include <cctype>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>
#include <cassert>
namespace Potassco {
struct AspifTextInput::Data {
	void clear() { rule.clear(); symbol.clear(); }
	AtomSpan atoms() const { return rule.head(); }
	LitSpan  lits()  const { return rule.body(); }
	RuleBuilder rule;
	std::string symbol;
};
AspifTextInput::AspifTextInput(AbstractProgram* out) : out_(out), data_(0) {}
bool AspifTextInput::doAttach(bool& inc) {
	char n = peek(true);
	if (out_ && (!n || std::islower(static_cast<unsigned char>(n)) || std::strchr(".#%{:", n))) {
		while (n == '%') {
			skipLine();
			n = peek(true);
		}
		inc = match("#incremental", false) && require(match("."), "unrecognized directive");
		out_->initProgram(inc);
		return true;
	}
	return false;
}

bool AspifTextInput::doParse() {
	out_->beginStep();
	if (!parseStatements()) { return false; }
	out_->endStep();
	return true;
}

bool AspifTextInput::parseStatements() {
	require(out_ != 0, "output not set");
	Data data;
	data_ = &data;
	for (char c; (c = peek(true)) != 0; data.clear()) {
		if      (c == '.') { match("."); }
		else if (c == '#') { if (!matchDirective()) break; }
		else if (c == '%') { skipLine(); }
		else               { matchRule(c); }
	}
	return true;
}

void AspifTextInput::matchRule(char c) {
	if (c == '{') { match("{"); data_->rule.start(Head_t::Choice); matchAtoms(";,"); match("}"); }
	else          { data_->rule.start(); matchAtoms(";|"); }
	if (match(":-", false)) {
		c = peek(true);
		if (!StreamType::isDigit(c) && c != '-') {
			data_->rule.startBody();
			matchLits();
		}
		else {
			data_->rule.startSum(matchInt());
			matchAgg();
		}
	}
	match(".");
	data_->rule.end(out_);
}

bool AspifTextInput::matchDirective() {
	if (match("#minimize", false)) {
		data_->rule.startMinimize(0);
		matchAgg();
		Weight_t prio = match("@", false) ? matchInt() : 0;
		match(".");
		data_->rule.setBound(prio);
		data_->rule.end(out_);
	}
	else if (match("#project", false)) {
		data_->rule.start();
		if (match("{", false)) {
			matchAtoms(",");
			match("}");
		}
		match(".");
		out_->project(data_->atoms());
	}
	else if (match("#output", false)) {
		matchTerm();
		matchCondition();
		match(".");
		out_->output(toSpan(data_->symbol), data_->lits());
	}
	else if (match("#external", false)) {
		Atom_t  a = matchId();
		Value_t v = Value_t::False;
		match(".");
		if (match("[", false)) {
			if      (match("true", false))    { v = Value_t::True; }
			else if (match("free", false))    { v = Value_t::Free; }
			else if (match("release", false)) { v = Value_t::Release; }
			else                              { match("false"); }
			match("]");
		}
		out_->external(a, v);
	}
	else if (match("#assume", false)) {
		data_->rule.startBody();
		if (match("{", false)) {
			matchLits();
			match("}");
		}
		match(".");
		out_->assume(data_->lits());
	}
	else if (match("#heuristic", false)) {
		Atom_t a = matchId();
		matchCondition();
		match(".");
		match("[");
		int v = matchInt();
		int p = 0;
		if (match("@", false)) { p = matchInt(); require(p >= 0, "positive priority expected"); }
		match(",");
		int h = -1;
		for (unsigned x = 0; x <= static_cast<unsigned>(Heuristic_t::eMax); ++x) {
			if (match(toString(static_cast<Heuristic_t>(x)), false)) {
				h = static_cast<int>(x);
				break;
			}
		}
		require(h >= 0, "unrecognized heuristic modification");
		skipws();
		match("]");
		out_->heuristic(a, static_cast<Heuristic_t>(h), v, static_cast<unsigned>(p), data_->lits());
	}
	else if (match("#edge", false)) {
		int s, t;
		match("("), s = matchInt(), match(","), t = matchInt(), match(")");
		matchCondition();
		match(".");
		out_->acycEdge(s, t, data_->lits());
	}
	else if (match("#step", false)) {
		require(incremental(), "#step requires incremental program");
		match(".");
		return false;
	}
	else if (match("#incremental", false)) {
		match(".");
	}
	else {
		require(false, "unrecognized directive");
	}
	return true;
}

void AspifTextInput::skipws() {
	stream()->skipWs();
}
bool AspifTextInput::match(const char* term, bool req) {
	if (ProgramReader::match(term, false)) { skipws(); return true; }
	require(!req, POTASSCO_FORMAT("'%s' expected", term));
	return false;
}
void AspifTextInput::matchAtoms(const char* seps) {
	if (std::islower(static_cast<unsigned char>(peek(true))) != 0) {
		do {
			Lit_t x = matchLit();
			require(x > 0, "positive atom expected");
			data_->rule.addHead(static_cast<Atom_t>(x));
		} while (std::strchr(seps, stream()->peek()) && stream()->get() && (skipws(), true));
	}
}
void AspifTextInput::matchLits() {
	if (std::islower(static_cast<unsigned char>(peek(true))) != 0) {
		do {
			data_->rule.addGoal(matchLit());
		} while (match(",", false));
	}
}
void AspifTextInput::matchCondition() {
	data_->rule.startBody();
	if (match(":", false)) { matchLits(); }
}
void AspifTextInput::matchAgg() {
	if (match("{") && !match("}", false)) {
		do {
			WeightLit_t wl = {matchLit(), 1};
			if (match("=", false)) { wl.weight = matchInt(); }
			data_->rule.addGoal(wl);
		}
		while (match(",", false));
		match("}");
	}
}

Lit_t AspifTextInput::matchLit() {
	int s = match("not ", false) ? -1 : 1;
	return static_cast<Lit_t>(matchId()) * s;
}

int AspifTextInput::matchInt() {
	int i = ProgramReader::matchInt();
	skipws();
	return i;
}
Atom_t AspifTextInput::matchId() {
	char c = stream()->get();
	char n = stream()->peek();
	require(std::islower(static_cast<unsigned char>(c)) != 0, "<id> expected");
	require(std::islower(static_cast<unsigned char>(n)) == 0, "<pos-integer> expected");
	if (c == 'x' && (BufferedStream::isDigit(n) || n == '_')) {
		if (n == '_') { stream()->get(); }
		int i = matchInt();
		require(i > 0, "<pos-integer> expected");
		return static_cast<Atom_t>(i);
	}
	else {
		skipws();
		return static_cast<Atom_t>(c - 'a') + 1;
	}
}
void AspifTextInput::push(char c) {
	data_->symbol.append(1, c);
}

void AspifTextInput::matchTerm() {
	char c = stream()->peek();
	if (std::islower(static_cast<unsigned char>(c)) != 0 || c == '_') {
		do { push(stream()->get()); } while (std::isalnum(static_cast<unsigned char>(c = stream()->peek())) != 0 || c == '_');
		skipws();
		if (match("(", false)) {
			push('(');
			for (;;) {
				matchAtomArg();
				if (!match(",", false)) break;
				push(',');
			}
			match(")");
			push(')');
		}
	}
	else if (c == '"') { matchStr(); }
	else { require(false, "<term> expected"); }
	skipws();
}
void AspifTextInput::matchAtomArg() {
	char c;
	for (int p = 0; (c = stream()->peek()) != 0; ) {
		if (c == '"') {
			matchStr();
		}
		else {
			if      (c == ')' && --p < 0) { break; }
			else if (c == ',' &&  p == 0) { break; }
			p += int(c == '(');
			push(stream()->get());
			skipws();
		}
	}
}
void AspifTextInput::matchStr() {
	match("\""), push('"');
	bool quoted = false;
	for (char c; (c = stream()->peek()) != 0 && (c != '\"' || quoted);) {
		quoted = !quoted && c == '\\';
		push(stream()->get());
	}
	match("\""), push('"');
}
/////////////////////////////////////////////////////////////////////////////////////////
// AspifTextOutput
/////////////////////////////////////////////////////////////////////////////////////////
struct AspifTextOutput::Data {
	typedef std::vector<std::string> StringVec;
	typedef std::vector<Id_t> AtomMap;
	typedef std::vector<Lit_t> LitVec;
	typedef std::vector<uint32_t> RawVec;
	LitSpan getCondition(Id_t id) const {
		return toSpan(&conditions[id + 1], static_cast<size_t>(conditions[id]));
	}
	Id_t addCondition(const LitSpan& cond) {
		if (conditions.empty()) { conditions.push_back(0); }
		if (empty(cond)) { return 0; }
		Id_t id = static_cast<Id_t>(conditions.size());
		conditions.push_back(static_cast<Lit_t>(size(cond)));
		conditions.insert(conditions.end(), begin(cond), end(cond));
		return id;
	}
	Id_t addString(const StringSpan& str) {
		Id_t id = static_cast<Id_t>(strings.size());
		strings.push_back(std::string(Potassco::begin(str), Potassco::end(str)));
		return id;
	}
	RawVec    directives;
	StringVec strings;
	AtomMap   atoms; // maps into strings
	LitVec    conditions;
	uint32_t  readPos;
	void reset() { directives.clear();  strings.clear(); atoms.clear(); conditions.clear(); readPos = 0; }
};
AspifTextOutput::AspifTextOutput(std::ostream& os) : os_(os), step_(-1) {
	data_ = new Data();
}
AspifTextOutput::~AspifTextOutput() {
	delete data_;
}
void AspifTextOutput::addAtom(Atom_t id, const StringSpan& str) {
	if (id >= data_->atoms.size()) { data_->atoms.resize(id + 1, idMax); }
	data_->atoms[id] = data_->addString(str);
}
std::ostream& AspifTextOutput::printName(std::ostream& os, Lit_t lit) const {
	if (lit < 0) { os << "not "; }
	Atom_t id = Potassco::atom(lit);
	if (id < data_->atoms.size() && data_->atoms[id] < data_->strings.size()) {
		os << data_->strings[data_->atoms[id]];
	}
	else {
		os << "x_" << id;
	}
	return os;
}
void AspifTextOutput::initProgram(bool incremental) {
	step_ = incremental ? 0 : -1;
	data_->reset();
}
void AspifTextOutput::beginStep() {
	if (step_ >= 0) {
		if (step_) { os_ << "% #program step(" << step_ << ").\n"; theory_.update(); }
		else       { os_ << "% #program base.\n"; }
		++step_;
	}
}
void AspifTextOutput::rule(Head_t ht, const AtomSpan& head, const LitSpan& body) {
	push(Directive_t::Rule).push(static_cast<uint32_t>(ht)).push(head).push(Body_t::Normal).push(body);
}
void AspifTextOutput::rule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& lits) {
	if (size(lits) == 0) {
		AspifTextOutput::rule(ht, head, toSpan<Lit_t>());
	}
	push(Directive_t::Rule).push(static_cast<uint32_t>(ht)).push(head);
	uint32_t top = static_cast<uint32_t>(data_->directives.size());
	Weight_t min = weight(*begin(lits)), max = min;
	push(Body_t::Sum).push(bound).push(static_cast<uint32_t>(size(lits)));
	for (const WeightLit_t* it = begin(lits), *end = Potassco::end(lits); it != end; ++it) {
		push(Potassco::lit(*it)).push(Potassco::weight(*it));
		if (Potassco::weight(*it) < min) { min = Potassco::weight(*it); }
		if (Potassco::weight(*it) > max) { max = Potassco::weight(*it); }
	}
	if (min == max) {
		data_->directives.resize(top);
		bound = (bound + min-1)/min;
		push(Body_t::Count).push(bound).push(static_cast<uint32_t>(size(lits)));
		for (const WeightLit_t* it = begin(lits), *end = Potassco::end(lits); it != end; ++it) {
			push(Potassco::lit(*it));
		}
	}
}
void AspifTextOutput::minimize(Weight_t prio, const WeightLitSpan& lits) {
	push(Directive_t::Minimize).push(lits).push(prio);
}
void AspifTextOutput::output(const StringSpan& str, const LitSpan& cond) {
	bool isAtom = size(str) > 0 && (std::islower(static_cast<unsigned char>(*begin(str))) || *begin(str) == '_');
	if (size(cond) == 1 && lit(*begin(cond)) > 0 && isAtom) {
		addAtom(Potassco::atom(*begin(cond)), str);
	}
	else {
		push(Directive_t::Output).push(data_->addString(str)).push(cond);
	}
}
void AspifTextOutput::external(Atom_t a, Value_t v) {
	push(Directive_t::External).push(a).push(static_cast<uint32_t>(v));
}
void AspifTextOutput::assume(const LitSpan& lits) {
	push(Directive_t::Assume).push(lits);
}
void AspifTextOutput::project(const AtomSpan& atoms) {
	push(Directive_t::Project).push(atoms);
}
void AspifTextOutput::acycEdge(int s, int t, const LitSpan& condition) {
	push(Directive_t::Edge).push(s).push(t).push(condition);
}
void AspifTextOutput::heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& condition) {
	push(Directive_t::Heuristic).push(a).push(condition).push(bias).push(prio).push(static_cast<uint32_t>(t));
}
void AspifTextOutput::theoryTerm(Id_t termId, int number) {
	theory_.addTerm(termId, number);
}
void AspifTextOutput::theoryTerm(Id_t termId, const StringSpan& name) {
	theory_.addTerm(termId, name);
}
void AspifTextOutput::theoryTerm(Id_t termId, int compound, const IdSpan& args) {
	theory_.addTerm(termId, compound, args);
}
void AspifTextOutput::theoryElement(Id_t id, const IdSpan& terms, const LitSpan& cond) {
	theory_.addElement(id, terms, data_->addCondition(cond));
}
void AspifTextOutput::theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements) {
	theory_.addAtom(atomOrZero, termId, elements);
}
void AspifTextOutput::theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements, Id_t op, Id_t rhs) {
	theory_.addAtom(atomOrZero, termId, elements, op, rhs);
}
template <class T>
T AspifTextOutput::get() {
	return static_cast<T>(data_->directives[data_->readPos++]);
}
AspifTextOutput& AspifTextOutput::push(uint32_t x) {
	data_->directives.push_back(x);
	return *this;
}
AspifTextOutput& AspifTextOutput::push(const AtomSpan& atoms) {
	data_->directives.push_back(static_cast<uint32_t>(size(atoms)));
	data_->directives.insert(data_->directives.end(), begin(atoms), end(atoms));
	return *this;
}
AspifTextOutput& AspifTextOutput::push(const LitSpan& lits) {
	data_->directives.push_back(static_cast<uint32_t>(size(lits)));
	data_->directives.insert(data_->directives.end(), begin(lits), end(lits));
	return *this;
}
AspifTextOutput& AspifTextOutput::push(const WeightLitSpan& wlits) {
	data_->directives.reserve(data_->directives.size() + (2*size(wlits)));
	data_->directives.push_back(static_cast<uint32_t>(size(wlits)));
	for (WeightLitSpan::iterator it = begin(wlits), end = Potassco::end(wlits); it != end; ++it) {
		data_->directives.push_back(static_cast<uint32_t>(lit(*it)));
		data_->directives.push_back(static_cast<uint32_t>(weight(*it)));
	}
	return *this;
}
void AspifTextOutput::writeDirectives() {
	const char* sep = 0, *term = 0;
	data_->readPos = 0;
	for (uint32_t x; (x = get<uint32_t>()) != Directive_t::End;) {
		sep = term = "";
		switch (x) {
			case Directive_t::Rule:
				if (get<uint32_t>() != 0) { os_ << "{"; term = "}"; }
				for (uint32_t n = get<uint32_t>(); n--; sep = !*term ? "|" : ";") { printName(os_ << sep, get<Atom_t>()); }
				if (*sep) { os_ << term; sep = " :- "; }
				else      { os_ << ":- "; }
				term = ".";
				switch (uint32_t bt = get<uint32_t>()) {
					case Body_t::Normal:
						for (uint32_t n = get<uint32_t>(); n--; sep = ", ") { printName(os_ << sep, get<Lit_t>()); }
						break;
					case Body_t::Count: // fall through
					case Body_t::Sum:
						os_ << sep << get<Weight_t>();
						sep = "{";
						for (uint32_t n = get<uint32_t>(); n--; sep = "; ") {
							printName(os_ << sep, get<Lit_t>());
							if (bt == Body_t::Sum) { os_ << "=" << get<Weight_t>(); }
						}
						os_ << "}";
						break;
				}
				break;
			case Directive_t::Minimize:
				sep = "#minimize{"; term = ".";
				for (uint32_t n = get<uint32_t>(); n--; sep = "; ") {
					printName(os_ << sep, get<Lit_t>());
					os_ << "=" << get<Weight_t>();
				}
				os_ << "}@" << get<Weight_t>();
				break;
			case Directive_t::Project:
				sep = "#project{"; term = "}.";
				for (uint32_t n = get<uint32_t>(); n--; sep = ", ") { printName(os_ << sep, get<Lit_t>()); }
				break;
			case Directive_t::Output:
				sep = " : "; term = ".";
				os_ << "#show " << data_->strings[get<uint32_t>()];
				for (uint32_t n = get<uint32_t>(); n--; sep = ", ") {
					printName(os_ << sep, get<Lit_t>());
				}
				break;
			case Directive_t::External:
				sep = "#external "; term = ".";
				printName(os_ << sep, get<Atom_t>());
				switch (get<uint32_t>()) {
					default: break;
					case Value_t::Free:    term = ". [free]"; break;
					case Value_t::True:    term = ". [true]"; break;
					case Value_t::Release: term = ". [release]"; break;
				}
				break;
			case Directive_t::Assume:
				sep = "#assume{"; term = "}.";
				for (uint32_t n = get<uint32_t>(); n--; sep = ", ") { printName(os_ << sep, get<Lit_t>()); }
				break;
			case Directive_t::Heuristic:
				sep = " : "; term = "";
				os_ << "#heuristic ";
				printName(os_, get<Atom_t>());
				for (uint32_t n = get<uint32_t>(); n--; sep = ", ") { printName(os_ << sep, get<Lit_t>()); }
				os_ << ". [" << get<int32_t>();
				if (uint32_t p = get<uint32_t>()) { os_ << "@" << p; }
				os_ << ", " << toString(static_cast<Heuristic_t>(get<uint32_t>())) << "]";
				break;
			case Directive_t::Edge:
				sep = " : "; term = ".";
				os_ << "#edge(" << get<int32_t>() << ",";
				os_ << get<int32_t>() << ")";
				for (uint32_t n = get<uint32_t>(); n--; sep = ", ") { printName(os_ << sep, get<Lit_t>()); }
				break;
			default: break;
		}
		os_ << term << "\n";
	}
}
void AspifTextOutput::visitTheories() {
	struct BuildStr : public TheoryAtomStringBuilder {
		explicit BuildStr(AspifTextOutput& s) : self(&s) {}
		virtual LitSpan getCondition(Id_t condId) const {
			return self->data_->getCondition(condId);
		}
		virtual std::string getName(Atom_t id) const {
			if (id < self->data_->atoms.size() && self->data_->atoms[id] < self->data_->strings.size()) {
				return self->data_->strings[self->data_->atoms[id]];
			}
			return std::string("x_").append(Potassco::toString(id));
		}
		AspifTextOutput* self;
	} toStr(*this);
	for (TheoryData::atom_iterator it = theory_.currBegin(), end = theory_.end(); it != end; ++it) {
		Atom_t atom = (*it)->atom();
		std::string name = toStr.toString(theory_, **it);
		if (!atom) {
			os_ << name << ".\n";
		}
		else {
			POTASSCO_REQUIRE(atom >= data_->atoms.size() || data_->atoms[atom] == idMax,
				"Redefinition: theory atom '%u' already shown as '%s'", atom, data_->strings[data_->atoms[atom]].c_str());
			addAtom(atom, toSpan(name));
		}
	}
}
void AspifTextOutput::endStep() {
	visitTheories();
	push(Directive_t::End);
	writeDirectives();
	Data::RawVec().swap(data_->directives);
	if (step_ < 0) { theory_.reset(); }
}
/////////////////////////////////////////////////////////////////////////////////////////
// TheoryAtomStringBuilder
/////////////////////////////////////////////////////////////////////////////////////////
std::string TheoryAtomStringBuilder::toString(const TheoryData& td, const TheoryAtom& a) {
	res_.clear();
	add('&').term(td, td.getTerm(a.term())).add('{');
	const char* sep = "";
	for (TheoryElement::iterator eIt = a.begin(), eEnd = a.end(); eIt != eEnd; ++eIt, sep = "; ") {
		add(sep).element(td, td.getElement(*eIt));
	}
	add('}');
	if (a.guard()) {
		add(' ').term(td, td.getTerm(*a.guard()));
	}
	if (a.rhs()) {
		add(' ').term(td, td.getTerm(*a.rhs()));
	}
	return res_;
}
bool TheoryAtomStringBuilder::function(const TheoryData& td, const TheoryTerm& f) {
	TheoryTerm x = td.getTerm(f.function());
	if (x.type() == Theory_t::Symbol && std::strchr("/!<=>+-*\\?&@|:;~^.", *x.symbol()) != 0) {
		if (f.size() == 1) {
			term(td, x).term(td, td.getTerm(*f.begin()));
			return false;
		}
		else if (f.size() == 2) {
			term(td, td.getTerm(*f.begin())).add(' ').term(td, x).add(' ').term(td, td.getTerm(*(f.begin() + 1)));
			return false;
		}
	}
	term(td, x);
	return true;
}
TheoryAtomStringBuilder& TheoryAtomStringBuilder::term(const TheoryData& data, const TheoryTerm& t) {
	switch (t.type()) {
		default: assert(false);
		case Theory_t::Number: add(Potassco::toString(t.number())); break;
		case Theory_t::Symbol: add(t.symbol()); break;
		case Theory_t::Compound: {
			if (!t.isFunction() || function(data, t)) {
				const char* parens = Potassco::toString(t.isTuple() ? t.tuple() : Potassco::Tuple_t::Paren);
				const char* sep = "";
				add(parens[0]);
				for (TheoryTerm::iterator it = t.begin(), end = t.end(); it != end; ++it, sep = ", ") {
					add(sep).term(data, data.getTerm(*it));
				}
				add(parens[1]);
			}
		}
	}
	return *this;
}
TheoryAtomStringBuilder& TheoryAtomStringBuilder::element(const TheoryData& data, const TheoryElement& e) {
	const char* sep = "";
	for (TheoryElement::iterator it = e.begin(), end = e.end(); it != end; ++it, sep = ", ") {
		add(sep).term(data, data.getTerm(*it));
	}
	if (e.condition()) {
		LitSpan cond = getCondition(e.condition());
		sep = " : ";
		for (const Lit_t* it = begin(cond), *end = Potassco::end(cond); it != end; ++it, sep = ", ") {
			add(sep);
			if (*it < 0) { add("not "); }
			add(getName(atom(*it)));
		}
	}
	return *this;
}
} // namespace Potassco
