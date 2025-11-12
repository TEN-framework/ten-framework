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
#include <clasp/solver_types.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/statistics.h>
#include <new>
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// Statistics
/////////////////////////////////////////////////////////////////////////////////////////
#define NO_ARG
#define CLASP_STAT_ACCU(m, k, a, accu) accu;
#define CLASP_STAT_KEY(m, k, a, accu)  k,
#define CLASP_STAT_GET(m, k, a, accu) if (std::strcmp(key, k) == 0) { return a; }
#define CLASP_DEFINE_ISTATS_COMMON(T, STATS, name) \
	static const char* const T ## _s[] = { STATS(CLASP_STAT_KEY, NO_ARG, NO_ARG) name };\
	uint32 T::size()                     { return (sizeof(T ## _s)/sizeof(T ## _s[0]))-1; } \
	const char* T::key(uint32 i)         { POTASSCO_CHECK(i < size(), ERANGE); return T ## _s[i]; } \
	void T::accu(const T& o)             { STATS(CLASP_STAT_ACCU, (*this), o);}
/////////////////////////////////////////////////////////////////////////////////////////
// CoreStats
/////////////////////////////////////////////////////////////////////////////////////////
CLASP_DEFINE_ISTATS_COMMON(CoreStats, CLASP_CORE_STATS, "core")
StatisticObject CoreStats::at(const char* key) const {
#define VALUE(X) StatisticObject::value(&X)
	CLASP_CORE_STATS(CLASP_STAT_GET, NO_ARG, NO_ARG);
#undef VALUE
	POTASSCO_CHECK(false, ERANGE);
}
void CoreStats::reset() { std::memset(this, 0, sizeof(*this)); }
/////////////////////////////////////////////////////////////////////////////////////////
// JumpStats
/////////////////////////////////////////////////////////////////////////////////////////
#define MAX_MEM(X, Y) X = std::max((X), (Y))
CLASP_DEFINE_ISTATS_COMMON(JumpStats, CLASP_JUMP_STATS, "jumps")
#undef MAX_MEM
StatisticObject JumpStats::at(const char* key) const {
#define VALUE(X) StatisticObject::value(&X)
	CLASP_JUMP_STATS(CLASP_STAT_GET, NO_ARG, NO_ARG);
#undef VALUE
	POTASSCO_CHECK(false, ERANGE);
}
void JumpStats::reset() { std::memset(this, 0, sizeof(*this)); }
/////////////////////////////////////////////////////////////////////////////////////////
// ExtendedStats
/////////////////////////////////////////////////////////////////////////////////////////
CLASP_DEFINE_ISTATS_COMMON(ExtendedStats, CLASP_EXTENDED_STATS, "extra")
namespace {
double _lemmas(const ExtendedStats* self)     { return static_cast<double>(self->lemmas()); }
double _learntLits(const ExtendedStats* self) { return static_cast<double>(self->learntLits()); }
}
StatisticObject ExtendedStats::at(const char* key) const {
#define VALUE(X) StatisticObject::value(&X)
#define MEM_FUN(X) StatisticObject::value<ExtendedStats, _ ## X>(this)
#define MAP(X) StatisticObject::map(&X)
	CLASP_EXTENDED_STATS(CLASP_STAT_GET, NO_ARG, NO_ARG);
#undef VALUE
#undef MEM_FUN
#undef MAP
	POTASSCO_CHECK(false, ERANGE);
}
void ExtendedStats::reset() {
	std::memset(this, 0, sizeof(ExtendedStats) - sizeof(JumpStats));
	jumps.reset();
}
/////////////////////////////////////////////////////////////////////////////////////////
// SolverStats
/////////////////////////////////////////////////////////////////////////////////////////
SolverStats::SolverStats() : extra(0), multi(0) {}
SolverStats::SolverStats(const SolverStats& o) : CoreStats(o), extra(0), multi(0) {
	if (o.extra && enableExtended()) { extra->accu(*o.extra); }
}
SolverStats::~SolverStats() {
	delete extra;
}
bool SolverStats::enableExtended() {
	return extra != 0 || (extra = new (std::nothrow) ExtendedStats()) != 0;
}
void SolverStats::reset() {
	CoreStats::reset();
	if (extra) extra->reset();
}
void SolverStats::accu(const SolverStats& o) {
	CoreStats::accu(o);
	if (extra && o.extra) extra->accu(*o.extra);
}
void SolverStats::accu(const SolverStats& o, bool enableRhs) {
	if (enableRhs) { enable(o); }
	accu(o);
}
void SolverStats::flush() const {
	if (multi) {
		multi->enable(*this);
		multi->accu(*this);
		multi->flush();
	}
}
void SolverStats::swapStats(SolverStats& o) {
	std::swap(static_cast<CoreStats&>(*this), static_cast<CoreStats&>(o));
	std::swap(extra, o.extra);
}
uint32 SolverStats::size() const {
	return CoreStats::size() + (extra != 0);
}
const char* SolverStats::key(uint32 i) const {
	POTASSCO_CHECK(i < size(), ERANGE);
	return i < CoreStats::size() ? CoreStats::key(i) : "extra";
}
template <unsigned n>
static bool matchPath(const char*& path, const char (&key)[n]) {
	std::size_t kLen = n-1;
	return std::strncmp(path, key, kLen) == 0 && (!path[kLen] || path[kLen++] == '.') && (path += kLen) != 0;
}
StatisticObject SolverStats::at(const char* k) const {
	if (extra && matchPath(k, "extra")) {
		return !*k ? StatisticObject::map(extra) : extra->at(k);
	}
	else {
		return CoreStats::at(k);
	}
}
void SolverStats::addTo(const char* key, StatsMap& solving, StatsMap* accu) const {
	solving.add(key, StatisticObject::map(this));
	if (accu && this->multi) { multi->addTo(key, *accu, 0); }
}
#undef NO_ARG
#undef CLASP_STAT_ACCU
#undef CLASP_STAT_KEY
#undef CLASP_STAT_GET
#undef CLASP_DEFINE_ISTATS_COMMON
/////////////////////////////////////////////////////////////////////////////////////////
// ClauseHead
/////////////////////////////////////////////////////////////////////////////////////////
ClauseHead::ClauseHead(const InfoType& init) : info_(init){
	static_assert(sizeof(ClauseHead)<=32, "Unsupported Alignment");
	head_[2] = lit_false();
}
void ClauseHead::resetScore(ScoreType sc) {
	info_.setScore(sc);
}
void ClauseHead::attach(Solver& s) {
	assert(head_[0] != head_[1] && head_[1] != head_[2]);
	s.addWatch(~head_[0], ClauseWatch(this));
	s.addWatch(~head_[1], ClauseWatch(this));
}

void ClauseHead::detach(Solver& s) {
	s.removeWatch(~head_[0], this);
	s.removeWatch(~head_[1], this);
}

bool ClauseHead::locked(const Solver& s) const {
	return (s.isTrue(head_[0]) && s.reason(head_[0]) == this)
	  ||   (s.isTrue(head_[1]) && s.reason(head_[1]) == this);
}

bool ClauseHead::satisfied(const Solver& s) const {
	return s.isTrue(head_[0]) || s.isTrue(head_[1]) || s.isTrue(head_[2]);
}

bool ClauseHead::toImplication(Solver& s) {
	uint32 sz     = isSentinel(head_[1]) ? 1 : 2 + (!s.isFalse(head_[2]) || s.level(head_[2].var()) > 0);
	ClauseRep rep = ClauseRep::create(head_, sz, InfoType(ClauseHead::type()).setLbd(2).setTagged(tagged()));
	bool implicit = s.allowImplicit(rep);
	bool locked   = ClauseHead::locked(s) && s.decisionLevel() > 0;
	rep.prep      = 1;
	if ((locked || !implicit) && sz > 1) { return false; }
	s.add(rep, false);
	detach(s);
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////
// SmallClauseAlloc
/////////////////////////////////////////////////////////////////////////////////////////
SmallClauseAlloc::SmallClauseAlloc() : blocks_(0), freeList_(0) { }
SmallClauseAlloc::~SmallClauseAlloc() {
	Block* r = blocks_;
	while (r) {
		Block* t = r;
		r = r->next;
		::operator delete(t);
	}
}

void SmallClauseAlloc::allocBlock() {
	Block* r = (Block*)::operator new(sizeof(Block));
	for (uint32 i = 0; i < Block::num_chunks-1; ++i) {
		r->chunk[i].next = &r->chunk[i+1];
	}
	r->chunk[Block::num_chunks-1].next = freeList_;
	freeList_ = r->chunk;
	r->next   = blocks_;
	blocks_   = r;
}

}
