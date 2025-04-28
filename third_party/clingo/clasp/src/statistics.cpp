//
// Copyright (c) 2016-2018 Benjamin Kaufmann
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
#include <clasp/statistics.h>
#include <clasp/util/misc_types.h>
#include <clasp/util/hash.h>
#include <potassco/match_basic_types.h>
#include <potassco/string_convert.h>
#include POTASSCO_EXT_INCLUDE(unordered_map)
#include POTASSCO_EXT_INCLUDE(unordered_set)
#include <stdexcept>
#include <cstring>
#ifdef _MSC_VER
#pragma warning (disable : 4996)
#endif
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// StatisticObject
/////////////////////////////////////////////////////////////////////////////////////////
StatisticObject::I      StatisticObject::empty_s(Potassco::Statistics_t::Empty);
StatisticObject::RegVec StatisticObject::types_s(1, &StatisticObject::empty_s);

StatisticObject::StatisticObject() : handle_(0) {}
StatisticObject::StatisticObject(const void* obj, uint32 type) {
	handle_  = static_cast<uint64>(type) << 48;
	handle_ |= static_cast<uint64>(reinterpret_cast<uintp>(obj));
}
const void* StatisticObject::self() const {
	static const uint64 ptrMask = bit_mask<uint64>(48) - 1;
	return reinterpret_cast<const void*>(static_cast<uintp>(handle_ & ptrMask));
}
std::size_t StatisticObject::typeId() const {
	return static_cast<uint32>(handle_ >> 48);
}
const StatisticObject::I* StatisticObject::tid() const {
	return types_s.at(static_cast<uint32>(handle_ >> 48));
}
StatisticObject::Type StatisticObject::type() const {
	return !empty() ? tid()->type : Potassco::Statistics_t::Empty;
}
bool StatisticObject::empty() const {
	return handle_ == 0;
}
uint32 StatisticObject::size() const {
	switch (type()) {
		default: throw std::logic_error("invalid object");
		case Potassco::Statistics_t::Empty: return 0;
		case Potassco::Statistics_t::Value: return 0;
		case Potassco::Statistics_t::Map:   return static_cast<const M*>(tid())->size(self());
		case Potassco::Statistics_t::Array: return static_cast<const A*>(tid())->size(self());
	}
}
const char* StatisticObject::key(uint32 i) const {
	POTASSCO_REQUIRE(type() == Potassco::Statistics_t::Map, "type error");
	return static_cast<const M*>(tid())->key(self(), i);
}
StatisticObject StatisticObject::at(const char* k) const {
	POTASSCO_REQUIRE(type() == Potassco::Statistics_t::Map, "type error");
	return static_cast<const M*>(tid())->at(self(), k);
}
StatisticObject StatisticObject::operator[](uint32 i) const {
	POTASSCO_REQUIRE(type() == Potassco::Statistics_t::Array, "type error");
	return static_cast<const A*>(tid())->at(self(), i);
}
double StatisticObject::value() const {
	POTASSCO_REQUIRE(type() == Potassco::Statistics_t::Value, "type error");
	return static_cast<const V*>(tid())->value(self());
}
std::size_t StatisticObject::hash() const {
	return POTASSCO_EXT_NS::hash<uint64>()(toRep());
}
uint64 StatisticObject::toRep() const {
	return handle_;
}
StatisticObject StatisticObject::fromRep(uint64 x) {
	if (!x) { return StatisticObject(0, 0); }
	StatisticObject r;
	r.handle_ = x;
	POTASSCO_REQUIRE(r.tid() != 0 && (reinterpret_cast<uintp>(r.self()) & 3u) == 0, "invalid key");
	return r;
}
/////////////////////////////////////////////////////////////////////////////////////////
// StatsMap
/////////////////////////////////////////////////////////////////////////////////////////
StatisticObject StatsMap::at(const char* k) const {
	if (const StatisticObject* o = find(k)) { return *o; }
	throw std::out_of_range(POTASSCO_FORMAT("StatsMap::at with key '%s'", k));
}
const StatisticObject* StatsMap::find(const char* k) const {
	for (MapType::const_iterator it = keys_.begin(), end = keys_.end(); it != end; ++it) {
		if (std::strcmp(it->first, k) == 0) { return &it->second; }
	}
	return 0;
}
bool StatsMap::add(const char* k, const StatisticObject& o) {
	return !find(k) && (keys_.push_back(MapType::value_type(k, o)), true);
}
void StatsMap::push(const char* k, const StatisticObject& o) {
	keys_.push_back(MapType::value_type(k, o));
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspStatistics
/////////////////////////////////////////////////////////////////////////////////////////
struct ClaspStatistics::Impl {
	typedef POTASSCO_EXT_NS::unordered_set<uint64> KeySet;
	typedef POTASSCO_EXT_NS::unordered_set<const char*, StrHash, StrEq> StringSet;
	struct Map : Clasp::StatsMap { const static std::size_t id_s; };
	struct Arr : PodVector<StatisticObject>::type {
		StatisticObject toStats() const { return StatisticObject::array(this); }
		const static std::size_t id_s;
	};
	struct Val {
		Val(double d) : value(d) {}
		operator double() const { return value;  }
		double value;
		const static std::size_t id_s;
	};

	Impl() : root_(0), gc_(0), rem_(0) {}

	~Impl() {
		for (StringSet::iterator it = strings_.begin(), end = strings_.end(); it != end; ++it) {
			delete[] *it;
		}
		for (KeySet::iterator it = objects_.begin(), end = objects_.end(); it != end; ++it) {
			destroyIfWritable(it);
		}
	}

	Key_t add(const StatisticObject& o) {
		return *objects_.insert(o.toRep()).first;
	}

	void destroyIfWritable(KeySet::iterator it) {
		StatisticObject obj = StatisticObject::fromRep(*it);
		size_t typeId = obj.typeId();
		if      (typeId == Map::id_s) { delete static_cast<const Map*>(obj.self()); }
		else if (typeId == Arr::id_s) { delete static_cast<const Arr*>(obj.self()); }
		else if (typeId == Val::id_s) { delete static_cast<const Val*>(obj.self()); }
	}
	StatisticObject newWritable(Type type) {
		StatisticObject obj;
		switch (type) {
			case Potassco::Statistics_t::Value:
				obj = StatisticObject::value(new Val(0.0));
				break;
			case Potassco::Statistics_t::Map:
				obj = StatisticObject::map(new Map());
				break;
			case Potassco::Statistics_t::Array:
				obj = StatisticObject::array(new Arr());
				break;
			default:
				POTASSCO_REQUIRE(false, "unsupported statistic object type");
				return obj;
		}
		add(obj);
		return obj;
	}

	bool writable(Key_t k) const {
		size_t typeId = StatisticObject::fromRep(k).typeId();
		return (typeId == Map::id_s || typeId == Arr::id_s || typeId == Val::id_s) && objects_.count(k) != 0;
	}

	bool remove(const StatisticObject& o) {
		KeySet::iterator it = objects_.find(o.toRep());
		if (it != objects_.end() && !emptyKey(*it)) {
			destroyIfWritable(it);
			objects_.erase(it);
			return true;
		}
		return false;
	}

	StatisticObject get(Key_t k) const {
		KeySet::const_iterator it = objects_.find(k);
		POTASSCO_REQUIRE(it != objects_.end(), "invalid key");
		return StatisticObject::fromRep(k);
	}

	template <class T>
	T* writable(Key_t k) const {
		StatisticObject obj = StatisticObject::fromRep(k);
		POTASSCO_REQUIRE(writable(k), "key not writable");
		POTASSCO_REQUIRE(T::id_s == obj.typeId(), "type error");
		return static_cast<T*>(const_cast<void*>(obj.self()));
	}

	void update(const StatisticObject& root) {
		KeySet seen;
		visit(root, seen);
		if (objects_.size() != seen.size()) {
			for (KeySet::iterator it = objects_.begin(), end = objects_.end(); it != end; ++it) {
				if (seen.count(*it) == 0) {
					destroyIfWritable(it);
				}
			}
			objects_.swap(seen);
		}
	}

	void visit(const StatisticObject& obj, KeySet& visited) {
		KeySet::iterator it = objects_.find(obj.toRep());
		if (it == objects_.end() || !visited.insert(*it).second) { return; }
		switch (obj.type()) {
			case Potassco::Statistics_t::Map:
				for (uint32 i = 0, end = obj.size(); i != end; ++i) { visit(obj.at(obj.key(i)), visited); }
				break;
			case Potassco::Statistics_t::Array:
				for (uint32 i = 0, end = obj.size(); i != end; ++i) { visit(obj[i], visited); }
				break;
			default: break;
		}
	}

	const char* string(const char* s) {
		StringSet::iterator it = strings_.find(s);
		if (it != strings_.end())
			return *it;
		return *strings_.insert(it, std::strcpy(new char[std::strlen(s) + 1], s));
	}
	static Key_t key(const StatisticObject& obj) { return static_cast<Key_t>(obj.toRep()); }
	static bool  emptyKey(Key_t k)               { return k == 0; }

	KeySet    objects_;
	StringSet strings_;
	Key_t     root_;
	uint32    gc_;
	uint32    rem_;
};

const std::size_t ClaspStatistics::Impl::Map::id_s = StatisticObject::map(static_cast<ClaspStatistics::Impl::Map*>(0)).typeId();
const std::size_t ClaspStatistics::Impl::Arr::id_s = StatisticObject::array(static_cast<ClaspStatistics::Impl::Arr*>(0)).typeId();
const std::size_t ClaspStatistics::Impl::Val::id_s = StatisticObject::value(static_cast<ClaspStatistics::Impl::Val*>(0)).typeId();

ClaspStatistics::ClaspStatistics() : impl_(new Impl()) {}
ClaspStatistics::ClaspStatistics(StatisticObject obj) : impl_(new Impl()) {
	impl_->root_ = impl_->add(obj);
}
ClaspStatistics::~ClaspStatistics() {
	delete impl_;
}
StatisticObject ClaspStatistics::getObject(Key_t k) const {
	return impl_->get(k);
}
ClaspStatistics::Key_t ClaspStatistics::root() const {
	return impl_->root_;
}
Potassco::Statistics_t ClaspStatistics::type(Key_t key) const {
	return getObject(key).type();
}
size_t ClaspStatistics::size(Key_t key) const {
	return getObject(key).size();
}
bool ClaspStatistics::writable(Key_t key) const {
	return impl_->writable(key);
}
ClaspStatistics::Key_t ClaspStatistics::at(Key_t arrK, size_t index) const {
	return impl_->add(getObject(arrK)[toU32(index)]);
}
const char* ClaspStatistics::key(Key_t mapK, size_t i) const {
	return getObject(mapK).key(toU32(i));
}
ClaspStatistics::Key_t ClaspStatistics::get(Key_t key, const char* path) const {
	return impl_->add(!std::strchr(path, '.')
		? getObject(key).at(path)
		: findObject(key, path));
}
bool ClaspStatistics::find(Key_t mapK, const char* element, Key_t* outKey) const {
	try {
		if (!writable(mapK) || std::strchr(element, '.')) {
			return !findObject(mapK, element, outKey).empty();
		}
		Impl::Map* map = impl_->writable<Impl::Map>(mapK);
		if (const StatisticObject* obj = map->find(element)) {
			if (outKey) { *outKey = impl_->add(*obj); }
			return true;
		}
	}
	catch (const std::exception&) {}
	return false;
}
double ClaspStatistics::value(Key_t key) const {
	return getObject(key).value();
}
ClaspStatistics::Key_t ClaspStatistics::changeRoot(Key_t k) {
	Key_t old = impl_->root_;
	impl_->root_ = impl_->add(getObject(k));
	return old;
}
StatsMap* ClaspStatistics::makeRoot() {
	Impl::Map* root = new Impl::Map();
	impl_->root_ = impl_->add(StatisticObject::map(root));
	return root;
}
bool ClaspStatistics::removeStat(const StatisticObject& obj, bool recurse) {
	bool ret = impl_->remove(obj);
	if (ret && recurse) {
		if (obj.type() == Potassco::Statistics_t::Map) {
			for (uint32 i = 0, end = obj.size(); i != end; ++i) {
				removeStat(obj.at(obj.key(i)), true);
			}
		}
		else if (obj.type() == Potassco::Statistics_t::Array) {
			for (uint32 i = 0, end = obj.size(); i != end; ++i) {
				removeStat(obj[i], true);
			}
		}
	}
	return ret;
}
bool ClaspStatistics::removeStat(Key_t k, bool recurse) {
	try { return removeStat(impl_->get(k), recurse); }
	catch (const std::logic_error&) { return false; }
}
void ClaspStatistics::update() {
	if (impl_->root_) { impl_->update(getObject(impl_->root_)); }
}
StatisticObject ClaspStatistics::findObject(Key_t root, const char* path, Key_t* res) const {
	StatisticObject o = getObject(root);
	StatisticObject::Type t = o.type();
	char temp[1024]; const char* top, *parent = path;
	for (int pos; path && *path;) {
		top = path;
		if ((path = std::strchr(path, '.')) != 0) {
			std::size_t len = static_cast<std::size_t>(path++ - top);
			POTASSCO_ASSERT(len < 1024, "invalid key");
			top = (const char*)std::memcpy(temp, top, len);
			temp[len] = 0;
		}
		if      (t == Potassco::Statistics_t::Map) { o = o.at(top); }
		else if (t == Potassco::Statistics_t::Array && Potassco::match(top, pos) && pos >= 0) {
			o = o[uint32(pos)];
		}
		else {
			throw std::out_of_range(POTASSCO_FORMAT("invalid path: '%s' at key '%s'", parent, top));
		}
		t = o.type();
	}
	if (res) { *res = impl_->add(o); }
	return o;
}
ClaspStatistics::Key_t ClaspStatistics::push(Key_t key, Type type) {
	Impl::Arr* arr = impl_->writable<Impl::Arr>(key);
	StatisticObject obj = impl_->newWritable(type);
	arr->push_back(obj);
	return impl_->key(obj);
}
ClaspStatistics::Key_t ClaspStatistics::add(Key_t mapK, const char* name, Type type) {
	Impl::Map* map = impl_->writable<Impl::Map>(mapK);
	if (const StatisticObject* stat = map->find(name)) {
		POTASSCO_REQUIRE(stat->type() == type, "redefinition error");
		return impl_->key(*stat);
	}
	StatisticObject stat = impl_->newWritable(type);
	map->push(impl_->string(name), stat);
	return impl_->key(stat);
}
void ClaspStatistics::set(Key_t key, double value) {
	Impl::Val* val = impl_->writable<Impl::Val>(key);
	*val = value;
}
/////////////////////////////////////////////////////////////////////////////////////////
// StatsVisitor
/////////////////////////////////////////////////////////////////////////////////////////
StatsVisitor::~StatsVisitor() {}
bool StatsVisitor::visitGenerator(Operation) {
	return true;
}
bool StatsVisitor::visitThreads(Operation) {
	return true;
}
bool StatsVisitor::visitTester(Operation) {
	return true;
}
bool StatsVisitor::visitHccs(Operation) {
	return true;
}
void StatsVisitor::visitHcc(uint32, const ProblemStats& p, const SolverStats& s) {
	visitProblemStats(p);
	visitSolverStats(s);
}
void StatsVisitor::visitThread(uint32, const SolverStats& stats) {
	visitSolverStats(stats);
}

}
