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

#ifndef GRINGO_HASH_SET_HH
#define GRINGO_HASH_SET_HH

#include <potassco/basic_types.h>
#include <iterator>
#include <memory>
#include <cassert>
#include <algorithm>
#include <stdexcept>
#include <array>
#include <gringo/utility.hh>
#include <tl/optional.hpp>
#include <tsl/ordered_set.h>
#include <tsl/ordered_map.h>
#include <unordered_set>
#if CLINGO_MAP_TYPE == 0
#   include <tsl/hopscotch_set.h>
#   include <tsl/hopscotch_map.h>
#elif CLINGO_MAP_TYPE == 1
#   include <tsl/sparse_set.h>
#   include <tsl/sparse_map.h>
#endif

namespace Gringo {

template <class Key,
          class Hash = mix_hash<Key>,
          class KeyEqual = std::equal_to<>,
          class Allocator = std::allocator<Key>,
          class ValueTypeContainer = std::vector<Key, Allocator>,
          class IndexType = std::uint_least32_t>
using ordered_set = tsl::ordered_set<Key, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;

template <class Key,
          class Value,
          class Hash = mix_hash<Key>,
          class KeyEqual = std::equal_to<>,
          class Allocator = std::allocator<std::pair<Key, Value>>,
          class ValueTypeContainer = std::vector<std::pair<Key, Value>, Allocator>,
          class IndexType = std::uint_least32_t>
using ordered_map = tsl::ordered_map<Key, Value, Hash, KeyEqual, Allocator, ValueTypeContainer, IndexType>;

#if CLINGO_MAP_TYPE == 0

template <class Key, class Hash = mix_hash<Key>,
          class KeyEqual = std::equal_to<>,
          class Allocator = std::allocator<Key>,
          unsigned int NeighborhoodSize = 62, bool StoreHash = false>
using hash_set = tsl::hopscotch_set<Key, Hash, KeyEqual, Allocator, NeighborhoodSize, StoreHash>;

template <class Key,
          class Value,
          class Hash = mix_hash<Key>,
          class KeyEqual = std::equal_to<>,
          class Allocator = std::allocator<std::pair<Key, Value>>,
          unsigned int NeighborhoodSize = 62,
          bool StoreHash = false>
using hash_map = tsl::hopscotch_map<Key, Value, Hash, KeyEqual, Allocator, NeighborhoodSize, StoreHash>;

#elif CLINGO_MAP_TYPE == 1

template <class Key, class Hash = mix_hash<Key>,
          class KeyEqual = std::equal_to<>,
          class Allocator = std::allocator<Key>>
using hash_set = tsl::sparse_set<Key, Hash, KeyEqual, Allocator>;

template <class Key,
          class Value,
          class Hash = mix_hash<Key>,
          class KeyEqual = std::equal_to<>,
          class Allocator = std::allocator<std::pair<Key, Value>>>
using hash_map = tsl::sparse_map<Key, Value, Hash, KeyEqual, Allocator>;

#endif

struct CallHash {
    template <typename T>
    size_t operator()(T const &x) const {
        return x.hash();
    }
};

struct EqualTo {
    template <typename T, typename U>
    size_t operator()(T const &a, U const &b) const {
        return a == b;
    }
};

template <typename T>
struct Cast {
    template <typename U>
    T const &operator()(U const &x) const {
        return static_cast<T const &>(x);
    }
};

template <typename T>
struct First {
    template <typename U>
    T const &operator()(U const &x) const {
        return std::get<0>(x);
    }
};

template <typename T, typename Get=Cast<T>, typename Hash=std::hash<T>>
struct HashKey : private Get, private Hash {
    // Note: to support lookup with key
    size_t operator()(T const &x) const {
        return Hash::operator()(x);
    }
    template <class U>
    size_t operator()(U const &x) const {
        return Hash::operator()(Get::operator()(x));
    }
};

template <typename T, typename Get=Cast<T>, typename EqualTo=std::equal_to<T>>
struct EqualToKey : private Get, private EqualTo {
    using is_transparent = void;
    // Note: to support lookup with key
    template <typename U>
    bool operator()(T const &a, U const &b) const {
        return EqualTo::operator()(a, Get::operator()(b));
    }
    template <typename U>
    bool operator()(U const &a, T const &b) const {
        return EqualTo::operator()(Get::operator()(a), b);
    }
    template <typename U>
    bool operator()(U const &a, U const &b) const {
        return EqualTo::operator()(Get::operator()(a), Get::operator()(b));
    }
};

template <typename T, typename Hash=std::hash<T>>
using HashFirst = HashKey<T,First<T>,Hash>;

template <typename T, typename EqualTo=std::equal_to<T>>
using EqualToFirst = EqualToKey<T,First<T>,EqualTo>;

template <typename C>
Potassco::Span<typename C::value_type> make_span(C const &container) {
    return {container.data(), container.size()};
}

template <typename Key, typename Hash=std::hash<Key>, typename KeyEqual=std::equal_to<Key>>
class array_set {
public:
    using key_type = typename Potassco::Span<Key>;
    using value_type = typename Potassco::Span<Key>;
    using index_type = std::pair<uint32_t, uint32_t>;
    using size_type = uint32_t;
    using difference_type = int32_t;
    using hasher = Hash;
    using key_equal = KeyEqual;

    std::pair<index_type, bool> insert(key_type const &key) {
        if (key.size > 0) {
            return impl_[key.size].insert(key);
        }
        // the special case that the key is empty is handled by inserting an
        // empty Impl object
        auto it = impl_.find(0);
        if (it == impl_.end()) {
            impl_.emplace(0, Impl{});
            return {{0, 0}, true};
        }
        return {{0, 0}, false};
    }
    tl::optional<index_type> find(key_type const &key) const {
        auto it = impl_.find(key.size);
        if (it == impl_.end()) {
            return tl::nullopt;
        }
        if (key.size > 0) {
            return impl_->find(key);
        }
        // if the key is empty and there is an Impl object, then it has already
        // been inserted
        return index_type{0, 0};
    }
    key_type at(index_type const &idx) const {
        if (idx.second > 0) {
            return impl_.find(idx.second)->second.at(idx);
        }
        return key_type{nullptr, 0};
    }
    void clear() {
        impl_.clear();
    }
    bool empty() const {
        return impl_.empty();
    }
private:
    struct Impl : private Hash, private KeyEqual {
        struct Hasher {
            size_t operator()(index_type const &idx) const {
                return impl->hash_(impl->at(idx));
            }
            friend void swap(Hasher &a, Hasher &b) {
                std::swap(a.impl, b.impl);
            }
            Impl *impl;
        };
        struct EqualTo {
            using is_transparent = void;
            bool operator()(index_type const &a, index_type const &b) const {
                auto span_a = impl->at(a);
                auto span_b = impl->at(b);
                return std::equal(begin(span_a), end(span_a), begin(span_b));
            }
            bool operator()(index_type const &a, key_type const &b) const {
                auto span_a = impl->at(a);
                return std::equal(begin(span_a), end(span_a), begin(b));
            }
            bool operator()(key_type const &a, index_type const &b) const {
                return operator()(b, a);
            }
            friend void swap(EqualTo &a, EqualTo &b) {
                std::swap(a.impl, b.impl);
            }
            Impl *impl;
        };

        Impl()
        : data{0, Hasher{this}, EqualTo{this}} { }

        size_t hash_(key_type const &key) {
            return hash_mix(hash_range(begin(key), end(key), static_cast<Hash&>(*this)));
        }

        std::pair<index_type, bool> insert(key_type const &key) {
            size_t index = values.size();
            values.insert(values.end(), begin(key), end(key));
            auto ret = data.insert({numeric_cast<uint32_t>(index / key.size),
                                    numeric_cast<uint32_t>(key.size)});
            if (!ret.second) {
                values.resize(index);
            }
            return {*ret.first, ret.second};
        }

        tl::optional<index_type> find(key_type const &key) const {
            auto it = data.find(key, hash_(key));
            return it == data.end() ? tl::nullopt : tl::make_optional(it->index);
        }

        key_type at(index_type const &idx) const {
            return {values.data() + static_cast<size_t>(idx.first) * idx.second, idx.second};
        }

        std::vector<Key> values;
        hash_set<index_type, Hasher, EqualTo> data;
    };

    std::unordered_map<size_t, Impl> impl_;
};

} // namespace Gringo

#endif // GRINGO_HASH_SET_HH
