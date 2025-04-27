// {{{ MIT License

// Copyright 2017 Roland Kaminski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// }}}

#ifndef GRINGO_DOMAIN_HH
#define GRINGO_DOMAIN_HH

#include <algorithm>
#include <cassert>
#include <gringo/base.hh>
#include <gringo/types.hh>
#include <deque>
#include <gringo/hash_set.hh>
#include <stdexcept>

namespace Gringo {

// {{{ declaration of BinderType

enum class BinderType { NEW, OLD, ALL };

inline std::ostream &operator<<(std::ostream &out, BinderType x) {
    switch (x) {
        case BinderType::NEW: { out << "NEW"; break; }
        case BinderType::OLD: { out << "OLD"; break; }
        case BinderType::ALL: { out << "ALL"; break; }
    }
    return out;
}


// }}}
// {{{ declaration of IndexUpdater

class IndexUpdater {
public:
    IndexUpdater() = default;
    IndexUpdater(IndexUpdater const &other) = default;
    IndexUpdater(IndexUpdater &&other) noexcept = default;
    IndexUpdater &operator=(IndexUpdater const &other) = default;
    IndexUpdater &operator=(IndexUpdater &&other) noexcept = default;
    virtual ~IndexUpdater() noexcept = default;
    // Updates the index with fresh atoms from the domain.
    // First traverses the atoms in the domain skipping undefined atoms and
    // afterwards traversing undefined atoms that have been defined later.
    virtual bool update() = 0;
};

using SValVec = std::vector<Term::SVal>;

// }}}
// {{{ declaration of BindIndex

template <class Domain>
class BindIndexEntry {
public:
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    using SizeType  = typename Domain::SizeType;
    BindIndexEntry(SymVec const &bound) {
        // NOLINTNEXTLINE
        data_ = static_cast<uint64_t*>(malloc(sizeof(uint64_t) * bound.size() + sizeof(SizeType)));
        if (data_ == nullptr) {
            throw std::bad_alloc();
        }
        begin_ = asSize_(bound.size());
        uint64_t *it = data_;
        for (auto const &sym : bound) {
            *it++ = sym.rep();
        }
    }
    BindIndexEntry(BindIndexEntry const &other) = delete;
    BindIndexEntry(BindIndexEntry &&other) noexcept {
        *this = std::move(other);
    }
    BindIndexEntry &operator=(BindIndexEntry const &other) = delete;
    BindIndexEntry &operator=(BindIndexEntry &&other) noexcept {
        std::swap(data_, other.data_);
        std::swap(begin_, other.begin_);
        std::swap(end_, other.end_);
        std::swap(reserved_, other.reserved_);
        return *this;
    }
    ~BindIndexEntry() {
        // NOLINTNEXTLINE
        free(data_);
    }
    SizeType const *begin() const {
        return begin_;
    }
    SizeType const *end() const {
        return begin_ + end_;
    }
    void push(SizeType x) {
        assert(reserved_ > 0 && end_ <= reserved_);
        if (end_ == reserved_) {
            size_t bound = asUint64_() - data_;
            size_t oldsize = sizeof(uint64_t) * bound + sizeof(SizeType) * end_;
            size_t size = oldsize + sizeof(SizeType) * end_;
            if (size < oldsize) {
                throw std::runtime_error("size limit exceeded");
            }
            // NOLINTNEXTLINE
            auto *ret = static_cast<uint64_t*>(realloc(data_, size));
            if (ret == nullptr) {
                throw std::bad_alloc();
            }
            reserved_ = 2 * end_;
            if (data_ != ret) {
                data_ = ret;
                begin_ = asSize_(bound);
            }
        }
        begin_[end_++] = x;
    }
    size_t hash() const {
        return hash_range(data_, asUint64_());
    }
    friend bool operator==(BindIndexEntry const &x, BindIndexEntry const &y) {
        return std::equal(x.data_, x.asUint64_(), y.data_, [](uint64_t a, uint64_t b) { return a == b; });
    }
    friend bool operator==(BindIndexEntry const &x, SymVec const &y) {
        return std::equal(y.begin(), y.end(), x.data_, [](Symbol const &a, uint64_t b) { return a.rep() == b; });
    }
    friend bool operator==(SymVec const &x, BindIndexEntry const &y) {
        return y == x;
    }
private:
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    uint64_t *asUint64_() const {
        return reinterpret_cast<uint64_t *>(begin_);
    }
    SizeType *asSize_(size_t offset) const {
        return reinterpret_cast<SizeType *>(data_ + offset);
    }
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    Id_t end_{0};
    Id_t reserved_{1};
    uint64_t* data_{nullptr};
    SizeType* begin_{nullptr};
};

} // namespace Gringo

namespace std {

template <class Domain>
struct hash<Gringo::BindIndexEntry<Domain>> {
    size_t operator()(Gringo::BindIndexEntry<Domain> const &entry) const {
        return entry.hash();
    }
    size_t operator()(Gringo::SymVec const &e) const {
        return hash_range(e.begin(), e.end());
    };
};

} // namespace std

namespace Gringo {

// An index for a positive literal occurrence
// with at least one variable bound and one variable unbound.
template <class Domain>
class BindIndex : public IndexUpdater {
public:
    using SizeType  = typename Domain::SizeType;
    using OffsetVec = std::vector<SizeType>;
    using Iterator  = SizeType const *;
    using Entry     = BindIndexEntry<Domain>;
    using Index     = ordered_set<Entry>;

    struct OffsetRange {
        bool next(Id_t &offset, Term const &repr, BindIndex &idx) {
            if (current != end) {
                offset = *current++;
                repr.match(idx.domain_[offset]);
                return true;
            }
            return false;
        }
        Iterator current;
        Iterator end;
    };

    BindIndex(Domain &domain, SValVec &&bound, UTerm &&repr)
    : repr_(std::move(repr))
    , domain_(domain)
    , bound_(std::move(bound)) {
        assert(!bound_.empty());
    }
    BindIndex(BindIndex const &other) = default;
    BindIndex(BindIndex &&other) noexcept = default;
    BindIndex &operator=(BindIndex const &other) = default;
    BindIndex &operator=(BindIndex &&other) noexcept = default;
    ~BindIndex() noexcept override = default;

    bool update() override {
        return domain_.update([this](SizeType offset) { add(offset); }, *repr_, imported_, importedDelayed_);
    }

    // Returns a range of offsets corresponding to atoms that match the given bound variables.
    OffsetRange lookup(SValVec const &bound, BinderType type, Logger &log) {
        static_cast<void>(log);
        boundVals_.clear();
        for (auto &&x : bound) { boundVals_.emplace_back(*x); }
        auto it(data_.find(boundVals_));
        if (it != data_.end()) {
            auto cmp = [this](SizeType a, SizeType gen) { return domain_[a].generation() < gen; };
            switch (type) {
                case BinderType::NEW: { return { std::lower_bound(it->begin(), it->end(), domain_.generation(), cmp), it->end() }; }
                case BinderType::OLD: { return { it->begin(), std::lower_bound(it->begin(), it->end(), domain_.generation(), cmp) }; }
                case BinderType::ALL: { return { it->begin(), it->end() }; }
            }
        }
        return { nullptr, nullptr };
    }

    // The equality and hash functions are used to prevent adding structurally equivalent indices twice.
    bool operator==(BindIndex const &x) const {
        return *repr_ == *x.repr_;
    }

    size_t hash() const {
        return repr_->hash();
    }

private:
    // Adds an atom given by its offset to the index.
    // Assumes that the atom matches and has not been added previously.
    void add(Id_t offset) {
        boundVals_.clear();
        for (auto &y : bound_) { boundVals_.emplace_back(*y); }
        auto jt = data_.insert(boundVals_).first;
        const_cast<Entry &>(*jt).push(offset);
    }

    UTerm const repr_;
    Domain     &domain_;
    SValVec     bound_;
    SymVec      boundVals_;
    Index       data_;
    Id_t        imported_ = 0;
    Id_t        importedDelayed_ = 0;
};

} // namespace Gringo

namespace std {

template <class Domain>
struct hash<Gringo::BindIndex<Domain>> {
    size_t operator()(Gringo::BindIndex<Domain> const &entry) const {
        return entry.hash();
    }
};

} // namespace std

namespace Gringo {

// }}}
// {{{ declaration of FullIndex

// An index for a positive literal occurrence with all literals unbound.
// The matches are represented as ranges.
// This means a literal of form p(X,Y) where both X and Y are unbound has an index with just one range.
template <class Domain>
class FullIndex : public IndexUpdater {
public:
    using SizeType    = typename Domain::SizeType;
    using IntervalVec = std::vector<std::pair<SizeType, SizeType>>;
    using Iterator    = typename IntervalVec::iterator;

    struct OffsetRange {
        bool next(SizeType &offset, Term const &repr, FullIndex &idx) {
            // For Old and All atoms, iterate forward until the atoms in the index are exceeded.
            // For Old atoms stop early if the atoms do not belong to previous generations anymore.
            if (type != BinderType::NEW) {
                if (rangeOffset == idx.index_.size()) {
                    return false;
                }
                if (intervalOffset == idx.index_[rangeOffset].second) {
                    ++rangeOffset;
                    if (rangeOffset == idx.index_.size()) {
                        return false;
                    }
                    intervalOffset = idx.index_[rangeOffset].first;
                }
                offset = intervalOffset++;
                auto &atom = idx.domain_[offset];
                if (type == BinderType::OLD && atom.generation() >= idx.domain_.generation()) {
                    rangeOffset = static_cast<SizeType>(idx.index_.size());
                    return false;
                }
            }
            // For New atoms iterate backward until the atoms do not belong to the current generation anymore.
            else {
                if (rangeOffset == 0) {
                    return false;
                }
                if (intervalOffset == idx.index_[rangeOffset - 1].first) {
                    --rangeOffset;
                    if (rangeOffset == 0) {
                        return false;
                    }
                    intervalOffset = idx.index_[rangeOffset-1].second;
                }
                offset = --intervalOffset;
                auto &atom = idx.domain_[offset];
                if (atom.generation() < idx.domain_.generation()) {
                    rangeOffset = 0;
                    return false;
                }
            }
            repr.match(idx.domain_[offset]);
            return true;
        }
        BinderType type;
        SizeType rangeOffset;
        SizeType intervalOffset;
    };

    // This index can be initialized to skip some atoms in a domain.
    // The number of skipped atoms are part of the equality and hash comparisons of the index.
    // This is used to implement projection in the incremental case.
    FullIndex(Domain &domain, UTerm &&repr, Id_t imported)
    : repr_(std::move(repr))
    , domain_(domain)
    , imported_(imported)
    , initialImport_(imported) { }

    FullIndex(FullIndex const &other) = default;
    FullIndex(FullIndex &&other) noexcept = default;
    FullIndex &operator=(FullIndex const &other) = default;
    FullIndex &operator=(FullIndex &&other) noexcept = default;
    ~FullIndex() noexcept override = default;

    // Returns a range of offsets corresponding to matching atoms.
    OffsetRange lookup(BinderType type, Logger &log) {
        static_cast<void>(log);
        switch (type) {
            case BinderType::OLD:
            case BinderType::ALL: { return { type, 0, !index_.empty() ? index_.front().first : 0 }; }
            case BinderType::NEW: { return { type, static_cast<SizeType>(index_.size()), !index_.empty() ? index_.back().second : 0 }; }
        }
        throw std::logic_error("cannot happen");
    }

    bool update() override {
        return domain_.update([this](SizeType offset) { add(offset); return true; }, *repr_, imported_, importedDelayed_);
    }

    bool operator==(FullIndex const &x) const {
        return *repr_ == *x.repr_ && initialImport_ == x.initialImport_;
    }

    size_t hash() const {
        return get_value_hash(repr_, initialImport_);
    }

private:
    // Adds an atom offset to the index.
    // The offset is merged into the last interval if possible.
    // Maintains the generation order by always inserting at the end.
    void add(SizeType offset) {
        if (!index_.empty() && index_.back().second == offset) {
            index_.back().second++;
        }
        else {
            index_.emplace_back(offset, offset + 1);
        }
    }

    UTerm       repr_;
    Domain     &domain_;
    IntervalVec index_;
    Id_t        imported_;
    Id_t        importedDelayed_ = 0;
    Id_t        initialImport_;
};

} // namespace Gringo

namespace std {

template <class Domain>
struct hash<Gringo::FullIndex<Domain>> {
    size_t operator()(Gringo::FullIndex<Domain> const &entry) const { return entry.hash(); }
};

} // namespace std

namespace Gringo {

// }}}
// {{{ declaration of Domain

class Domain {
public:
    Domain() = default;
    Domain(Domain const &other) = default;
    Domain(Domain &&other) noexcept = default;
    Domain &operator=(Domain const &other) = default;
    Domain &operator=(Domain &&other) noexcept = default;
    virtual ~Domain() noexcept = default;

    virtual void init() = 0;
    virtual void enqueue() = 0;
    virtual bool dequeue() = 0;
    virtual bool isEnqueued() const = 0;
    // Advances the domain to the next generation.
    // Returns true if there are fresh atoms that have to be incorporated into the indices.
    virtual void nextGeneration() = 0;
    virtual void setDomainOffset(Id_t offset) = 0;
    virtual Id_t domainOffset() const = 0;
};
using UDom = std::unique_ptr<Domain>;
using UDomVec = std::vector<UDom>;

// }}}
// {{{ declaration of AbstractDomain

template <class T>
class AbstractDomain : public Domain {
public:
    using Atom            = T;
    // TODO: this should be refactored to use an ordered_map instead
    using Atoms           = ordered_set<Atom, HashKey<Symbol, Cast<Symbol>, mix_value_hash<Symbol>>, EqualToKey<Symbol>>;
    using BindIndex       = Gringo::BindIndex<AbstractDomain>;
    using FullIndex       = Gringo::FullIndex<AbstractDomain>;
    using BindIndices     = std::unordered_set<BindIndex, mix_value_hash<BindIndex>>;
    using FullIndices     = std::unordered_set<FullIndex, mix_value_hash<FullIndex>>;
    using AtomVec         = typename Atoms::values_container_type;
    using Iterator        = typename AtomVec::iterator;
    using ConstIterator   = typename AtomVec::const_iterator;
    using SizeType        = uint32_t;
    using OffsetVec       = std::vector<SizeType>;

    AbstractDomain() = default;
    AbstractDomain(AbstractDomain const &other) = delete;
    AbstractDomain(AbstractDomain &&other) noexcept = delete;
    AbstractDomain &operator=(AbstractDomain const &other) = delete;
    AbstractDomain &operator=(AbstractDomain &&other) noexcept = delete;
    ~AbstractDomain() noexcept override = default;

    // All indices that use a domain have to be registered with it.
    BindIndex &add(SValVec &&bound, UTerm &&repr) {
        auto ret(indices_.emplace(*this, std::move(bound), std::move(repr)));
        // NOLINTNEXTLINE
        auto &idx = const_cast<BindIndex&>(*ret.first);
        idx.update();
        return idx;
    }

    FullIndex &add(UTerm &&repr, Id_t imported) {
        auto ret(fullIndices_.emplace(*this, std::move(repr), imported));
        // NOLINTNEXTLINE
        auto &idx = const_cast<FullIndex&>(*ret.first);
        idx.update();
        return idx;
    }

    // Function to lookup negative literals or non-recursive atoms.
    bool lookup(SizeType &offset, Term const &repr, RECNAF naf, Logger &log) {
        bool undefined = false;
        switch (naf) {
            case RECNAF::POS: {
                // Note: intended for non-recursive case only
                auto it = atoms_.find(repr.eval(undefined, log));
                if (!undefined && it != atoms_.end() && it->defined()) {
                    offset = static_cast<SizeType>(it - atoms_.begin());
                    return true;
                }
                break;
            }
            case RECNAF::NOT: {
                auto it = atoms_.find(repr.eval(undefined, log));
                if (!undefined && it != atoms_.end()) {
                    if (!it->fact()) {
                        offset = static_cast<SizeType>(it - atoms_.begin());
                        return true;
                    }
                }
                else if (!undefined) {
                    // This can only happen if literals are used negatively and non-recursively.
                    // Example: a :- not b.
                    offset = std::numeric_limits<SizeType>::max();
                    return true;
                }
                break;
            }
            case RECNAF::RECNOT: {
                auto it = reserve(repr.eval(undefined, log));
                if (!undefined && !it->fact()) {
                    offset = static_cast<SizeType>(it - begin());
                    return true;
                }
                break;
            }
            case RECNAF::NOTNOT: {
                // Note: intended for recursive case only
                auto it = reserve(repr.eval(undefined, log));
                if (!undefined) {
                    offset = static_cast<SizeType>(it - begin());
                    return true;
                }
                break;
            }
        }
        offset = std::numeric_limits<SizeType>::max();
        return false;
    }

    // Function to lookup recursive atoms.
    bool lookup(SizeType &offset, Term const &repr, BinderType type, Logger &log) {
        // Note: intended for recursive case only
        bool undefined = false;
        auto it = atoms_.find(repr.eval(undefined, log));
        if (!undefined && it != atoms_.end() && it->defined()) {
            switch (type) {
                case BinderType::OLD: {
                    if (it->generation() <  generation_) {
                        offset = static_cast<SizeType>(it - atoms_.begin());
                        return true;
                    }
                    break;
                }
                case BinderType::ALL: {
                    if (it->generation() <=  generation_) {
                        offset = static_cast<SizeType>(it - atoms_.begin());
                        return true;
                    }
                    break;
                }
                case BinderType::NEW: {
                    if (it->generation() == generation_) {
                        offset = static_cast<SizeType>(it - atoms_.begin());
                        return true;
                    }
                    break;
                }
            }
        }
        offset = std::numeric_limits<SizeType>::max();
        return false;
    }

    // Loops over all atoms that might have to be imported into an index.
    // Already seen atoms are indicated with the two offset parameters.
    // Returns true if a maching atom was false.
    // Furthermore, accepts a callback f that receives the offset of a matching atom.
    // If the callback returns false the search for matching atoms is stopped and the function returns true.
    template <typename F>
    bool update(F f, Term const &repr, SizeType &imported, SizeType &importedDelayed) {
        bool ret = false;
        for (auto it(atoms_.begin() + imported), ie(atoms_.end()); it < ie; ++it, ++imported) {
            if (it->defined()) {
                if (!it->delayed() && repr.match(*it)) {
                    ret = true;
                    f(imported);
                }
            }
            else {
                const_cast<Atom&>(*it).markDelayed();
            }
        }
        for (auto it(delayed_.begin() + importedDelayed), ie(delayed_.end()); it < ie; ++it) {
            auto &atom = operator[](*it);
            if (repr.match(atom)) {
                ret = true;
                f(*it);
            }
        }
        choiceIndex_ = 0;
        importedDelayed = static_cast<SizeType>(delayed_.size());
        return ret;
    }

    // Return true if there is an atom that is not a fact in the domain.
    bool hasChoice() const {
        for (auto it = atoms_.begin() + choiceIndex_, ie = atoms_.end(); it != ie; ++it, ++choiceIndex_) {
            if (!it->fact() && it->defined()) {
                return true;
            }
        }
        return false;
    }

    bool empty() const {
        return atoms_.empty();
    }

    void clear() {
        atoms_.clear();
        indices_.clear();
        fullIndices_.clear();
        generation_ = 0;
    }
    void reset() {
        indices_.clear();
        fullIndices_.clear();
    }

    std::vector<Atom> &container() {
        return const_cast<AtomVec&>(atoms_.values_container());
    }
    std::vector<Atom> const &container() const {
        return atoms_.values_container();
    }


    // Returns the current generation.
    // The generation corresponds to the number of grounding iterations
    // the domain was involved in.
    SizeType generation() const { return generation_; }
    // Resevers an atom for a recursive negative literal.
    // This does not set a generation.
    Iterator reserve(Symbol x) {
        return convert_(atoms_.insert(x).first);
    }
    // Defines (adds) an atom setting its generation.
    std::pair<Iterator, bool> define(Symbol value) {
        auto ret = atoms_.insert(value);
        auto offset = numeric_cast<typename OffsetVec::value_type>(ret.first - atoms_.begin());
        auto it = begin() + offset;
        if (ret.second) {
            it->setGeneration(generation() + 1);
        }
        else if (!ret.first->defined()) {
            ret.second = true;
            it->setGeneration(generation() + 1);
            if (ret.first->delayed()) {
                delayed_.emplace_back(offset);
            }
        }
        return {it, ret.second};
    }
    void define(SizeType offset) {
        auto &atm = operator[](offset);
        if (!atm.defined()) {
            atm.setGeneration(generation() + 1);
            if (atm.delayed()) {
                delayed_.emplace_back(offset);
            }
        }
    }
    // Sets the generation of the domain and all atoms back to zero.
    void init() override {
        generation_ = 0;
        for (auto it = begin() + initOffset_, ie = end(); it != ie; ++it) {
            if (it->defined()) { it->setGeneration(0); }
            else               { it->markDelayed(); }
        }
        initOffset_ = atoms_.size();
        for (auto it = delayed_.begin() + initDelayedOffset_, ie = delayed_.end(); it != ie; ++it) {
            operator[](*it).setGeneration(0);
        }
        initDelayedOffset_ = static_cast<Id_t>(delayed_.size());
    }
    // A domain is enqueued for two grounding iterations.
    // This gives atoms a chance to go from state Open -> New -> Old.
    void enqueue() override { enqueued_ = 2; }
    bool dequeue() override {
        --enqueued_;
        assert(0 <= enqueued_ && enqueued_ <= 2);
        return enqueued_ > 0;
    }
    bool isEnqueued() const override { return enqueued_ > 0; }
    void nextGeneration() override { ++generation_; }
    OffsetVec &delayed() { return delayed_; }
    Iterator find(Symbol x) {
        return convert_(atoms_.find(x));
    }
    ConstIterator find(Symbol x) const { return atoms_.find(x); }
    Id_t size() const { return atoms_.size(); }
    Iterator begin() {
        return container().begin();
    }
    Iterator end() {
        return container().end();
    }
    ConstIterator begin() const {
        return container().begin();
    }
    ConstIterator end() const {
        return container().end();
    }
    Atom &operator[](Id_t x) {
        return const_cast<Atom &>(*atoms_.nth(x));
    }
    void setDomainOffset(Id_t offset) override {
        domainOffset_ = offset;
    }
    Id_t domainOffset() const override {
        return domainOffset_;
    }

protected:
    // Assumes that cleanup sets the generation back to 1 and removes delayed
    // atoms. The given function is simply passed as argument to the erase
    // function of the vector holding the domain elements.
    template <class F>
    void cleanup_(F f) {
        reset();
        Id_t offset = 0;
        Id_t revOffset = atoms_.size();
        Id_t oldOffset = 0;
        for (auto it = atoms_.begin(); it != atoms_.end();) {
            assert(it - atoms_.begin() == offset);
            if (f(const_cast<Atom&>(*it), oldOffset, offset)) {
                it = atoms_.unordered_erase(it);
                --revOffset;
                oldOffset = revOffset;
            }
            else {
                ++it;
                ++offset;
                oldOffset = offset;
            }
        }
        delayed_.clear();
        generation_ = 1;
        initOffset_ = atoms_.size();
        initDelayedOffset_ = 0;
    }

private:
    ConstIterator convert_(typename Atoms::const_iterator it) {
        return begin() + (it - atoms_.begin());
    }
    Iterator convert_(typename Atoms::iterator it) {
        return begin() + (it - atoms_.begin());
    }

    BindIndices indices_;
    FullIndices fullIndices_;
    Atoms       atoms_;
    OffsetVec   delayed_;
    Id_t        enqueued_ = 0;
    Id_t        generation_ = 0;
    Id_t        initOffset_ = 0;
    Id_t        initDelayedOffset_ = 0;
    Id_t        domainOffset_ = InvalidId;
    Id_t mutable choiceIndex_ = 0;
};

// }}}

} // namespace Gringo

#endif // GRINGO_DOMAIN_HH
