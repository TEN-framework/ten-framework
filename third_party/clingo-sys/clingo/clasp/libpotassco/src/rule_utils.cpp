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
#include <potassco/rule_utils.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>
namespace Potassco {
Rule_t Rule_t::normal(Head_t ht, const AtomSpan& head, const LitSpan& body) {
	Rule_t r = {ht, head, Body_t::Normal, {body}};
	return r;
}
Rule_t Rule_t::sum(Head_t ht, const AtomSpan& head, const Sum_t& sum) {
	Rule_t r = {ht, head, Body_t::Sum, {}};
	r.agg = sum;
	return r;
}
Rule_t Rule_t::sum(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& lits) {
	Sum_t s = {lits, bound};
	return sum(ht, head, s);
}
/////////////////////////////////////////////////////////////////////////////////////////
// RuleBuilder
/////////////////////////////////////////////////////////////////////////////////////////
struct RuleBuilder::Rule {
	Rule() { head.init(0, 0); body.init(0, 0); top = sizeof(Rule); fix = 0; }
	struct Span {
		void init(uint32_t p, uint32_t t) { mbeg = mend = p; type = t; }
		uint32_t mbeg:30;
		uint32_t type: 2;
		uint32_t mend;
		uint32_t len() const { return mend - mbeg; }
	};
	uint32_t top:31;
	uint32_t fix: 1;
	Span     head;
	Span     body;
};
namespace {
template <class T>
inline Potassco::Span<T> span_cast(const MemoryRegion& m, const RuleBuilder::Rule::Span& in) {
	return Potassco::toSpan(static_cast<T*>(m[in.mbeg]), in.len() / sizeof(T));
}
template <class T>
inline RuleBuilder::Rule* push(MemoryRegion& m, RuleBuilder::Rule* r, const T& what) {
	assert(r == m.begin());
	uint32_t t = r->top, nt = t + sizeof(T);
	if (nt > m.size()) {
		m.grow(nt);
		r = static_cast<RuleBuilder::Rule*>(m.begin());
	}
	new (m[t]) T(what);
	r->top = nt;
	return r;
}
}
RuleBuilder::RuleBuilder() : mem_(64) {
	new (mem_.begin()) Rule();
}
RuleBuilder::Rule* RuleBuilder::rule_() const {
	return static_cast<Rule*>(mem_.begin());
}
RuleBuilder::RuleBuilder(const RuleBuilder& other) {
	mem_.grow(other.rule_()->top);
	std::memcpy(mem_.begin(), other.mem_.begin(), other.rule_()->top);
}
RuleBuilder& RuleBuilder::operator=(const RuleBuilder& other) {
	RuleBuilder(other).swap(*this);
	return *this;
}
RuleBuilder::~RuleBuilder() { mem_.release(); }
void RuleBuilder::swap(RuleBuilder& other) {
	mem_.swap(other.mem_);
}
RuleBuilder& RuleBuilder::clear() {
	new (mem_.begin()) Rule();
	return *this;
}
RuleBuilder::Rule* RuleBuilder::unfreeze(bool discard) {
	Rule* r = rule_();
	if (r->fix) {
		if (!discard) { r->fix = 0; }
		else          { clear(); }
	}
	return r;
}
/////////////////////////////////////////////////////////////////////////////////////////
// RuleBuilder - Head management
/////////////////////////////////////////////////////////////////////////////////////////
RuleBuilder& RuleBuilder::start(Head_t ht) {
	Rule* r = unfreeze(true);
	Rule::Span& h = r->head;
	POTASSCO_ASSERT(!h.mbeg || h.len() == 0u, "Invalid second call to start()");
	h.init(r->top, ht);
	return *this;
}
RuleBuilder& RuleBuilder::addHead(Atom_t a) {
	Rule* r = rule_();
	POTASSCO_ASSERT(!r->fix, "Invalid call to addHead() on frozen rule");
	if (!r->head.mend) { r->head.init(r->top, 0); }
	POTASSCO_ASSERT(r->head.mbeg >= r->body.mend, "Invalid call to addHead() after startBody()");
	r = push(mem_, r, a);
	r->head.mend = r->top;
	return *this;
}
RuleBuilder& RuleBuilder::clearHead() {
	Rule* r = unfreeze(false);
	r->top = std::max(r->body.mend, static_cast<uint32_t>(sizeof(Rule)));
	r->head.init(0, 0);
	return *this;
}
AtomSpan RuleBuilder::head()       const { return span_cast<Atom_t>(mem_, rule_()->head); }
Atom_t*  RuleBuilder::head_begin() const { return static_cast<Atom_t*>(mem_[rule_()->head.mbeg]); }
Atom_t*  RuleBuilder::head_end()   const { return static_cast<Atom_t*>(mem_[rule_()->head.mend]); }
/////////////////////////////////////////////////////////////////////////////////////////
// RuleBuilder - Body management
/////////////////////////////////////////////////////////////////////////////////////////
void RuleBuilder::startBody(Body_t bt, Weight_t bnd) {
	Rule* r = unfreeze(true);
	if (!r->body.mend) {
		if (bt != Body_t::Normal) { r = push(mem_, r, bnd); }
		r->body.init(r->top, bt);
	}
	else {
		POTASSCO_ASSERT(r->body.len() == 0, "Invalid second call to startBody()");
	}
}
RuleBuilder& RuleBuilder::startBody() { startBody(Body_t::Normal, -1); return *this; }
RuleBuilder& RuleBuilder::startSum(Weight_t bound) { startBody(Body_t::Sum, bound); return *this; }
RuleBuilder& RuleBuilder::startMinimize(Weight_t prio) {
	Rule* r = unfreeze(true);
	POTASSCO_ASSERT(!r->head.mbeg && !r->body.mbeg, "Invalid call to startMinimize()");
	r->head.init(r->top, Directive_t::Minimize);
	r = push(mem_, r, prio);
	r->body.init(r->top, Body_t::Sum);
	return *this;
}
RuleBuilder& RuleBuilder::addGoal(WeightLit_t lit) {
	Rule* r = rule_();
	POTASSCO_ASSERT(!r->fix, "Invalid call to addGoal() on frozen rule");
	if (!r->body.mbeg) { r->body.init(r->top, 0); }
	POTASSCO_ASSERT(r->body.mbeg >= r->head.mend, "Invalid call to addGoal() after start()");
	if (lit.weight == 0) { return *this; }
	r = bodyType() == Body_t::Normal ? push(mem_, r, lit.lit) : push(mem_, r, lit);
	r->body.mend = r->top;
	return *this;
}
RuleBuilder& RuleBuilder::setBound(Weight_t bound) {
	POTASSCO_ASSERT(!rule_()->fix && bodyType() != Body_t::Normal, "Invalid call to setBound()");
	std::memcpy(bound_(), &bound, sizeof(Weight_t));
	return *this;
}
RuleBuilder& RuleBuilder::clearBody() {
	Rule* r = unfreeze(false);
	r->top = std::max(r->head.mend, static_cast<uint32_t>(sizeof(Rule)));
	r->body.init(0, 0);
	return *this;
}
RuleBuilder& RuleBuilder::weaken(Body_t to, bool w) {
	Rule* r = rule_();
	if (r->body.type == Body_t::Normal || r->body.type == to) { return *this; }
	WeightLit_t* bIt = wlits_begin(), *bEnd = wlits_end();
	if (to == Body_t::Normal) {
		uint32_t i = r->body.mbeg - sizeof(Weight_t);
		r->body.init(i, 0);
		for (; bIt != bEnd; ++bIt, i += sizeof(Lit_t)) {
			new (mem_[i])Lit_t(bIt->lit);
		}
		r->body.mend = i;
		r->top = std::max(r->head.mend, r->body.mend);
	}
	else if (to == Body_t::Count && w && bIt != bEnd) {
		Weight_t bnd = *bound_(), min = bIt->weight;
		for (; bIt != bEnd; ++bIt) {
			if (min > bIt->weight) { min = bIt->weight; }
			bIt->weight = 1;
		}
		setBound((bnd+(min-1))/min);
	}
	r->body.type = to;
	return *this;
}
Body_t       RuleBuilder::bodyType()   const { return static_cast<Body_t>(rule_()->body.type); }
LitSpan      RuleBuilder::body()       const { return span_cast<Lit_t>(mem_, rule_()->body); }
Lit_t*       RuleBuilder::lits_begin() const { return static_cast<Lit_t*>(mem_[rule_()->body.mbeg]); }
Lit_t*       RuleBuilder::lits_end()   const { return static_cast<Lit_t*>(mem_[rule_()->body.mend]); }
Sum_t        RuleBuilder::sum()        const { Sum_t r = {span_cast<WeightLit_t>(mem_, rule_()->body), bound()}; return r; }
WeightLit_t* RuleBuilder::wlits_begin()const { return static_cast<WeightLit_t*>(mem_[rule_()->body.mbeg]); }
WeightLit_t* RuleBuilder::wlits_end()  const { return static_cast<WeightLit_t*>(mem_[rule_()->body.mend]); }
Weight_t     RuleBuilder::bound()      const { return bodyType() != Body_t::Normal ? *bound_() : -1; }
Weight_t*    RuleBuilder::bound_()     const { return static_cast<Weight_t*>(mem_[rule_()->body.mbeg - sizeof(Weight_t)]); }
/////////////////////////////////////////////////////////////////////////////////////////
// RuleBuilder - Product
/////////////////////////////////////////////////////////////////////////////////////////
RuleBuilder& RuleBuilder::end(AbstractProgram* out) {
	Rule* r = rule_();
	r->fix  = 1;
	if (!out) { return *this; }
	if (r->head.type != Directive_t::Minimize && r->body.type == Body_t::Normal) {
		out->rule(static_cast<Head_t>(r->head.type), head(), body());
	}
	else {
		if   (r->head.type != Directive_t::Minimize) { out->rule(static_cast<Head_t>(r->head.type), head(), *bound_(), sum().lits); }
		else                                         { out->minimize(*bound_(), sum().lits); }
	}
	return *this;
}
Rule_t RuleBuilder::rule() const {
	Rule* r = rule_();
	const Rule::Span& h = r->head;
	const Rule::Span& b = r->body;
	Rule_t ret;
	ret.ht = static_cast<Head_t>(h.type);
	ret.head = head();
	ret.bt = static_cast<Body_t>(b.type);
	if (b.type == Body_t::Normal) {
		ret.cond = body();
	}
	else {
		ret.agg = sum();
	}
	return ret;
}

} // namespace Potassco
