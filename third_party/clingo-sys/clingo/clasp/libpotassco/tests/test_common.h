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
#ifndef POTASSCO_TEST_COMMON_H_INCLUDED
#define POTASSCO_TEST_COMMON_H_INCLUDED
#include <vector>
#include <potassco/basic_types.h>
namespace Potassco {
namespace Test {
template <class T>
using Vec = std::vector<T>;

struct Rule {
	Head_t           ht;
	Vec<Atom_t>      head;
	Body_t           bt;
	Weight_t         bnd;
	Vec<WeightLit_t> body;
	bool operator==(const Rule& rhs) const {
		return ht == rhs.ht && head == rhs.head && bt == rhs.bt && bnd == rhs.bnd && body == rhs.body;
	}
};
struct Edge {
	int s;
	int t;
	Vec<Lit_t> cond;
	bool operator==(const Edge& rhs) const {
		return s == rhs.s && t == rhs.t && cond == rhs.cond;
	}
};
struct Heuristic {
	Atom_t      atom;
	Heuristic_t type;
	int         bias;
	unsigned    prio;
	Vec<Lit_t>  cond;
	bool operator==(const Heuristic& rhs) const {
		return atom == rhs.atom && type == rhs.type && bias == rhs.bias && prio == rhs.prio && cond == rhs.cond;
	}
};

class ReadObserver : public AbstractProgram {
public:
	virtual void initProgram(bool inc) override {
		incremental = inc;
	}
	virtual void beginStep() override {
		++nStep;
	}
	virtual void endStep() override {}

	virtual void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& cond) override {
		heuristics.push_back({a, t, bias, prio, {begin(cond), end(cond)}});
	}
	virtual void acycEdge(int s, int t, const LitSpan& cond) override {
		edges.push_back({s, t, {begin(cond), end(cond)}});
	}
	Vec<Heuristic> heuristics;
	Vec<Edge>      edges;
	int  nStep = 0;
	bool incremental = false;
};


}}

#endif
